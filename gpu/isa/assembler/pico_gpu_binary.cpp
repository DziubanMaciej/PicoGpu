#include "gpu/isa/assembler/pico_gpu_binary.h"
#include "gpu/util/math.h"

namespace Isa {

PicoGpuBinary::PicoGpuBinary() {
    const size_t headerSize = sizeof(Command::CommandStoreIsa) / sizeof(uint32_t);
    data.resize(headerSize);
    directives = data.data() + headerSize;
}

void PicoGpuBinary::encodeDirectiveInput(int mask) {
    FATAL_ERROR_IF(mask & ~0b1111, "Mask must be a 4-bit value");

    const int components = countBits(mask);
    const auto componentsField = intToNonZeroCount(components);

    switch (inputRegistersCount) {
    case 0:
        getStoreIsaCommand().inputSize0 = componentsField;
        break;
    case 1:
        getStoreIsaCommand().inputSize1 = componentsField;
        break;
    case 2:
        getStoreIsaCommand().inputSize2 = componentsField;
        break;
    case 3:
        getStoreIsaCommand().inputSize3 = componentsField;
        break;
    default:
        FATAL_ERROR("Too many input directives. Max is 4");
    }
    inputRegistersCount++;
    inputComponentsCount += components;
}

void PicoGpuBinary::encodeDirectiveOutput(int mask) {
    FATAL_ERROR_IF(mask & ~0b1111, "Mask must be a 4-bit value");

    const int components = countBits(mask);
    const auto componentsField = intToNonZeroCount(components);

    switch (outputRegistersCount) {
    case 0:
        getStoreIsaCommand().outputSize0 = componentsField;
        break;
    case 1:
        getStoreIsaCommand().outputSize1 = componentsField;
        break;
    case 2:
        getStoreIsaCommand().outputSize2 = componentsField;
        break;
    case 3:
        getStoreIsaCommand().outputSize3 = componentsField;
        break;
    default:
        FATAL_ERROR("Too many output directives. Max is 4");
    }
    outputRegistersCount++;
    outputComponentsCount += components;
}

bool PicoGpuBinary::finalizeDirectives(const char **error) {
    if (inputRegistersCount == 0) {
        *error = "At least one input directive is required";
        return false;
    }
    if (outputRegistersCount == 0) {
        *error = "At least one output directive is required";
        return false;
    }

    getStoreIsaCommand().inputsCount = intToNonZeroCount(inputRegistersCount);
    getStoreIsaCommand().outputsCount = intToNonZeroCount(outputRegistersCount);

    *error = nullptr;
    return true;
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
    FATAL_ERROR_IF(immediateValues.empty(), "UnaryMathImm must have at least one immediate value\n");
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
    FATAL_ERROR_IF(immediateValues.empty(), "BinaryMathImm must have at least one immediate value\n");
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

bool PicoGpuBinary::finalizeInstructions(const char **error) {
    if (outputRegistersCount == 0) {
        *error = "At least one output directive is required";
        return false;
    }

    getStoreIsaCommand().programLength = data.size() - sizeof(Command::CommandStoreIsa) / sizeof(uint32_t);

    *error = nullptr;
    return true;
}

void PicoGpuBinary::setHasNextCommand() {
    getStoreIsaCommand().hasNextCommand = 1;
}

} // namespace Isa
