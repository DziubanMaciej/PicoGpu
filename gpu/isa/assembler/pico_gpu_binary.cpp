#include "gpu/definitions/custom_components.h"
#include "gpu/definitions/register_allocator.h"
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
    std::memset(&inputs, 0, sizeof(inputs));
    std::memset(&outputs, 0, sizeof(outputs));
}

void PicoGpuBinary::encodeDirectiveInputOutput(RegisterSelection reg, int mask, bool input) {
    const char *label = input ? "input" : "output";
    InputOutputRegisters &io = input ? this->inputs : this->outputs;

    // Validate component mask
    FATAL_ERROR_IF(mask == 0, "Mask must be non zero")
    FATAL_ERROR_IF(mask & ~0b1111, "Mask must be a 4-bit value");
    if (mask != 0b1000 && mask != 0b1100 && mask != 0b1110 && mask != 0b1111) {
        error << "Components for " << label << " directive must be used in order: x,y,z,w. Mask is " << mask;
        return;
    }

    // Validate if register is not already used
    if (isBitSet(io.usedRegsMask, reg)) {
        error << "Multiple " << label << " directives for r" << reg;
        return;
    }

    // Validate if we exceeded max number of io regisers
    if (io.usedRegsCount == Isa::maxInputOutputRegisters) {
        error << "Too many " << label << " directives. Max is " << Isa::maxInputOutputRegisters;
        return;
    }

    // Store the values
    io.regs[io.usedRegsCount].index = reg;
    io.regs[io.usedRegsCount].usage = InputOutputRegisterUsage::Custom; // Mark all registers as custom. This may be overriden later.
    io.regs[io.usedRegsCount].mask = mask;
    io.regs[io.usedRegsCount].componentsCount = countBits(mask);
    io.usedRegsCount++;
    setBit(io.usedRegsMask, reg);
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
    InputOutputRegisters &io = input ? this->inputs : this->outputs;

    // Ensure that vertex shader has at least one input. There are no enforced size of any inputs, so all VS inputs are
    // considered custom - there are no fixed inputs. But we require at least one.
    if (input && isVs()) {
        if (io.usedRegsCount == 0) {
            error << getShaderTypeName() << " must use at least one input register";
            return;
        }
    }

    // Ensure that vertex shader returns 4-component vector as first output and fragment shader receives it as first input.
    // Mark this input/output register as fixed
    if ((!input && isVs()) || (input && isFs())) {
        if (io.usedRegsCount == 0) {
            error << getShaderTypeName() << " must have at least 1 " << label;
            return;
        }
        if (io.regs[0].componentsCount != 4) {
            error << getShaderTypeName() << " must have a 4-component position vector as its first " << label;
            return;
        }
        FATAL_ERROR_IF(io.regs[0].usage != InputOutputRegisterUsage::Custom, "Unexpected usage of fixed io register");
        io.regs[0].usage = InputOutputRegisterUsage::Fixed;
    }

    // Ensure that fragment shader only uses one output and mark it as fixed.
    // Add a second output for interpolated z-value and mark it as internal.
    if (!input && programType.value() == Isa::Command::ProgramType::FragmentShader) {
        if (io.usedRegsCount != 1) {
            error << getShaderTypeName() << " must use one output register";
            return;
        }
        FATAL_ERROR_IF(io.regs[0].usage != InputOutputRegisterUsage::Custom, "Unexpected usage of fixed io register");
        io.regs[0].usage = InputOutputRegisterUsage::Fixed;

        // TODO we should allocate some free register and use it instead. Current code will create conflict, if user defines input0 as output too.
        encodeDirectiveInputOutput(inputs.regs[0].index, 0b1000, false);
        io.regs[1].usage = InputOutputRegisterUsage::Internal;
    }

    // Store the data to ISA
    auto &command = getStoreIsaCommand();
    if (input) {
        command.inputsCount = intToNonZeroCount(io.usedRegsCount);
        command.inputSize0 = intToNonZeroCount(io.regs[0].componentsCount);
        command.inputSize1 = intToNonZeroCount(io.regs[1].componentsCount);
        command.inputSize2 = intToNonZeroCount(io.regs[2].componentsCount);
        command.inputSize3 = intToNonZeroCount(io.regs[3].componentsCount);
        command.inputRegister0 = io.regs[0].index;
        command.inputRegister1 = io.regs[1].index;
        command.inputRegister2 = io.regs[2].index;
        command.inputRegister3 = io.regs[3].index;
    } else {
        command.outputsCount = intToNonZeroCount(io.usedRegsCount);
        command.outputSize0 = intToNonZeroCount(io.regs[0].componentsCount);
        command.outputSize1 = intToNonZeroCount(io.regs[1].componentsCount);
        command.outputSize2 = intToNonZeroCount(io.regs[2].componentsCount);
        command.outputSize3 = intToNonZeroCount(io.regs[3].componentsCount);
        command.outputRegister0 = io.regs[0].index;
        command.outputRegister1 = io.regs[1].index;
        command.outputRegister2 = io.regs[2].index;
        command.outputRegister3 = io.regs[3].index;
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

    if (vs.outputs.usedRegsCount != vs.inputs.usedRegsCount) {
        return false;
    }

    for (auto regIndex = 0u; regIndex < vs.outputs.usedRegsCount; regIndex++) {
        if (vs.outputs.regs[regIndex].componentsCount != fs.inputs.regs[regIndex].componentsCount) {
            return false;
        }
    }

    return true;
}

CustomShaderComponents PicoGpuBinary::getVsCustomInputComponents() {
    FATAL_ERROR_IF(programType.value() != Isa::Command::ProgramType::VertexShader, "Invalid shader type");

    CustomShaderComponents result = {0};
    result.registersCount = inputs.usedRegsCount;
    result.comp0 = intToNonZeroCount(inputs.regs[0].componentsCount);
    result.comp1 = intToNonZeroCount(inputs.regs[1].componentsCount);
    result.comp2 = intToNonZeroCount(inputs.regs[2].componentsCount);
    result.comp3 = intToNonZeroCount(inputs.regs[3].componentsCount);
    return result;
}

CustomShaderComponents PicoGpuBinary::getVsPsCustomComponents() {
    InputOutputRegisters *io = nullptr;
    switch (programType.value()) {
    case Isa::Command::ProgramType::VertexShader:
        io = &this->outputs;
        break;
    case Isa::Command::ProgramType::FragmentShader:
        io = &this->inputs;
        break;
    default:
        FATAL_ERROR("Invalid shader type");
    }

    uint8_t components[Isa::maxInputOutputRegisters] = {};
    uint8_t customRegsCount = {};
    for (uint32_t i = 0; i < Isa::maxInputOutputRegisters; i++) {
        if (io->regs[i].usage == InputOutputRegisterUsage::Custom) {
            components[customRegsCount++] = io->regs[i].componentsCount;
        }
    }
    FATAL_ERROR_IF(customRegsCount > Isa::maxInputOutputRegisters - 1, "Too many VsPs custom components (one component is reserved for Fixed position input");

    CustomShaderComponents result = {0};
    result.registersCount = customRegsCount;
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
        // Put Z coordinate as a first component, so it can be exported
        encodeSwizzle(Isa::Opcode::swizzle, inputs.regs[0].index, inputs.regs[0].index,
                      Isa::SwizzlePatternComponent::SwizzleZ,
                      Isa::SwizzlePatternComponent::SwizzleZ,
                      Isa::SwizzlePatternComponent::SwizzleZ,
                      Isa::SwizzlePatternComponent::SwizzleZ);
    }

    getStoreIsaCommand().programLength = data.size() - sizeof(Command::CommandStoreIsa) / sizeof(uint32_t);
}

