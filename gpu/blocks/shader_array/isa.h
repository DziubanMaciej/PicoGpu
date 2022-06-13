#pragma once

#include <cstddef>
#include <cstdint>

namespace Isa {

constexpr inline size_t maxIsaSizeExponent = 5;
constexpr inline size_t maxIsaSize = 1 << maxIsaSizeExponent;

constexpr inline size_t inputRegistersCount = 4;
constexpr inline size_t outputRegistersCount = 4;
constexpr inline size_t generalPurposeRegistersCount = 8;

struct Header {
    // First dword is a general descriptor specifying various sizes.
    union Dword1 {
        struct {
            uint32_t programLength : 16;
            uint32_t programType : 2;
            uint32_t threadCount : 5;
        };
        uint32_t raw;
    } dword1;

    // Second dword specifies sizes of inputs and outputs. All inputs and outputs
    // are 4 component vectors and we can select how many channels we will specify. During
    // data streaming into the shader unit. For example if we have two input vectors,
    // two components each, we will need to stream 4 values: x,y,z,w and values of these
    // vectors would be:
    //  v0 = (x, y, 0, 0)
    //  v1 = (z, w, 0, 0)
    union Dword2 {
        struct {
            uint32_t inputSize0 : 3;
            uint32_t inputSize1 : 3;
            uint32_t inputSize2 : 3;
            uint32_t inputSize3 : 3;
            uint32_t outputSize0 : 3;
            uint32_t outputSize1 : 3;
            uint32_t outputSize2 : 3;
            uint32_t outputSize3 : 3;
        };
        uint32_t raw;
    } dword2;
};

static_assert(sizeof(Header::Dword1) == 4);
static_assert(sizeof(Header::Dword2) == 4);
static_assert(sizeof(Header) == sizeof(Header::Dword1) + sizeof(Header::Dword2));

enum class Opcode : uint32_t {
    // Integer math
    add,
    add_imm,
    mul,
    mov,

    // Misc
    swizzle,
};

enum class SwizzlePatternComponent : uint32_t {
    SwizzleX = 0b00,
    SwizzleY = 0b01,
    SwizzleZ = 0b10,
    SwizzleW = 0b11,
};

enum class RegisterSelection : uint32_t {
    i0 = 0,
    i1 = 1,
    i2 = 2,
    i3 = 3,
    o0 = 4,
    o1 = 5,
    o2 = 6,
    o3 = 7,
    r0 = 8,
    r1 = 9,
    r2 = 10,
    r3 = 11,
    r4 = 12,
    r5 = 13,
    r6 = 14,
    r7 = 15,
};

namespace InstructionLayouts {

// This is not an actual instruction layout, but just some type that can be used to extract
// the opcode.
struct Header {
    Opcode opcode : 5;
};

// Any operation, that can take a register, compute something and output to a register
// Destination is masked, which means we can select which channels of the vector registers
// will be affected (1 bit per channel)
struct UnaryMath {
    Opcode opcode : 5;
    RegisterSelection dest : 4;
    RegisterSelection src : 4;
    uint32_t destMask : 4;
};

// Any operation, that can take two registers, compute something and output to a register
// Destination is masked, which means we can select which channels of the vector registers
// will be affected (1 bit per channel)
struct BinaryMath {
    Opcode opcode : 5;
    RegisterSelection dest : 4;
    RegisterSelection src1 : 4;
    RegisterSelection src2 : 4;
    uint32_t destMask : 4;
};

// Any operation, that can take a register and an immediate value, compute something and output
// to a register. Destination is masked, which means we can select which channels of the vector registers
// will be affected (1 bit per channel)
struct BinaryMathImm {
    Opcode opcode : 5;
    RegisterSelection dest : 4;
    RegisterSelection src : 4;
    uint32_t destMask : 4;
    uint32_t reserved : 15;
    uint32_t immediateValue : 32;
};

// Swizzle the components of a vector register and store it in a register (can be the same one).
// The swizzle pattern is 8 bit, 2 bits per component to select either x,y,z or w from src.
struct Swizzle {
    Opcode opcode : 5;
    RegisterSelection dest : 4;
    RegisterSelection src : 4;
    SwizzlePatternComponent patternX : 2;
    SwizzlePatternComponent patternY : 2;
    SwizzlePatternComponent patternZ : 2;
    SwizzlePatternComponent patternW : 2;
};

} // namespace InstructionLayouts

union Instruction {
    InstructionLayouts::Header header;
    InstructionLayouts::BinaryMath binaryMath;
    InstructionLayouts::UnaryMath unaryMath;
    InstructionLayouts::BinaryMathImm binaryMathImm;
    InstructionLayouts::Swizzle swizzle;
    uint32_t raw;
};
static_assert(sizeof(Instruction) == 8);

}; // namespace Isa
