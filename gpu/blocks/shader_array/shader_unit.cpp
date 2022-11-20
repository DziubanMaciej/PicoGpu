#include "gpu/blocks/shader_array/shader_unit.h"
#include "gpu/util/conversions.h"
#include "gpu/util/handshake.h"
#include "gpu/util/math.h"

void ShaderUnit::main() {
    bool handshakeAlreadyEstablished = false;

    while (true) {
        // Read ISA header
        Isa::Command::Command command = {};
        if (handshakeAlreadyEstablished) {
            wait();
            command.dummy.raw = request.inpData.read();
        } else {
            command.dummy.raw = Handshake::receive(request.inpSending, request.inpData, request.outReceiving, &profiling.outBusy).to_int();
        }
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

    Handshake::sendArray(response.inpReceiving, response.outSending, response.outData, outputStream, outputStreamSize);
}

void ShaderUnit::initializeInputRegisters(uint32_t threadCount) {
    const uint32_t inputsCount = nonZeroCountToInt(isaMetadata.inputsCount);
    const uint32_t verticesInTriangle = 3;

    if (isaMetadata.programType == Isa::Command::ProgramType::FragmentShader) {
        // Receive per thread x,y coordinates and write them to registers
        for (int threadIndex = 0; threadIndex < threadCount; threadIndex++) {
            wait();
            registers.gpr[threadIndex][0].x = request.inpData.read();
            wait();
            registers.gpr[threadIndex][0].y = request.inpData.read();
        }

        // Receive per request custom attributes
        VectorRegister perRequestInputs[verticesInTriangle][Isa::maxInputOutputRegisters] = {};
        size_t componentsCounts[Isa::maxInputOutputRegisters] = {};
        for (int inputIndex = 0; inputIndex < inputsCount; inputIndex++) {
            switch (inputIndex) {
            case 0:
                componentsCounts[inputIndex] = 3; // we always pass x,y,z position
                break;
            case 1:
                componentsCounts[inputIndex] = nonZeroCountToInt(isaMetadata.inputSize1);
                break;
            case 2:
                componentsCounts[inputIndex] = nonZeroCountToInt(isaMetadata.inputSize2);
                break;
            case 3:
                componentsCounts[inputIndex] = nonZeroCountToInt(isaMetadata.inputSize3);
                break;
            default:
                FATAL_ERROR("Invalid input reg index");
            }
        }
        for (int vertexIndex = 0; vertexIndex < verticesInTriangle; vertexIndex++) {
            for (int inputIndex = 0; inputIndex < inputsCount; inputIndex++) {
                const size_t componentsCount = componentsCounts[inputIndex];
                for (size_t componentIndex = 0; componentIndex < componentsCount; componentIndex++) {
                    wait();
                    perRequestInputs[vertexIndex][inputIndex][componentIndex] = request.inpData.read();
                }
            }
        }

        // Write per request custom attributes to registers
        constexpr size_t maxCustomAttributes = Isa::maxInputOutputRegisters - 1; // 1 is reserved for position
        const Isa::RegisterSelection basePerConfigRegister = Isa::generalPurposeRegistersCount - maxCustomAttributes * verticesInPrimitive;
        for (int threadIndex = 0; threadIndex < threadCount; threadIndex++) {
            for (int vertexIndex = 0; vertexIndex < verticesInTriangle; vertexIndex++) {
                for (int inputIndex = 0; inputIndex < inputsCount; inputIndex++) {
                    for (size_t componentIndex = 0; componentIndex < Isa::registerComponentsCount; componentIndex++) {
                        const Isa::RegisterSelection reg = basePerConfigRegister + vertexIndex * verticesInTriangle + inputIndex;
                        registers.gpr[threadIndex][reg][componentIndex] = perRequestInputs[vertexIndex][inputIndex][componentIndex];
                    }
                }
            }
        }
    } else {
        // Get how many components of each input has been passed
        uint32_t componentsCounts[4] = {};
        for (int inputIndex = 0; inputIndex < inputsCount; inputIndex++) {
            NonZeroCount componentsCountField = {};
            switch (inputIndex) {
            case 0:
                componentsCountField = isaMetadata.inputSize0;
                break;
            case 1:
                componentsCountField = isaMetadata.inputSize1;
                break;
            case 2:
                componentsCountField = isaMetadata.inputSize2;
                break;
            case 3:
                componentsCountField = isaMetadata.inputSize3;
                break;
            default:
                FATAL_ERROR("Invalid input reg index");
            }
            componentsCounts[inputIndex] = nonZeroCountToInt(componentsCountField);
        }

        // Read per thread inputs and store in registers
        for (int threadIndex = 0; threadIndex < threadCount; threadIndex++) {
            for (int inputIndex = 0; inputIndex < inputsCount; inputIndex++) {
                VectorRegister &reg = registers.gpr[threadIndex][inputIndex + Isa::inputRegistersOffset];
                uint32_t componentsCount = componentsCounts[inputIndex];
                for (int component = 0; component < componentsCount; component++) {
                    wait();
                    reg[component] = request.inpData.read();
                }
            }
        }
    }
}

void ShaderUnit::appendOutputRegistersValues(uint32_t threadCount, uint32_t *outputStream, uint32_t &outputStreamSize) {
    const uint32_t outputsCount = nonZeroCountToInt(isaMetadata.outputsCount);

    uint32_t componentsCounts[4] = {};
    for (int outputIndex = 0; outputIndex < outputsCount; outputIndex++) {
        NonZeroCount componentsCountField = {};
        switch (outputIndex) {
        case 0:
            componentsCountField = isaMetadata.outputSize0;
            break;
        case 1:
            componentsCountField = isaMetadata.outputSize1;
            break;
        case 2:
            componentsCountField = isaMetadata.outputSize2;
            break;
        case 3:
            componentsCountField = isaMetadata.outputSize3;
            break;
        default:
            FATAL_ERROR("Invalid output reg index");
        }
        componentsCounts[outputIndex] = nonZeroCountToInt(componentsCountField);
    }

    for (int threadIndex = 0; threadIndex < threadCount; threadIndex++) {
        for (int outputIndex = 0; outputIndex < outputsCount; outputIndex++) {
            const VectorRegister &reg = registers.gpr[threadIndex][outputIndex + Isa::outputRegistersOffset];
            uint32_t componentsCount = componentsCounts[outputIndex];
            for (int component = 0; component < componentsCount; component++) {
                outputStream[outputStreamSize++] = reg[component];
            }
        }
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
