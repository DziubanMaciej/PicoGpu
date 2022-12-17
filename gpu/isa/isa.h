#pragma once

#include "gpu/util/non_zero_count.h"

#include <cstddef>
#include <cstdint>

namespace Isa {

// Define general constants used during further definition of the ISA as well as by
// components using it.
constexpr inline size_t opcodeBitsize = 6;
constexpr inline size_t simdExponent = 5;
constexpr inline size_t simdSize = 1 << simdExponent;
constexpr inline size_t maxIsaSizeExponent = 9;
constexpr inline size_t maxIsaSize = 1 << maxIsaSizeExponent;
constexpr inline size_t generalPurposeRegistersCountExponent = 4;
constexpr inline size_t generalPurposeRegistersCount = 1 << generalPurposeRegistersCountExponent;
constexpr inline size_t maxInputOutputRegistersExponent = 2;
constexpr inline size_t maxInputOutputRegisters = 1 << maxInputOutputRegistersExponent;
constexpr inline size_t registerComponentsCountExponent = 2;
constexpr inline size_t registerComponentsCount = 1 << 2;
constexpr inline size_t commandSizeInDwords = 3;

using RegisterIndex = uint32_t;

// Commands are macro-operations issued to the shader units in order to prepare and
// execute shaders.
namespace Command {
    enum class CommandType : uint32_t {
        StoreIsa = 0,
        ExecuteIsa = 1,
    };

    enum class ProgramType : uint32_t {
        VertexShader = 0,
        FragmentShader = 1,
    };

    union CommandDummy {
        struct {
            CommandType commandType : 1;
            uint32_t hasNextCommand : 1;
        };
        uint32_t raw[commandSizeInDwords];
    };

    union CommandStoreIsa {
        struct {
            CommandType commandType : 1; // must be CommandType::StoreIsa
            uint32_t hasNextCommand : 1;
            uint32_t programLength : maxIsaSizeExponent;
            ProgramType programType : 1;

            NonZeroCount inputsCount : maxInputOutputRegistersExponent;              // the number valid input registers from 1 to 4.
            NonZeroCount inputSize0 : registerComponentsCountExponent;               // the number of components of first input register that will have a meaningful value.
            RegisterIndex inputRegister0 : generalPurposeRegistersCountExponent; // the register index of first input register
            NonZeroCount inputSize1 : registerComponentsCountExponent;
            RegisterIndex inputRegister1 : generalPurposeRegistersCountExponent;
            NonZeroCount inputSize2 : registerComponentsCountExponent;
            RegisterIndex inputRegister2 : generalPurposeRegistersCountExponent;
            NonZeroCount inputSize3 : registerComponentsCountExponent;
            RegisterIndex inputRegister3 : generalPurposeRegistersCountExponent;

            NonZeroCount outputsCount : maxInputOutputRegistersExponent;
            NonZeroCount outputSize0 : registerComponentsCountExponent;
            RegisterIndex outputRegister0 : generalPurposeRegistersCountExponent;
            NonZeroCount outputSize1 : registerComponentsCountExponent;
            RegisterIndex outputRegister1 : generalPurposeRegistersCountExponent;
            NonZeroCount outputSize2 : registerComponentsCountExponent;
            RegisterIndex outputRegister2 : generalPurposeRegistersCountExponent;
            NonZeroCount outputSize3 : registerComponentsCountExponent;
            RegisterIndex outputRegister3 : generalPurposeRegistersCountExponent;

            uint32_t uniformsCount : 3;
            NonZeroCount uniformSize0 : registerComponentsCountExponent;
            RegisterIndex uniformRegister0 : generalPurposeRegistersCountExponent;
            NonZeroCount uniformSize1 : registerComponentsCountExponent;
            RegisterIndex uniformRegister1 : generalPurposeRegistersCountExponent;
            NonZeroCount uniformSize2 : registerComponentsCountExponent;
            RegisterIndex uniformRegister2 : generalPurposeRegistersCountExponent;
            NonZeroCount uniformSize3 : registerComponentsCountExponent;
            RegisterIndex uniformRegister3 : generalPurposeRegistersCountExponent;
        };
        uint32_t raw[commandSizeInDwords];
    };

    union CommandExecuteIsa {
        struct {
            CommandType commandType : 1; // must be CommandType::Execute
            uint32_t hasNextCommand : 1;
            NonZeroCount threadCount : simdExponent;
        };
        uint32_t raw[commandSizeInDwords];
    };

    union Command {
        CommandDummy dummy;
        CommandStoreIsa storeIsa;
        CommandExecuteIsa executeIsa;
    };
    static_assert(sizeof(Command) == commandSizeInDwords * sizeof(uint32_t));

} // namespace Command

enum class Opcode : uint32_t {
    // Float math
    fadd,
    fadd_imm,
    fsub,
    fsub_imm,
    fmul,
    fmul_imm,
    fdiv,
    fdiv_imm,
    fneg,
    fdot,
    fcross,
    fcross2,
    fmad,
    frcp,
    fnorm,
    fmax,
    fmin,

