#include "gpu/blocks/shader_array/shader_unit.h"
#include "gpu/definitions/register_allocator.h"
#include "gpu/util/conversions.h"
#include "gpu/util/math.h"
#include "gpu/util/os_interface.h"
#include "gpu/util/transfer.h"

void ShaderUnit::main() {
    bool handshakeAlreadyEstablished = false;

    while (true) {
        // Read ISA header
        Isa::Command::Command command = {};
        Transfer::receiveArray(request.inpSending, request.inpData, request.outReceiving, command.dummy.raw, Isa::commandSizeInDwords, &profiling.outBusy, !handshakeAlreadyEstablished);
        handshakeAlreadyEstablished = command.dummy.hasNextCommand;

        switch (command.dummy.commandType) {
        case Isa::Command::CommandType::ExecuteIsa:
            processExecuteIsaCommand(reinterpret_cast<Isa::Command::CommandExecuteIsa &>(command));
            break;
        case Isa::Command::CommandType::StoreIsa:
            processStoreIsaCommand(reinterpret_cast<Isa::Command::CommandStoreIsa &>(command));
            break;
        default:
            FATAL_ERROR("Unknown command type");
        }
    }
}

void ShaderUnit::processStoreIsaCommand(Isa::Command::CommandStoreIsa command) {
    const uint32_t isaSize = command.programLength;
    this->isaMetadata = command;
    for (int i = 0; i < isaSize; i++) {
        wait();
        this->isa[i] = request.inpData.read();
    }
}

void ShaderUnit::processExecuteIsaCommand(Isa::Command::CommandExecuteIsa command) {
    // Clear all registers to 0
    static_assert(std::is_pod_v<Registers>);
    memset(&registers, 0, sizeof(registers));

    // Stream-in values for input registers
    const auto threadCount = nonZeroCountToInt(command.threadCount);
    initializeInputRegisters(threadCount);

    // Execute isa
    profiling.outThreadsStarted = profiling.outThreadsStarted.read() + threadCount;
    executeInstructions(isaMetadata.programLength, threadCount);
    profiling.outThreadsFinished = profiling.outThreadsFinished.read() + threadCount;

    // Stream-out values from output registers
    uint32_t outputStream[Isa::registerComponentsCount * Isa::maxInputOutputRegisters * Isa::simdSize];
    uint32_t outputStreamSize = {};
    appendOutputRegistersValues(threadCount, outputStream, outputStreamSize);

    Transfer::sendArray(response.inpReceiving, response.outSending, response.outData, outputStream, outputStreamSize);
}

