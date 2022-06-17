#include "gpu/isa/assembler/pico_gpu_binary.h"
#include "gpu/util/error.h"
#include "gpu/util/math.h"

#include <cstdio>

namespace Isa {

PicoGpuBinary::PicoGpuBinary() {
    const size_t headerSize = sizeof(Header) / sizeof(uint32_t);
    data.resize(headerSize);
    header = reinterpret_cast<Header *>(data.data());
    directives = data.data() + headerSize;
}

void PicoGpuBinary::encodeDirectiveInput(int mask) {
    printf("i%d mask=0x%02X\n", inputComponentsCount, mask);
    FATAL_ERROR_IF(mask & ~0b1111, "Mask must be a 4-bit value");

    int components = countBits(mask);

    switch (inputRegistersCount) {
    case 0:
        header->dword2.inputSize0 = components;
        break;
    case 1:
        header->dword2.inputSize1 = components;
        break;
    case 2:
        header->dword2.inputSize2 = components;
        break;
    case 3:
        header->dword2.inputSize3 = components;
        break;
    default:
        FATAL_ERROR("Too many input directives. Max is 4");
    }
    inputRegistersCount++;
    inputComponentsCount += components;
}

void PicoGpuBinary::encodeDirectiveOutput(int mask) {
    printf("o%d mask=0x%02X\n", outputComponentsCount, mask);
    FATAL_ERROR_IF(mask & ~0b1111, "Mask must be a 4-bit value");

    int components = countBits(mask);

    switch (outputRegistersCount) {
    case 0:
        header->dword2.outputSize0 = components;
        break;
    case 1:
        header->dword2.outputSize1 = components;
        break;
    case 2:
        header->dword2.outputSize2 = components;
        break;
    case 3:
        header->dword2.outputSize3 = components;
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

void PicoGpuBinary::encodeBinaryMathImm(Opcode opcode, RegisterSelection dest, RegisterSelection src, uint32_t destMask, uint32_t immediateValue) {
    auto inst = getSpace<InstructionLayouts::BinaryMathImm>();
    inst->opcode = opcode;
    inst->dest = dest;
    inst->src = src;
    inst->destMask = destMask;
    inst->immediateValue = immediateValue;
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

    header->dword1.programLength = data.size() - sizeof(Header) / sizeof(uint32_t);

    *error = nullptr;
    return true;
}

} // namespace Isa
