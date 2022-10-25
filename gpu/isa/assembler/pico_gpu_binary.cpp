#include "gpu/isa/assembler/pico_gpu_binary.h"
#include "gpu/util/conversions.h"
#include "gpu/util/math.h"

namespace Isa {

PicoGpuBinary::PicoGpuBinary() {
    const size_t headerSize = sizeof(Command::CommandStoreIsa) / sizeof(uint32_t);
    data.resize(headerSize);
}

void PicoGpuBinary::reset() {
    error.clear();
    programType = {};
    std::fill(data.begin(), data.end(), 0);
    std::fill_n(inputRegistersComponents, maxInputOutputRegisters, 0);
    std::fill_n(outputRegistersComponents, maxInputOutputRegisters, 0);
}

void PicoGpuBinary::encodeDirectiveInputOutput(RegisterSelection reg, int mask, bool input) {
    const char *label = input ? "input" : "output";
    auto components = input ? inputRegistersComponents : outputRegistersComponents;
    int indexOffset = input ? Isa::inputRegistersOffset : Isa::outputRegistersOffset;

    // Validate mask
    FATAL_ERROR_IF(mask == 0, "Mask must be non zero")
    FATAL_ERROR_IF(mask & ~0b1111, "Mask must be a 4-bit value");

    // Validate if register index is correct
    const auto minReg = indexOffset;
    const auto maxReg = indexOffset + Isa::maxInputOutputRegisters - 1;
    if (maxReg < reg || reg < minReg) {
        error << "Invalid " << label << " register: r" << reg << ". Must be between r" << minReg << " and r" << maxReg;
        return;
    }

    // Validate if this register was already declared as input/output
    if (components[reg - indexOffset] != 0) {
        error << "Multiple " << (input ? "input" : "output") << " directives for r" << reg;
        return;
    }

    // Write the value to the data structure
    components[reg - indexOffset] = countBits(mask); // TODO should we remember the mask and load it correctly in shader unit? Or return error for masks like .xw?
}

void PicoGpuBinary::encodeDirectiveShaderType(Isa::Command::ProgramType programType) {
    if (this->programType.has_value()) {
        error << "Multiple program type specifications";
        return;
    }
    this->programType = programType;
}

void PicoGpuBinary::finalizeDirectives() {
    if (!this->programType.has_value()) {
        error << "No program type specification";
        return;
    } else {
        getStoreIsaCommand().programType = this->programType.value();
    }

    finalizeInputOutputDirectives(false);
    finalizeInputOutputDirectives(true);

    // Insert preamble code if necessary
    if (this->programType.value() == Isa::Command::ProgramType::FragmentShader) {
        encodeAttributeInterpolationForFragmentShader();
    }
}

void PicoGpuBinary::finalizeInputOutputDirectives(bool input) {
    const char *label = input ? "input" : "output";
    auto components = input ? inputRegistersComponents : outputRegistersComponents;
    const auto indexOffset = input ? Isa::inputRegistersOffset : Isa::outputRegistersOffset;

    // Count used components (before the first zero in the array)
    size_t usedRegistersCount = 0;
    for (; usedRegistersCount < Isa::maxInputOutputRegisters; usedRegistersCount++) {
        if (components[usedRegistersCount] == 0) {
            break;
        }
    }
    if (usedRegistersCount == 0) {
        error << "At least one " << label << " register is required";
        return;
    }

    // Ensure that fragment shader only uses one output. Add second internal output for interpolated z-value
    if (!input && programType.value() == Isa::Command::ProgramType::FragmentShader) {
        if (usedRegistersCount != 1) {
            error << "Fragment shader may use only one output register";
            return;
        }
        encodeDirectiveInputOutput(indexOffset + 1, 0b1000, false);
        components[1] = 1;
        usedRegistersCount = 2;
    }

    // Check for more components after the first zero, which is illegal.
    for (size_t i = usedRegistersCount + 1; i < Isa::maxInputOutputRegisters; i++) {
        if (components[i] != 0) {
            error << "r" << (indexOffset + i) << " register was used as " << label << ". Use r" << (indexOffset + i - 1) << " first";
            return;
        }
    }

    // Write the data to ISA
    auto &command = getStoreIsaCommand();
    if (input) {
        command.inputsCount = intToNonZeroCount(usedRegistersCount);
        command.inputSize0 = intToNonZeroCount(components[0]);
        command.inputSize1 = intToNonZeroCount(components[1]);
        command.inputSize2 = intToNonZeroCount(components[2]);
        command.inputSize3 = intToNonZeroCount(components[3]);
    } else {
        command.outputsCount = intToNonZeroCount(usedRegistersCount);
        command.outputSize0 = intToNonZeroCount(components[0]);
        command.outputSize1 = intToNonZeroCount(components[1]);
        command.outputSize2 = intToNonZeroCount(components[2]);
        command.outputSize3 = intToNonZeroCount(components[3]);
    }
}

void PicoGpuBinary::encodeUnaryMath(Opcode opcode, RegisterSelection dest, RegisterSelection src, uint32_t destMask) {
    auto inst = getSpace<InstructionLayouts::UnaryMath>();
    inst->opcode = opcode;
    inst->dest = dest;
    inst->src = src;
    inst->destMask = destMask;
}

void PicoGpuBinary::encodeBinaryMath(Opcode opcode, RegisterSelection dest, RegisterSelection src1, RegisterSelection src2, uint32_t destMask) {
    auto inst = getSpace<InstructionLayouts::BinaryMath>();
    inst->opcode = opcode;
    inst->dest = dest;
    inst->src1 = src1;
    inst->src2 = src2;
    inst->destMask = destMask;
}

void PicoGpuBinary::encodeTernaryMath(Opcode opcode, RegisterSelection dest, RegisterSelection src1, RegisterSelection src2, RegisterSelection src3, uint32_t destMask) {
    auto inst = getSpace<InstructionLayouts::TernaryMath>();
    inst->opcode = opcode;
    inst->dest = dest;
    inst->src1 = src1;
    inst->src2 = src2;
    inst->src3 = src3;
    inst->destMask = destMask;
}

void PicoGpuBinary::encodeUnaryMathImm(Opcode opcode, RegisterSelection dest, uint32_t destMask, const std::vector<int32_t> &immediateValues) {
    FATAL_ERROR_IF(immediateValues.empty(), "UnaryMathImm must have at least one immediate value");
    FATAL_ERROR_IF(immediateValues.size() > 4, "UnaryMathImm can have at most 4 immediate values");

    auto inst = getSpace<InstructionLayouts::UnaryMathImm>(1 + immediateValues.size());
    inst->opcode = opcode;
    inst->dest = dest;
    inst->destMask = destMask;
    inst->immediateValuesCount = intToNonZeroCount(immediateValues.size());
    for (uint32_t i = 0; i < immediateValues.size(); i++) {
        inst->immediateValues[i] = reinterpret_cast<const uint32_t &>(immediateValues[i]);
    }
}

void PicoGpuBinary::encodeBinaryMathImm(Opcode opcode, RegisterSelection dest, RegisterSelection src, uint32_t destMask, const std::vector<int32_t> &immediateValues) {
    FATAL_ERROR_IF(immediateValues.empty(), "BinaryMathImm must have at least one immediate value");
    FATAL_ERROR_IF(immediateValues.size() > 4, "BinaryMathImm can have at most 4 immediate values");

    auto inst = getSpace<InstructionLayouts::BinaryMathImm>(1 + immediateValues.size());
    inst->opcode = opcode;
    inst->dest = dest;
    inst->src = src;
    inst->destMask = destMask;
    inst->immediateValuesCount = intToNonZeroCount(immediateValues.size());
    for (uint32_t i = 0; i < immediateValues.size(); i++) {
        inst->immediateValues[i] = reinterpret_cast<const uint32_t &>(immediateValues[i]);
    }
}

void PicoGpuBinary::encodeSwizzle(Opcode opcode, RegisterSelection dest, RegisterSelection src,
                                  SwizzlePatternComponent x, SwizzlePatternComponent y, SwizzlePatternComponent z, SwizzlePatternComponent w) {
    auto inst = getSpace<InstructionLayouts::Swizzle>();
    inst->opcode = opcode;
    inst->dest = dest;
    inst->src = src;
    inst->patternX = x;
    inst->patternY = y;
    inst->patternZ = z;
    inst->patternW = w;
}

void PicoGpuBinary::finalizeInstructions() {
    if (programType.value() == Isa::Command::ProgramType::FragmentShader) {
        encodeSwizzle(Isa::Opcode::swizzle, Isa::outputRegistersOffset + 1, Isa::inputRegistersOffset,
                      Isa::SwizzlePatternComponent::SwizzleZ,
                      Isa::SwizzlePatternComponent::SwizzleZ,
                      Isa::SwizzlePatternComponent::SwizzleZ,
                      Isa::SwizzlePatternComponent::SwizzleZ);
    }

    getStoreIsaCommand().programLength = data.size() - sizeof(Command::CommandStoreIsa) / sizeof(uint32_t);
}

void PicoGpuBinary::encodeAttributeInterpolationForFragmentShader() {
    /* Notation and assumptions used in this function:
    A,B,C are vertices of the triangle. Their attributes are stored in the last registers like so:
        - A: r7 (position), r8, r9
        - B: r10 (position), r11, r12
        - C: r13 (position), r14, r15

    P is the point that we will be interpolating. Its x,y position is stored in r0.
    */

    const RegisterSelection regPositionP = 0;
    const RegisterSelection regPositionA = 7;
    const RegisterSelection regPositionB = 10;
    const RegisterSelection regPositionC = 13;

    // Calculate edges (2 component vectors)
    const RegisterSelection regEdgeAB = 1;
    const RegisterSelection regEdgeAC = 2;
    const RegisterSelection regEdgeAP = 3;
    encodeBinaryMath(Isa::Opcode::fsub, regEdgeAB, regPositionB, regPositionA, 0b1100);
    encodeBinaryMath(Isa::Opcode::fsub, regEdgeAC, regPositionC, regPositionA, 0b1100);
    encodeBinaryMath(Isa::Opcode::fsub, regEdgeAP, regPositionP, regPositionA, 0b1100);

    // Calculate parallelogram areas (store 2D cross product result in all 4 components)
    const RegisterSelection regAreaABP = 4;
    const RegisterSelection regAreaACP = 5;
    const RegisterSelection regAreaABC = 6;
    encodeBinaryMath(Isa::Opcode::fcross2, regAreaABP, regEdgeAB, regEdgeAP, 0b1111);
    encodeBinaryMath(Isa::Opcode::fcross2, regAreaACP, regEdgeAP, regEdgeAC, 0b1111);
    encodeBinaryMath(Isa::Opcode::fcross2, regAreaABC, regEdgeAB, regEdgeAC, 0b1111);

    // Calculate barycentric coordinates - weights (store in all components).
    // We may reuse registers used for storing area, because they will not be used afterwards.
    const RegisterSelection regWeightC = regAreaABP;
    const RegisterSelection regWeightB = regAreaACP;
    const RegisterSelection regWeightA = regAreaABC;
    const int32_t floatOne = Conversions::floatBytesToInt(1.0f);
    encodeBinaryMath(Isa::Opcode::fdiv, regWeightC, regAreaABP, regAreaABC, 0b1111);
    encodeBinaryMath(Isa::Opcode::fdiv, regWeightB, regAreaACP, regAreaABC, 0b1111);
    encodeUnaryMathImm(Isa::Opcode::init, regWeightA, 0b1111, {floatOne, floatOne, floatOne, floatOne});
    encodeBinaryMath(Isa::Opcode::fsub, regWeightA, regWeightA, regWeightB, 0b1111);
    encodeBinaryMath(Isa::Opcode::fsub, regWeightA, regWeightA, regWeightC, 0b1111);

    // Interpolate depth
    encodeTernaryMath(Isa::Opcode::fmad, regPositionP, regWeightA, regPositionA, regPositionP, 0b0010);
    encodeTernaryMath(Isa::Opcode::fmad, regPositionP, regWeightB, regPositionB, regPositionP, 0b0010);
    encodeTernaryMath(Isa::Opcode::fmad, regPositionP, regWeightC, regPositionC, regPositionP, 0b0010);
}

void PicoGpuBinary::setHasNextCommand() {
    getStoreIsaCommand().hasNextCommand = 1;
}

} // namespace Isa