void ShaderUnit::initializeInputRegisters(uint32_t threadCount) {
    const uint32_t inputsCount = nonZeroCountToInt(isaMetadata.inputsCount);

    if (isaMetadata.programType == Isa::Command::ProgramType::VertexShader) {
        // Get info about components
        Isa::RegisterSelection registerIndices[4] = {};
        uint32_t componentsCounts[4] = {};
        for (int inputIndex = 0; inputIndex < inputsCount; inputIndex++) {
            registerIndices[inputIndex] = getInputOutputRegisterIndex(true, inputIndex);
            componentsCounts[inputIndex] = nonZeroCountToInt(getInputOutputSize(true, inputIndex));
        }

        // Read per thread inputs and store in registers
        for (int threadIndex = 0; threadIndex < threadCount; threadIndex++) {
            for (int inputIndex = 0; inputIndex < inputsCount; inputIndex++) {
                const auto componentsCount = componentsCounts[inputIndex];
                const auto registerIndex = registerIndices[inputIndex];

                VectorRegister &reg = registers.gpr[threadIndex][registerIndex];
                for (int component = 0; component < componentsCount; component++) {
                    wait();
                    reg[component] = request.inpData.read();
                }
            }
        }
    } else if (isaMetadata.programType == Isa::Command::ProgramType::FragmentShader) {
        // Fragment shaders are handled differently. First, per thread x,y position coordinates
        // are stored to the first input register. Then, per request triangle attributes are
        // stored to some other registers, NOT inputs defined in the shader code. They will be
        // interpolated and results will be stored to actual input registers later.

        // Get info about components
        Isa::RegisterSelection registerIndices[Isa::maxInputOutputRegisters] = {}; // registers defined by the user. We will only populate the first one with xy position.
        size_t componentsCounts[Isa::maxInputOutputRegisters] = {};                // component counts of per-request triangle attributes
        for (int inputIndex = 0; inputIndex < inputsCount; inputIndex++) {
            registerIndices[inputIndex] = getInputOutputRegisterIndex(true, inputIndex);
            if (inputIndex == 0) {
                componentsCounts[inputIndex] = 3; // we always pass x,y,z position per request. It is hardcoded in FS and SF blocks.
            } else {
                componentsCounts[inputIndex] = nonZeroCountToInt(getInputOutputSize(true, inputIndex));
            }
        }

        // Receive per thread x,y coordinates and write them to registers
        for (int threadIndex = 0; threadIndex < threadCount; threadIndex++) {
            wait();
            registers.gpr[threadIndex][registerIndices[0]].x = request.inpData.read();
            wait();
            registers.gpr[threadIndex][registerIndices[0]].y = request.inpData.read();
        }

        // Receive per request attributes and store them in temporary array
        RegisterAllocator registerAllocator{isaMetadata};
        struct PerRequestReg {
            Isa::RegisterSelection index;
            VectorRegister value;
        };
        PerRequestReg perRequestInputs[verticesInPrimitive][Isa::maxInputOutputRegisters] = {};
        for (int vertexIndex = 0; vertexIndex < verticesInPrimitive; vertexIndex++) {
            for (int inputIndex = 0; inputIndex < inputsCount; inputIndex++) {
                PerRequestReg &reg = perRequestInputs[vertexIndex][inputIndex];

                // Determine where to put the per-request attribute value. It doesn't matter to the shade code,
                // but it has to be in sync with the interpolation code generated by the compiler, which will
                // expect the values to be in particular registers.
                reg.index = registerAllocator.allocate();

                // Receive register value and store it in a temporary array
                const size_t componentsCount = componentsCounts[inputIndex];
                for (size_t componentIndex = 0; componentIndex < componentsCount; componentIndex++) {
                    wait();
                    reg.value[componentIndex] = request.inpData.read();
                }
            }
        }

        // Write per request attributes to registers
        for (int threadIndex = 0; threadIndex < threadCount; threadIndex++) {
            for (int vertexIndex = 0; vertexIndex < verticesInPrimitive; vertexIndex++) {
                for (int inputIndex = 0; inputIndex < inputsCount; inputIndex++) {
                    const PerRequestReg &reg = perRequestInputs[vertexIndex][inputIndex];
                    registers.gpr[threadIndex][reg.index] = reg.value;
                }
            }
        }
    } else {
        FATAL_ERROR("Unknown shader type");
    }
}

void ShaderUnit::appendOutputRegistersValues(uint32_t threadCount, uint32_t *outputStream, uint32_t &outputStreamSize) {
    const uint32_t outputsCount = nonZeroCountToInt(isaMetadata.outputsCount);

    // Get info about components
    Isa::RegisterSelection registerIndices[4] = {};
    uint32_t componentsCounts[4] = {};
    for (int outputIndex = 0; outputIndex < outputsCount; outputIndex++) {
        registerIndices[outputIndex] = getInputOutputRegisterIndex(false, outputIndex);
        componentsCounts[outputIndex] = nonZeroCountToInt(getInputOutputSize(false, outputIndex));
    }

    for (int threadIndex = 0; threadIndex < threadCount; threadIndex++) {
        for (int outputIndex = 0; outputIndex < outputsCount; outputIndex++) {
            const auto registerIndex = registerIndices[outputIndex];
            const auto componentsCount = componentsCounts[outputIndex];

            const VectorRegister &reg = registers.gpr[threadIndex][registerIndex];
            for (int component = 0; component < componentsCount; component++) {
                outputStream[outputStreamSize++] = reg[component];
            }
        }
    }
}

