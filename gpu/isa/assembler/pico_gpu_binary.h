#pragma once

#include "gpu/isa/isa.h"
#include "gpu/util/error.h"

#include <cstddef>
#include <vector>

namespace Isa {
class PicoGpuBinary {
public:
    PicoGpuBinary();
    PicoGpuBinary &operator=(const PicoGpuBinary &) = default;

    void encodeDirectiveInput(int mask);
    void encodeDirectiveOutput(int mask);
    bool finalizeDirectives(const char **error);

    void encodeUnaryMath(Opcode opcode, RegisterSelection dest, RegisterSelection src, uint32_t destMask);
    void encodeBinaryMath(Opcode opcode, RegisterSelection dest, RegisterSelection src1, RegisterSelection src2, uint32_t destMask);
    void encodeBinaryMathImm(Opcode opcode, RegisterSelection dest, RegisterSelection src, uint32_t destMask, uint32_t immediateValue);
    void encodeSwizzle(Opcode opcode, RegisterSelection dest, RegisterSelection src, SwizzlePatternComponent x, SwizzlePatternComponent y, SwizzlePatternComponent z, SwizzlePatternComponent w);
    bool finalizeInstructions(const char **error);

    void setHasNextCommand();

    auto &getData() { return data; }

private:
    template <typename InstructionLayout>
    InstructionLayout *getSpace() {
        const uint32_t length = sizeof(InstructionLayout) / sizeof(uint32_t);
        FATAL_ERROR_IF(data.size() + length - sizeof(Command::CommandStoreIsa) / sizeof(uint32_t) >= Isa::maxIsaSize, "Too long program"); // TODO make this more graceful
        data.resize(data.size() + length);
        InstructionLayout *space = reinterpret_cast<InstructionLayout *>(data.data() + data.size() - length);
        return space;
    }
    Command::CommandStoreIsa &getStoreIsaCommand() { return reinterpret_cast<Command::CommandStoreIsa &>(data[0]); }

    std::vector<uint32_t> data = {};
    uint32_t *directives;
    uint32_t *instructions;

    size_t inputRegistersCount = 0;
    size_t outputRegistersCount = 0;

    size_t inputComponentsCount = 0;
    size_t outputComponentsCount = 0;
};

} // namespace Isa
