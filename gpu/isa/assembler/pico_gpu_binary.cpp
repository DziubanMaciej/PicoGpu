#include "gpu/custom_components.h"
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
    std::fill_n(inputs, maxInputOutputRegisters, InputOutputRegister{});
    std::fill_n(outputs, maxInputOutputRegisters, InputOutputRegister{});
}

void PicoGpuBinary::encodeDirectiveInputOutput(RegisterSelection reg, int mask, bool input) {
    const char *label = input ? "input" : "output";
    const size_t indexOffset = input ? Isa::inputRegistersOffset : Isa::outputRegistersOffset;
    const size_t ioOffset = reg - indexOffset;
    InputOutputRegister *io = input ? this->inputs : this->outputs;

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
    if (io[ioOffset].usage != InputOutputRegisterUsage::Unknown) {
        error << "Multiple " << (input ? "input" : "output") << " directives for r" << reg;
        return;
    }

    // Store the values
    io[ioOffset].usage = InputOutputRegisterUsage::Custom; // Mark all registers as custom. This may be overriden later.
    io[ioOffset].componentsCount = countBits(mask);        // TODO should we remember the mask and load it correctly in shader unit? Or return error for masks like .xw?
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
    InputOutputRegister *io = input ? this->inputs : this->outputs;
    const auto indexOffset = input ? Isa::inputRegistersOffset : Isa::outputRegistersOffset;

    // Count used io registers up until the first Unknown register
    size_t usedRegistersCount = 0;
    for (; usedRegistersCount < Isa::maxInputOutputRegisters; usedRegistersCount++) {
        if (io[usedRegistersCount].usage == InputOutputRegisterUsage::Unknown) {
            break;
        }
    }
    if (usedRegistersCount == 0) {
        error << "At least one " << label << " register is required";
        return;
    }

    // Ensure that no other registers after Unknown is used
    for (size_t i = usedRegistersCount + 1; i < Isa::maxInputOutputRegisters; i++) {
        if (io[i].usage != InputOutputRegisterUsage::Unknown) {
            error << "r" << (indexOffset + i) << " register was used as " << label << ". Use r" << (indexOffset + i - 1) << " first";
            return;
        }
    }

    // Ensure that vertex shader returns 4-component vector as first output and fragment shader receives it as first input.
    // Mark this input/output register as fixed
    if ((!input && isVs()) || (input && isFs())) {
        if (io[0].componentsCount != 4) {
            error << getShaderTypeName() << " must " << label << " a 4-component vector in r" << indexOffset;
            return;
        }
        FATAL_ERROR_IF(io[0].usage != InputOutputRegisterUsage::Custom, "Unexpected usage of fixed io register");
        io[0].usage = InputOutputRegisterUsage::Fixed;
    }

    // Ensure that fragment shader only uses one output and mark it as fixed.
    // Add a second output for interpolated z-value and mark it as internal.
    if (!input && programType.value() == Isa::Command::ProgramType::FragmentShader) {
        if (usedRegistersCount != 1) {
            error << getShaderTypeName() << " may use only one output register";
            return;
        }
        io[0].usage = InputOutputRegisterUsage::Fixed;

        encodeDirectiveInputOutput(indexOffset + 1, 0b1000, false);
        io[1].usage = InputOutputRegisterUsage::Internal;
        usedRegistersCount++;
    }

    // Store the data to ISA
    auto &command = getStoreIsaCommand();
    if (input) {
        command.inputsCount = intToNonZeroCount(usedRegistersCount);
        command.inputSize0 = intToNonZeroCount(inputs[0].componentsCount);
        command.inputSize1 = intToNonZeroCount(inputs[1].componentsCount);
        command.inputSize2 = intToNonZeroCount(inputs[2].componentsCount);
        command.inputSize3 = intToNonZeroCount(inputs[3].componentsCount);
    } else {
        command.outputsCount = intToNonZeroCount(usedRegistersCount);
        command.outputSize0 = intToNonZeroCount(outputs[0].componentsCount);
        command.outputSize1 = intToNonZeroCount(outputs[1].componentsCount);
        command.outputSize2 = intToNonZeroCount(outputs[2].componentsCount);
        command.outputSize3 = intToNonZeroCount(outputs[3].componentsCount);
    }
}

const char *PicoGpuBinary::getShaderTypeName() {
    if (!programType.has_value()) {
        return "UnknownShader";
    }
    if (isVs()) {
        return "VertexShader";
    }
    if (isFs()) {
        return "FragmentShader";
    }
    return "UnknownShader";
}

bool PicoGpuBinary::areShadersCompatible(const PicoGpuBinary &vs, const PicoGpuBinary &fs) {
    FATAL_ERROR_IF(!vs.isVs(), "Expected a vertex shader");
    FATAL_ERROR_IF(!fs.isFs(), "Expected a fragment shader");
    return memcmp(vs.outputs, fs.inputs, sizeof(vs.outputs)) == 0;
}

CustomShaderComponents PicoGpuBinary::getVsPsCustomComponents() {
    InputOutputRegister *io = nullptr;
    switch (programType.value()) {
    case Isa::Command::ProgramType::VertexShader:
        io = this->outputs;
        break;
    case Isa::Command::ProgramType::FragmentShader:
        io = this->inputs;
        break;
    default:
        FATAL_ERROR("Invalid shader type");
    }

    uint8_t components[Isa::maxInputOutputRegisters] = {};
    uint8_t registersCount = {};
    for (uint32_t i = 0; i < Isa::maxInputOutputRegisters; i++) {
        if (io[i].usage == InputOutputRegisterUsage::Custom) {
            components[registersCount++] = io[i].componentsCount;
        }
    }

    CustomShaderComponents result = {0};
    result.registersCount = registersCount;
    result.comp0 = intToNonZeroCount(components[0]);
    result.comp1 = intToNonZeroCount(components[1]);
    result.comp2 = intToNonZeroCount(components[2]);
    return result;
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

    constexpr size_t perTriangleAttribsOffset = 7;
    const RegisterSelection regPositionP = 0;
    const RegisterSelection regPositionA = perTriangleAttribsOffset + 0;
    const RegisterSelection regPositionB = perTriangleAttribsOffset + 3;
    const RegisterSelection regPositionC = perTriangleAttribsOffset + 6;

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

    // Interpolate custom attributes
    for (uint32_t i = 1; i < Isa::maxInputOutputRegisters; i++) {
        if (inputs[i].usage == InputOutputRegisterUsage::Custom) {
            const RegisterSelection regDst = Isa::inputRegistersOffset + i;
            const RegisterSelection regPerTriangle = perTriangleAttribsOffset + i;
            encodeUnaryMathImm(Isa::Opcode::init, regDst, 0b1111, {0});
            encodeTernaryMath(Isa::Opcode::fmad, regDst, regWeightA, regPerTriangle + 0, regDst, 0b1111); // TODO mask could be more narrow
            encodeTernaryMath(Isa::Opcode::fmad, regDst, regWeightB, regPerTriangle + 3, regDst, 0b1111);
            encodeTernaryMath(Isa::Opcode::fmad, regDst, regWeightC, regPerTriangle + 6, regDst, 0b1111);
        }
    }
}

void PicoGpuBinary::setHasNextCommand() {
    getStoreIsaCommand().hasNextCommand = 1;
}

} // namespace Isa