NonZeroCount ShaderUnit::getInputOutputSize(bool input, uint32_t index) const {
    switch (index) {
    case 0:
        return input ? isaMetadata.inputSize0 : isaMetadata.outputSize0;
    case 1:
        return input ? isaMetadata.inputSize1 : isaMetadata.outputSize1;
    case 2:
        return input ? isaMetadata.inputSize2 : isaMetadata.outputSize2;
    case 3:
        return input ? isaMetadata.inputSize3 : isaMetadata.outputSize3;
    default:
        FATAL_ERROR("Invalid index for ", __FUNCTION__);
    }
}

Isa::RegisterSelection ShaderUnit::getInputOutputRegisterIndex(bool input, uint32_t index) const {
    switch (index) {
    case 0:
        return input ? isaMetadata.inputRegister0 : isaMetadata.outputRegister0;
    case 1:
        return input ? isaMetadata.inputRegister1 : isaMetadata.outputRegister1;
    case 2:
        return input ? isaMetadata.inputRegister2 : isaMetadata.outputRegister2;
    case 3:
        return input ? isaMetadata.inputRegister3 : isaMetadata.outputRegister3;
    default:
        FATAL_ERROR("Invalid index for ", __FUNCTION__);
    }
}

void ShaderUnit::executeInstructions(uint32_t isaSize, uint32_t threadCount) {
    while (registers.pc < isaSize) {
        const Isa::Instruction &instruction = *reinterpret_cast<const Isa::Instruction *>(isa + registers.pc);

        const static auto asf = [](int32_t arg) { return reinterpret_cast<float &>(arg); };
        const static auto asi = [](float arg) { return reinterpret_cast<int32_t &>(arg); };

        switch (instruction.header.opcode) {
        case Isa::Opcode::fadd:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMath, [](int32_t src1, int32_t src2) { return asi(asf(src1) + asf(src2)); });
            break;
        case Isa::Opcode::fadd_imm:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMathImm, [](int32_t src1, int32_t src2) { return asi(asf(src1) + asf(src2)); });
            break;
        case Isa::Opcode::fsub:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMath, [](int32_t src1, int32_t src2) { return asi(asf(src1) - asf(src2)); });
            break;
        case Isa::Opcode::fsub_imm:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMathImm, [](int32_t src1, int32_t src2) { return asi(asf(src1) - asf(src2)); });
            break;
        case Isa::Opcode::fmul:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMath, [](int32_t src1, int32_t src2) { return asi(asf(src1) * asf(src2)); });
            break;
        case Isa::Opcode::fmul_imm:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMathImm, [](int32_t src1, int32_t src2) { return asi(asf(src1) * asf(src2)); });
            break;
        case Isa::Opcode::fdiv:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMath, [](int32_t src1, int32_t src2) { return asi(asf(src1) / asf(src2)); });
            break;
        case Isa::Opcode::fdiv_imm:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMathImm, [](int32_t src1, int32_t src2) { return asi(asf(src1) / asf(src2)); });
            break;
        case Isa::Opcode::fneg:
            registers.pc += executeInstructionForLanes(threadCount, instruction.unaryMath, [](int32_t src) { return asi(-asf(src)); });
            break;
        case Isa::Opcode::fdot:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMath, [](VectorRegister src1, VectorRegister src2) {
                return asi(asf(src1.x) * asf(src2.x) +
                           asf(src1.y) * asf(src2.y) +
                           asf(src1.z) * asf(src2.z) +
                           asf(src1.w) * asf(src2.w));
            });
            break;
        case Isa::Opcode::fcross:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMath, [](VectorRegister src1, VectorRegister src2) {
                return VectorRegister{
                    asi(asf(src1.y) * asf(src2.z) - asf(src1.z) * asf(src2.y)),
                    asi(asf(src1.z) * asf(src2.x) - asf(src1.x) * asf(src2.z)),
                    asi(asf(src1.x) * asf(src2.y) - asf(src1.y) * asf(src2.x)),
                    0,
                };
            });
            break;
        case Isa::Opcode::fcross2:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMath, [](VectorRegister src1, VectorRegister src2) {
                return asi(asf(src1.x) * asf(src2.y) - asf(src1.y) * asf(src2.x));
            });
            break;
        case Isa::Opcode::fmad:
            registers.pc += executeInstructionForLanes(threadCount, instruction.ternaryMath, [](int32_t src1, int32_t src2, int32_t src3) {
                return asi(asf(src1) * asf(src2) + asf(src3));
            });
            break;
        case Isa::Opcode::frcp:
            registers.pc += executeInstructionForLanes(threadCount, instruction.unaryMath, [](int32_t src) { return asi(1 / asf(src)); });
            break;

        case Isa::Opcode::iadd:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMath, [](int32_t src1, int32_t src2) { return src1 + src2; });
            break;
        case Isa::Opcode::iadd_imm:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMathImm, [](int32_t src1, int32_t src2) { return src1 + src2; });
            break;
        case Isa::Opcode::isub:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMath, [](int32_t src1, int32_t src2) { return src1 - src2; });
            break;
        case Isa::Opcode::isub_imm:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMathImm, [](int32_t src1, int32_t src2) { return src1 - src2; });
            break;
        case Isa::Opcode::imul:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMath, [](int32_t src1, int32_t src2) { return src1 * src2; });
            break;
        case Isa::Opcode::imul_imm:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMathImm, [](int32_t src1, int32_t src2) { return src1 * src2; });
            break;
        case Isa::Opcode::idiv:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMath, [](int32_t src1, int32_t src2) { return src1 / src2; });
            break;
        case Isa::Opcode::idiv_imm:
            registers.pc += executeInstructionForLanes(threadCount, instruction.binaryMathImm, [](int32_t src1, int32_t src2) { return src1 / src2; });
            break;
        case Isa::Opcode::ineg:
            registers.pc += executeInstructionForLanes(threadCount, instruction.unaryMath, [](int32_t src) { return -src; });
            break;

        case Isa::Opcode::init:
            registers.pc += executeInstructionForLanes(threadCount, instruction.unaryMathImm, [](int32_t src) { return src; });
            break;
        case Isa::Opcode::mov:
            registers.pc += executeInstructionForLanes(threadCount, instruction.unaryMath, [](int32_t src) { return src; });
            break;
        case Isa::Opcode::swizzle:
            registers.pc += executeInstructionForLanes(threadCount, instruction.swizzle);
            break;
        case Isa::Opcode::trap:
            if (OsInterface::isDebuggerAttached()) {
                OsInterface::breakpoint();
            }
            registers.pc += sizeof(instruction.nullary) / sizeof(uint32_t);
            break;

        default:
            FATAL_ERROR("Unknown opcode: ", (uint32_t)instruction.header.opcode);
        }

        wait();
    }
}