void PicoGpuBinary::encodeAttributeInterpolationForFragmentShader() {
    constexpr static size_t perspectiveAware = true;

    /* Notation and assumptions used in this function:
    A,B,C are vertices of the triangle. When this code runs, their attributes have already been
    stored in some registers. We use RegisterAllocator to determine which ones. This depends on
    ShaderUnit performing the same logic

    P is the point that we will be interpolating. Its x,y position is stored in first input
    register. We have to interpolate Z coordinate and store it in this input. We also interpolate
    custom attributes and store them in subsequent input registers.

    Formulas for depth interpolation:
        - perspective-unaware: Z = (w0*z0) + (w1*z1) + (w2*z2)
        - perspective-aware:   Z = 1 / ((w0/z0) + (w1/z1) + (w2/z2))

    Formulas for custom attributes interpolation:
        - perspective-unaware: C = (w0*c0) + (w1*c1) + (w2*c2)
        - perspective-aware:   C = Z * (C0*w0/z0 + C1*w1/z1 + C2*w2/z2)
    */

    // Prepare register indices to operate on
    RegisterAllocator registerAllocator{inputs.usedRegsMask};
    size_t maxInputOutputRegisters = 3; // TODO remove it and make Isa::maxInputOutputRegisters=3
    RegisterSelection regPerTriangleAttrib[verticesInPrimitive][maxInputOutputRegisters] = {};
    for (int vertexIndex = 0; vertexIndex < verticesInPrimitive; vertexIndex++) {
        for (int inputIndex = 0; inputIndex < inputs.usedRegsCount; inputIndex++) {
            regPerTriangleAttrib[vertexIndex][inputIndex] = registerAllocator.allocate(); // this has to be in sync with how SU lays out these attributes
        }
    }
    const RegisterSelection regPositionP = inputs.regs[0].index;
    const RegisterSelection regPositionA = regPerTriangleAttrib[0][0];
    const RegisterSelection regPositionB = regPerTriangleAttrib[1][0];
    const RegisterSelection regPositionC = regPerTriangleAttrib[2][0];

    // Calculate edges (2 component vectors). We may temporarily store them at the future
    // location of inputs  we're interpolating here.
    const RegisterSelection regEdgeAB = inputs.usedRegsCount > 1 ? inputs.regs[1].index : registerAllocator.allocate();
    const RegisterSelection regEdgeAC = inputs.usedRegsCount > 2 ? inputs.regs[2].index : registerAllocator.allocate();
    const RegisterSelection regEdgeAP = inputs.usedRegsCount > 3 ? inputs.regs[3].index : registerAllocator.allocate();
    encodeBinaryMath(Isa::Opcode::fsub, regEdgeAB, regPositionB, regPositionA, 0b1100);
    encodeBinaryMath(Isa::Opcode::fsub, regEdgeAC, regPositionC, regPositionA, 0b1100);
    encodeBinaryMath(Isa::Opcode::fsub, regEdgeAP, regPositionP, regPositionA, 0b1100);

    // Calculate parallelogram areas (store 2D cross product result in all 4 components)
    const RegisterSelection regAreaABP = registerAllocator.allocate();
    const RegisterSelection regAreaACP = registerAllocator.allocate();
    const RegisterSelection regAreaABC = registerAllocator.allocate();
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

    // Correct the weights to be pespective aware - divide all components by Z
    if (perspectiveAware) {
        encodeBinaryMath(Isa::Opcode::fdiv, regWeightA, regWeightA, regPositionA, 0b0010);
        encodeBinaryMath(Isa::Opcode::fdiv, regWeightB, regWeightB, regPositionB, 0b0010);
        encodeBinaryMath(Isa::Opcode::fdiv, regWeightC, regWeightC, regPositionC, 0b0010);
        encodeSwizzle(Isa::Opcode::swizzle, regWeightA, regWeightA, Isa::SwizzlePatternComponent::SwizzleZ, Isa::SwizzlePatternComponent::SwizzleZ, Isa::SwizzlePatternComponent::SwizzleZ, Isa::SwizzlePatternComponent::SwizzleZ);
        encodeSwizzle(Isa::Opcode::swizzle, regWeightB, regWeightB, Isa::SwizzlePatternComponent::SwizzleZ, Isa::SwizzlePatternComponent::SwizzleZ, Isa::SwizzlePatternComponent::SwizzleZ, Isa::SwizzlePatternComponent::SwizzleZ);
        encodeSwizzle(Isa::Opcode::swizzle, regWeightC, regWeightC, Isa::SwizzlePatternComponent::SwizzleZ, Isa::SwizzlePatternComponent::SwizzleZ, Isa::SwizzlePatternComponent::SwizzleZ, Isa::SwizzlePatternComponent::SwizzleZ);
    }

    // Interpolate depth
    const RegisterSelection regZ = regPositionA; // we may reuse this slot after interpolating depth
    if (perspectiveAware) {
        encodeBinaryMath(Isa::Opcode::fadd, regPositionP, regPositionP, regWeightA, 0b0010);
        encodeBinaryMath(Isa::Opcode::fadd, regPositionP, regPositionP, regWeightB, 0b0010);
        encodeBinaryMath(Isa::Opcode::fadd, regPositionP, regPositionP, regWeightC, 0b0010);
        encodeUnaryMath(Isa::Opcode::frcp, regPositionP, regPositionP, 0b0010);

        // Prepare a register with all components set to interpolated Z for future
        encodeSwizzle(Isa::Opcode::swizzle, regZ, regPositionP, Isa::SwizzlePatternComponent::SwizzleZ, Isa::SwizzlePatternComponent::SwizzleZ, Isa::SwizzlePatternComponent::SwizzleZ, Isa::SwizzlePatternComponent::SwizzleZ);
    } else {
        encodeTernaryMath(Isa::Opcode::fmad, regPositionP, regWeightA, regPositionA, regPositionP, 0b0010);
        encodeTernaryMath(Isa::Opcode::fmad, regPositionP, regWeightB, regPositionB, regPositionP, 0b0010);
        encodeTernaryMath(Isa::Opcode::fmad, regPositionP, regWeightC, regPositionC, regPositionP, 0b0010);
    }

    // Interpolate custom attributes
    for (uint32_t i = 1; i < maxInputOutputRegisters; i++) {
        if (inputs.regs[i].usage == InputOutputRegisterUsage::Custom) {
            const auto regDst = inputs.regs[i].index;
            const auto mask = inputs.regs[i].mask;
            encodeUnaryMathImm(Isa::Opcode::init, regDst, 0b1111, {0});
            encodeTernaryMath(Isa::Opcode::fmad, regDst, regWeightA, regPerTriangleAttrib[0][i], regDst, mask);
            encodeTernaryMath(Isa::Opcode::fmad, regDst, regWeightB, regPerTriangleAttrib[1][i], regDst, mask);
            encodeTernaryMath(Isa::Opcode::fmad, regDst, regWeightC, regPerTriangleAttrib[2][i], regDst, mask);

            if (perspectiveAware) {
                encodeBinaryMath(Isa::Opcode::fmul, regDst, regDst, regZ, 0b1111);
            }
        }
    }
}

void PicoGpuBinary::setHasNextCommand() {
    getStoreIsaCommand().hasNextCommand = 1;
}

} // namespace Isa
