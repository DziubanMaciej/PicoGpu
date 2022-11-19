#pragma once

#include "gpu/isa/isa.h"
#include "gpu/types.h"
#include "gpu/util/error.h"

#include <cstddef>
#include <optional>
#include <sstream>
#include <vector>

union VsPsCustomComponents;

namespace Isa {
class PicoGpuBinary {
public:
    PicoGpuBinary();
    void reset();

    void encodeDirectiveInputOutput(RegisterSelection reg, int mask, bool input);
    void encodeDirectiveShaderType(Isa::Command::ProgramType programType);
    void finalizeDirectives();

    void encodeUnaryMath(Opcode opcode, RegisterSelection dest, RegisterSelection src, uint32_t destMask);
    void encodeBinaryMath(Opcode opcode, RegisterSelection dest, RegisterSelection src1, RegisterSelection src2, uint32_t destMask);
    void encodeTernaryMath(Opcode opcode, RegisterSelection dest, RegisterSelection src1, RegisterSelection src2, RegisterSelection src3, uint32_t destMask);
    void encodeUnaryMathImm(Opcode opcode, RegisterSelection dest, uint32_t destMask, const std::vector<int32_t> &immediateValue);
    void encodeBinaryMathImm(Opcode opcode, RegisterSelection dest, RegisterSelection src, uint32_t destMask, const std::vector<int32_t> &immediateValue);
    void encodeSwizzle(Opcode opcode, RegisterSelection dest, RegisterSelection src, SwizzlePatternComponent x, SwizzlePatternComponent y, SwizzlePatternComponent z, SwizzlePatternComponent w);
    void finalizeInstructions();

    void setHasNextCommand();

    auto &getData() { return data; }
    auto getSizeInBytes() const { return data.size() * sizeof(uint32_t); }
    auto getSizeInDwords() const { return data.size(); }
    auto hasError() const { return !error.str().empty(); }
    auto getError() const { return error.str(); }
    auto isVs() const { return programType.value() == Isa::Command::ProgramType::VertexShader; }
    auto isFs() const { return programType.value() == Isa::Command::ProgramType::FragmentShader; }

    static bool areShadersCompatible(const PicoGpuBinary &vs, const PicoGpuBinary &fs);
    VsPsCustomComponents getVsPsCustomComponents();

private:
    void encodeAttributeInterpolationForFragmentShader();
    void finalizeInputOutputDirectives(bool input);
    const char *getShaderTypeName();

    template <typename InstructionLayout>
    InstructionLayout *getSpace() {
        const uint32_t length = sizeof(InstructionLayout) / sizeof(uint32_t);
        return getSpace<InstructionLayout>(length);
    }
    template <typename InstructionLayout>
    InstructionLayout *getSpace(uint32_t length) {
        FATAL_ERROR_IF(data.size() + length - sizeof(Command::CommandStoreIsa) / sizeof(uint32_t) >= Isa::maxIsaSize, "Too long program"); // TODO make this more graceful
        data.resize(data.size() + length);
        InstructionLayout *space = reinterpret_cast<InstructionLayout *>(data.data() + data.size() - length);
        return space;
    }
    Command::CommandStoreIsa &getStoreIsaCommand() { return reinterpret_cast<Command::CommandStoreIsa &>(data[0]); }

    std::ostringstream error = {};
    std::optional<Isa::Command::ProgramType> programType = {};
    std::vector<uint32_t> data = {};

    // Below two arrays have the same format. Each element is a number from 0 to 4 describing number of
    // components used for a register. For example "#input i0.xy" and "#input i1.yzw" would set the array
    // for input regs to {2, 3, 0, 0}.
    enum class InputOutputRegisterUsage : uint8_t {
        Unknown,
        Fixed,
        Internal,
        Custom,
    };
    struct InputOutputRegister {
        InputOutputRegisterUsage usage = InputOutputRegisterUsage::Unknown;
        uint8_t componentsCount = 0;
    };
    InputOutputRegister inputs[maxInputOutputRegisters] = {};
    InputOutputRegister outputs[maxInputOutputRegisters] = {};
};

} // namespace Isa