int32_t ShaderUnit::executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::UnaryMath &inst, UnaryFunction function) {
    VectorRegister &src = registers.gpr[lane][inst.src];
    VectorRegister &dest = registers.gpr[lane][inst.dest];
    for (int i = 0; i < 4; i++) {
        if (isBitSet(inst.destMask, 3 - i)) {
            dest[i] = function(src[i]);
        }
    }
    return sizeof(inst) / sizeof(uint32_t);
}

int32_t ShaderUnit::executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::BinaryMath &inst, BinaryFunction function) {
    VectorRegister &src1 = registers.gpr[lane][inst.src1];
    VectorRegister &src2 = registers.gpr[lane][inst.src2];
    VectorRegister &dest = registers.gpr[lane][inst.dest];
    for (int i = 0; i < 4; i++) {
        if (isBitSet(inst.destMask, 3 - i)) {
            dest[i] = function(src1[i], src2[i]);
        }
    }
    return sizeof(inst) / sizeof(uint32_t);
}

int32_t ShaderUnit::executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::TernaryMath &inst, TernaryFunction function) {
    VectorRegister &src1 = registers.gpr[lane][inst.src1];
    VectorRegister &src2 = registers.gpr[lane][inst.src2];
    VectorRegister &src3 = registers.gpr[lane][inst.src3];
    VectorRegister &dest = registers.gpr[lane][inst.dest];
    for (int i = 0; i < 4; i++) {
        if (isBitSet(inst.destMask, 3 - i)) {
            dest[i] = function(src1[i], src2[i], src3[i]);
        }
    }
    return sizeof(inst) / sizeof(uint32_t);
}

