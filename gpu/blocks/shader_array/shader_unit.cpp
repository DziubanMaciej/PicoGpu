#include "gpu/blocks/shader_array/shader_unit.h"
#include "gpu/util/conversions.h"
#include "gpu/util/handshake.h"
#include "gpu/util/math.h"

void ShaderUnit::main() {
    while (true) {
        // Read ISA header
        Isa::Header header = {};
        header.dword1.raw = Handshake::receive(request.inpSending, request.inpData, request.outReceiving).to_int();
        wait();
        header.dword2.raw = request.inpData.read();

        // Validate values in header
        // TODO

        // Based on the header we know how long the ISA is. Read it from the input stream
        uint32_t isaSize = header.dword1.programLength;
        uint32_t isa[Isa::maxIsaSize] = {};
        for (int i = 0; i < isaSize; i++) {
            wait();
            isa[i] = request.inpData.read();
        }

        // Clear all registers to 0
        static_assert(std::is_pod_v<Registers>);
        memset(&registers, 0, sizeof(registers));

        // Stream-in values for input registers
        initializeInputRegister(registers.i0, header.dword2.inputSize0);
        initializeInputRegister(registers.i1, header.dword2.inputSize1);
        initializeInputRegister(registers.i2, header.dword2.inputSize2);
        initializeInputRegister(registers.i3, header.dword2.inputSize3);

        // Execute isa
        while (registers.pc < isaSize) {
            executeInstruction(isa);
        }

        // Stream-out values from output registers
        uint32_t outputStream[4 * Isa::outputRegistersCount];
        uint32_t outputStreamSize = {};
        appendToOutputStream(registers.o0, header.dword2.outputSize0, outputStream, outputStreamSize);
        appendToOutputStream(registers.o1, header.dword2.outputSize1, outputStream, outputStreamSize);
        appendToOutputStream(registers.o2, header.dword2.outputSize2, outputStream, outputStreamSize);
        appendToOutputStream(registers.o3, header.dword2.outputSize3, outputStream, outputStreamSize);
        sc_uint<32> outputToken = outputStream[0];
        Handshake::send(response.inpReceiving, response.outSending, response.outData, outputToken);
        for (int i = 1; i < outputStreamSize; i++) {
            outputToken = outputStream[i];
            response.outData = outputToken;
            wait();
        }
        response.outData = 0;
    }
}

void ShaderUnit::initializeInputRegister(VectorRegister &reg, uint32_t componentsCount) {
    FATAL_ERROR_IF(componentsCount > 4, "Vectors have only 4 components.");

    for (int component = 0; component < componentsCount; component++) {
        wait();
        // TODO currently read uints from simplicity. Change this to float
        // reg[component] = Conversions::uintBytesToFloat(request.inpData.read());
        reg[component] = request.inpData.read();
    }
}

void ShaderUnit::appendToOutputStream(VectorRegister &reg, uint32_t componentsCount, uint32_t *outputStream, uint32_t &outputStreamSize) {
    FATAL_ERROR_IF(componentsCount > 4, "Vectors have only 4 components.");

    for (int component = 0; component < componentsCount; component++) {
        outputStream[outputStreamSize++] = reg[component];
    }
}

VectorRegister &ShaderUnit::selectRegister(Isa::RegisterSelection selection) {
    // clang-format off
    switch (selection) {
    case Isa::RegisterSelection::i0: return registers.i0;
    case Isa::RegisterSelection::i1: return registers.i1;
    case Isa::RegisterSelection::i2: return registers.i2;
    case Isa::RegisterSelection::i3: return registers.i3;
    case Isa::RegisterSelection::o0: return registers.o0;
    case Isa::RegisterSelection::o1: return registers.o1;
    case Isa::RegisterSelection::o2: return registers.o2;
    case Isa::RegisterSelection::o3: return registers.o3;
    case Isa::RegisterSelection::r0: return registers.r0;
    case Isa::RegisterSelection::r1: return registers.r1;
    case Isa::RegisterSelection::r2: return registers.r2;
    case Isa::RegisterSelection::r3: return registers.r3;
    case Isa::RegisterSelection::r4: return registers.r4;
    case Isa::RegisterSelection::r5: return registers.r5;
    case Isa::RegisterSelection::r6: return registers.r6;
    case Isa::RegisterSelection::r7: return registers.r7;
    default:
        FATAL_ERROR("Unknown register selection");
    }
    // clang-format on
}

void ShaderUnit::executeInstruction(uint32_t *isa) {
    const Isa::Instruction &instruction = *reinterpret_cast<const Isa::Instruction *>(isa + registers.pc);

    size_t programCounterIncrement = 1;
    switch (instruction.header.opcode) {
    case Isa::Opcode::add:
        executeInstruction(instruction.binaryMath, [](uint32_t src1, uint32_t src2) { return src1 + src2; });
        break;
    case Isa::Opcode::mul:
        executeInstruction(instruction.binaryMath, [](uint32_t src1, uint32_t src2) { return src1 * src2; });
        break;
    case Isa::Opcode::mov:
        executeInstruction(instruction.unaryMath, [](uint32_t src) { return src; });
        break;
    case Isa::Opcode::add_imm:
        executeInstruction(instruction.binaryMathImm, [](uint32_t src1, uint32_t src2) { return src1 + src2; });
        programCounterIncrement = 2;
        break;
    case Isa::Opcode::swizzle:
        executeInstruction(instruction.swizzle);
        break;
    default:
        FATAL_ERROR("Unknown opcode: ", (uint32_t)instruction.header.opcode);
    }

    registers.pc += programCounterIncrement;
    wait();
}

void ShaderUnit::executeInstruction(Isa::InstructionLayouts::UnaryMath inst, UnaryFunction function) {
    VectorRegister &src = selectRegister(inst.src);
    VectorRegister &dest = selectRegister(inst.dest);
    for (int i = 0; i < 4; i++) {
        if (isBitSet(inst.destMask, 3 - i)) {
            dest[i] = function(src[i]);
        }
    }
}

void ShaderUnit::executeInstruction(Isa::InstructionLayouts::BinaryMath inst, BinaryFunction function) {
    VectorRegister &src1 = selectRegister(inst.src1);
    VectorRegister &src2 = selectRegister(inst.src2);
    VectorRegister &dest = selectRegister(inst.dest);
    for (int i = 0; i < 4; i++) {
        if (isBitSet(inst.destMask, 3 - i)) {
            dest[i] = function(src1[i], src2[i]);
        }
    }
}

void ShaderUnit::executeInstruction(Isa::InstructionLayouts::BinaryMathImm inst, BinaryFunction function) {
    VectorRegister &src1 = selectRegister(inst.src);
    uint32_t src2 = inst.immediateValue;
    VectorRegister &dest = selectRegister(inst.dest);
    for (int i = 0; i < 4; i++) {
        if (isBitSet(inst.destMask, 3 - i)) {
            dest[i] = function(src1[i], src2);
        }
    }
}

void ShaderUnit::executeInstruction(Isa::InstructionLayouts::Swizzle inst) {
    VectorRegister &src = selectRegister(inst.src);
    VectorRegister &dest = selectRegister(inst.dest);

    dest.x = src[static_cast<uint32_t>(inst.patternX)];
    dest.y = src[static_cast<uint32_t>(inst.patternY)];
    dest.z = src[static_cast<uint32_t>(inst.patternZ)];
    dest.w = src[static_cast<uint32_t>(inst.patternW)];
}