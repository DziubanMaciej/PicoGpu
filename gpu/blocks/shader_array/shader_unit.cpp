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
            command.dummy.raw = Handshake::receive(request.inpSending, request.inpData, request.outReceiving).to_int();
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
    initializeInputRegisters(command.threadCount);

    // Execute isa
    // TODO move this loop into function
    for (const uint32_t isaSize = isaMetadata.programLength; registers.pc < isaSize;) {
        executeInstruction(command.threadCount);
    }

    // Stream-out values from output registers
    uint32_t outputStream[4 * Isa::outputRegistersCount];
    uint32_t outputStreamSize = {};
    appendOutputRegistersValues(command.threadCount, outputStream, outputStreamSize);

    Handshake::sendArray(response.inpReceiving, response.outSending, response.outData, outputStream, outputStreamSize);
}

void ShaderUnit::initializeInputRegisters(uint32_t threadCount) {
    const uint32_t inputsCount = Isa::Command::nonZeroCountToInt(isaMetadata.inputsCount);

    uint32_t componentsCounts[4] = {};
    for (int regIndex = 0; regIndex < inputsCount; regIndex++) {
        Isa::Command::NonZeroCount componentsCountField = {};
        switch (regIndex) {
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
        componentsCounts[regIndex] = Isa::Command::nonZeroCountToInt(componentsCountField);
    }

    for (int threadIndex = 0; threadIndex < threadCount; threadIndex++) {
        for (int regIndex = 0; regIndex < inputsCount; regIndex++) {
            VectorRegister &reg = selectRegister(Isa::RegisterSelection::i0 + regIndex, threadIndex);
            uint32_t componentsCount = componentsCounts[regIndex];
            for (int component = 0; component < componentsCount; component++) {
                wait();
                reg[component] = request.inpData.read();
            }
        }
    }
}

void ShaderUnit::appendOutputRegistersValues(uint32_t threadCount, uint32_t *outputStream, uint32_t &outputStreamSize) {
    const uint32_t outputsCount = Isa::Command::nonZeroCountToInt(isaMetadata.outputsCount);

    uint32_t componentsCounts[4] = {};
    for (int regIndex = 0; regIndex < outputsCount; regIndex++) {
        Isa::Command::NonZeroCount componentsCountField = {};
        switch (regIndex) {
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
        componentsCounts[regIndex] = Isa::Command::nonZeroCountToInt(componentsCountField);
    }

    for (int threadIndex = 0; threadIndex < threadCount; threadIndex++) {
        for (int regIndex = 0; regIndex < outputsCount; regIndex++) {
            const VectorRegister &reg = selectRegister(Isa::RegisterSelection::o0 + regIndex, threadIndex);
            uint32_t componentsCount = componentsCounts[regIndex];
            for (int component = 0; component < componentsCount; component++) {
                outputStream[outputStreamSize++] = reg[component];
            }
        }
    }
}

VectorRegister &ShaderUnit::selectRegister(Isa::RegisterSelection selection, uint32_t lane) {
    // clang-format off
    switch (selection) {
    case Isa::RegisterSelection::i0: return registers.lanes[lane].i0;
    case Isa::RegisterSelection::i1: return registers.lanes[lane].i1;
    case Isa::RegisterSelection::i2: return registers.lanes[lane].i2;
    case Isa::RegisterSelection::i3: return registers.lanes[lane].i3;
    case Isa::RegisterSelection::o0: return registers.lanes[lane].o0;
    case Isa::RegisterSelection::o1: return registers.lanes[lane].o1;
    case Isa::RegisterSelection::o2: return registers.lanes[lane].o2;
    case Isa::RegisterSelection::o3: return registers.lanes[lane].o3;
    case Isa::RegisterSelection::r0: return registers.lanes[lane].r0;
    case Isa::RegisterSelection::r1: return registers.lanes[lane].r1;
    case Isa::RegisterSelection::r2: return registers.lanes[lane].r2;
    case Isa::RegisterSelection::r3: return registers.lanes[lane].r3;
    case Isa::RegisterSelection::r4: return registers.lanes[lane].r4;
    case Isa::RegisterSelection::r5: return registers.lanes[lane].r5;
    case Isa::RegisterSelection::r6: return registers.lanes[lane].r6;
    case Isa::RegisterSelection::r7: return registers.lanes[lane].r7;
    default:
        FATAL_ERROR("Unknown register selection");
    }
    // clang-format on
}

void ShaderUnit::executeInstruction(uint32_t threadCount) {
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

int32_t ShaderUnit::executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::UnaryMath &inst, UnaryFunction function) {
    VectorRegister &src = selectRegister(inst.src, lane);
    VectorRegister &dest = selectRegister(inst.dest, lane);
    for (int i = 0; i < 4; i++) {
        if (isBitSet(inst.destMask, 3 - i)) {
            dest[i] = function(src[i]);
        }
    }
    return sizeof(inst) / sizeof(uint32_t);
}

int32_t ShaderUnit::executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::BinaryMath &inst, BinaryFunction function) {
    VectorRegister &src1 = selectRegister(inst.src1, lane);
    VectorRegister &src2 = selectRegister(inst.src2, lane);
    VectorRegister &dest = selectRegister(inst.dest, lane);
    for (int i = 0; i < 4; i++) {
        if (isBitSet(inst.destMask, 3 - i)) {
            dest[i] = function(src1[i], src2[i]);
        }
    }
    return sizeof(inst) / sizeof(uint32_t);
}

int32_t ShaderUnit::executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::UnaryMathImm &inst, UnaryFunction function) {
    const size_t immCount = Isa::Command::nonZeroCountToInt(inst.immediateValuesCount);

    VectorRegister &dest = selectRegister(inst.dest, lane);
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
    const size_t immCount = Isa::Command::nonZeroCountToInt(inst.immediateValuesCount);

    VectorRegister &src1 = selectRegister(inst.src, lane);
    VectorRegister &dest = selectRegister(inst.dest, lane);
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
    VectorRegister &src = selectRegister(inst.src, lane);
    VectorRegister &dest = selectRegister(inst.dest, lane);

    dest.x = src[static_cast<size_t>(inst.patternX)];
    dest.y = src[static_cast<size_t>(inst.patternY)];
    dest.z = src[static_cast<size_t>(inst.patternZ)];
    dest.w = src[static_cast<size_t>(inst.patternW)];
    return sizeof(inst) / sizeof(uint32_t);
}