int32_t ShaderUnit::executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::BinaryMath &inst, BinaryVectorScalarFunction function) {
    VectorRegister &src1 = registers.gpr[lane][inst.src1];
    VectorRegister &src2 = registers.gpr[lane][inst.src2];
    VectorRegister &dest = registers.gpr[lane][inst.dest];

    const uint32_t result = function(src1, src2);
    for (int i = 0; i < 4; i++) {
        if (isBitSet(inst.destMask, 3 - i)) {
            dest[i] = result; // store the same result in each component from mask
        }
    }

    return sizeof(inst) / sizeof(uint32_t);
}
int32_t ShaderUnit::executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::BinaryMath &inst, BinaryVectorVectorFunction function) {
    VectorRegister &src1 = registers.gpr[lane][inst.src1];
    VectorRegister &src2 = registers.gpr[lane][inst.src2];
    VectorRegister &dest = registers.gpr[lane][inst.dest];

    const VectorRegister result = function(src1, src2);
    for (int i = 0; i < 4; i++) {
        dest = result; // ignore destination mask
    }

    return sizeof(inst) / sizeof(uint32_t);
}

int32_t ShaderUnit::executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::UnaryMathImm &inst, UnaryFunction function) {
    const size_t immCount = nonZeroCountToInt(inst.immediateValuesCount);

    VectorRegister &dest = registers.gpr[lane][inst.dest];
    size_t immediateValueIndex = 0;
    for (int i = 0; i < 4; i++) {
        if (isBitSet(inst.destMask, 3 - i)) {
            int32_t src2 = reinterpret_cast<const int32_t &>(inst.immediateValues[immediateValueIndex]);
            dest[i] = function(src2);

            if (immediateValueIndex + 1 < immCount) {
                immediateValueIndex++;
            }
        }
    }
    return 1 + immCount;
}

int32_t ShaderUnit::executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::BinaryMathImm &inst, BinaryFunction function) {
    const size_t immCount = nonZeroCountToInt(inst.immediateValuesCount);

    VectorRegister &src1 = registers.gpr[lane][inst.src];
    VectorRegister &dest = registers.gpr[lane][inst.dest];
    size_t immediateValueIndex = 0;
    for (int i = 0; i < 4; i++) {
        if (isBitSet(inst.destMask, 3 - i)) {
            int32_t src2 = reinterpret_cast<const int32_t &>(inst.immediateValues[immediateValueIndex]);
            dest[i] = function(src1[i], src2);

            if (immediateValueIndex + 1 < immCount) {
                immediateValueIndex++;
            }
        }
    }
    return 1 + immCount;
}

int32_t ShaderUnit::executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::Swizzle &inst) {
    VectorRegister &src = registers.gpr[lane][inst.src];
    VectorRegister &dest = registers.gpr[lane][inst.dest];

    dest.x = src[static_cast<size_t>(inst.patternX)];
    dest.y = src[static_cast<size_t>(inst.patternY)];
    dest.z = src[static_cast<size_t>(inst.patternZ)];
    dest.w = src[static_cast<size_t>(inst.patternW)];
    return sizeof(inst) / sizeof(uint32_t);
}