    // Integer math
    iadd,
    iadd_imm,
    isub,
    isub_imm,
    imul,
    imul_imm,
    idiv,
    idiv_imm,
    ineg,
    imax,
    imin,

    // Misc
    init,
    swizzle,
    mov,
    trap,

    // Internal
    lduni,    // load uniform values to registers
    initregs, // initialize unused register to 0

    // control value
    COUNT,
};
static_assert(static_cast<uint32_t>(Opcode::COUNT) < (1 << opcodeBitsize), "Too many ops for selected opcode bit size");

enum class SwizzlePatternComponent : uint32_t {
    SwizzleX = 0b00,
    SwizzleY = 0b01,
    SwizzleZ = 0b10,
    SwizzleW = 0b11,
};

namespace InstructionLayouts {
    // This is not an actual instruction layout, but just some type that can be used to extract
    // the opcode.
    struct Header {
        Opcode opcode : opcodeBitsize;
    };

    // An operation without any arguments
    struct Nullary {
        Opcode opcode : opcodeBitsize;
    };

    // Any operation, that can take a register, compute something and output to a register
    // Destination is masked, which means we can select which channels of the vector registers
    // will be affected (1 bit per channel)
    struct UnaryMath {
        Opcode opcode : opcodeBitsize;
        RegisterIndex dest : generalPurposeRegistersCountExponent;
        RegisterIndex src : generalPurposeRegistersCountExponent;
        uint32_t destMask : 4;
    };

    // Any operation, that can take two registers, compute something and output to a register
    // Destination is masked, which means we can select which channels of the vector registers
    // will be affected (1 bit per channel)
    struct BinaryMath {
        Opcode opcode : opcodeBitsize;
        RegisterIndex dest : generalPurposeRegistersCountExponent;
        RegisterIndex src1 : generalPurposeRegistersCountExponent;
        RegisterIndex src2 : generalPurposeRegistersCountExponent;
        uint32_t destMask : 4;
    };

    // Any operation, that can take three registers, compute something and output to a register
    // Destination is masked, which means we can select which channels of the vector registers
    // will be affected (1 bit per channel)
    struct TernaryMath {
        Opcode opcode : opcodeBitsize;
        RegisterIndex dest : generalPurposeRegistersCountExponent;
        RegisterIndex src1 : generalPurposeRegistersCountExponent;
        RegisterIndex src2 : generalPurposeRegistersCountExponent;
        RegisterIndex src3 : generalPurposeRegistersCountExponent;
        uint32_t destMask : 4;
    };

    // Any operation, that can take one or more immediate values, compute something and output
    // to a register. Destination is masked, which means we can select which channels of the vector register
    // will be affected (1 bit per channel). ImmediateValues field is a one-element array, but the instruction
    // can actually take more bytes if immediateValuesCount is greater than 1.
    struct UnaryMathImm {
        Opcode opcode : opcodeBitsize;
        RegisterIndex dest : generalPurposeRegistersCountExponent;
        uint32_t destMask : 4;
        NonZeroCount immediateValuesCount : 2;
        uint32_t reserved : 16;
        uint32_t immediateValues[1];
    };

    // Any operation, that can take a register and one or more immediate values, compute something and output
    // to a register. Destination is masked, which means we can select which channels of the vector registers
    // will be affected (1 bit per channel). ImmediateValues field is a one-element array, but the instruction
    // can actually take more bytes if immediateValuesCount is greater than 1.
    struct BinaryMathImm {
        Opcode opcode : opcodeBitsize;
        RegisterIndex dest : generalPurposeRegistersCountExponent;
        RegisterIndex src : generalPurposeRegistersCountExponent;
        uint32_t destMask : 4;
        NonZeroCount immediateValuesCount : 2;
        uint32_t reserved : 12;
        uint32_t immediateValues[1];
    };

    // Swizzle the components of a vector register and store it in a register (can be the same one).
    // The swizzle pattern is 8 bit, 2 bits per component to select either x,y,z or w from src.
    struct Swizzle {
        Opcode opcode : opcodeBitsize;
        RegisterIndex dest : generalPurposeRegistersCountExponent;
        RegisterIndex src : generalPurposeRegistersCountExponent;
        SwizzlePatternComponent patternX : 2;
        SwizzlePatternComponent patternY : 2;
        SwizzlePatternComponent patternZ : 2;
        SwizzlePatternComponent patternW : 2;
    };
} // namespace InstructionLayouts

union Instruction {
    InstructionLayouts::Header header;
    InstructionLayouts::Nullary nullary;
    InstructionLayouts::BinaryMath binaryMath;
    InstructionLayouts::UnaryMath unaryMath;
    InstructionLayouts::TernaryMath ternaryMath;
    InstructionLayouts::UnaryMathImm unaryMathImm;
    InstructionLayouts::BinaryMathImm binaryMathImm;
    InstructionLayouts::Swizzle swizzle;
    uint32_t raw;
};
static_assert(sizeof(Instruction) == 8);

}; // namespace Isa
