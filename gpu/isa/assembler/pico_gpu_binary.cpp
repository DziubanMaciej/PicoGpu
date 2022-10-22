#include "gpu/isa/assembler/pico_gpu_binary.h"
#include "gpu/util/math.h"

namespace Isa {

PicoGpuBinary::PicoGpuBinary() {
    const size_t headerSize = sizeof(Command::CommandStoreIsa) / sizeof(uint32_t);
    data.resize(headerSize);
}

void PicoGpuBinary::reset() {
    error.clear();
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

void PicoGpuBinary::finalizeDirectives() {
    finalizeInputOutputDirectives(false);
    finalizeInputOutputDirectives(true);
}

void PicoGpuBinary::finalizeInputOutputDirectives(bool input) {
    const char *label = input ? "input" : "output";
    auto components = input ? inputRegistersComponents : outputRegistersComponents;
    int indexOffset = input ? Isa::inputRegistersOffset : Isa::outputRegistersOffset;

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
    getStoreIsaCommand().programLength = data.size() - sizeof(Command::CommandStoreIsa) / sizeof(uint32_t);
}

void PicoGpuBinary::setHasNextCommand() {
    getStoreIsaCommand().hasNextCommand = 1;
}

} // namespace Isa
