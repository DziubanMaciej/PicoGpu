#pragma once

#include "gpu/isa/isa.h"

#include <systemc.h>

constexpr inline size_t memoryDataTypeByteSize = 4;
using MemoryAddressType = sc_uint<32>;
using MemoryDataType = sc_uint<memoryDataTypeByteSize * 8>;

using VertexPositionFloatType = sc_uint<32>;   // 32-bit float saved as uint
using VertexPositionIntegerType = sc_uint<16>; // actual uint

constexpr inline size_t fragmentColorTypeByteSize = 4; // RGBA, 8 bits per channel
using FragmentColorType = sc_uint<fragmentColorTypeByteSize * 8>;

constexpr inline size_t depthTypeByteSize = 4;
using DepthType = sc_uint<depthTypeByteSize * 8>;

using PointerType = sc_uint<sizeof(void *) * 8>;

union VsPsCustomComponents {
    struct {
        uint32_t registersCount : 2;
        NonZeroCount comp0 : Isa::registerComponentsCountExponent;
        NonZeroCount comp1 : Isa::registerComponentsCountExponent;
        NonZeroCount comp2 : Isa::registerComponentsCountExponent;
    };
    uint32_t raw;

    VsPsCustomComponents(uint32_t raw) : raw(raw) {}

    uint32_t getCustomComponentsCount() const {
        const uint32_t registersCount = this->registersCount;
        uint32_t result = 0;
        if (registersCount > 0) {
            result += nonZeroCountToInt(this->comp0);
        }
        if (registersCount > 1) {
            result += nonZeroCountToInt(this->comp1);
        }
        if (registersCount > 2) {
            result += nonZeroCountToInt(this->comp2);
        }
        return result;
    }
};
static_assert(sizeof(VsPsCustomComponents) == sizeof(uint32_t));
constexpr inline size_t vsPsCustomComponentsBits = 2 + 3 * Isa::registerComponentsCountExponent;
using VsPsCustomComponentsType = sc_uint<vsPsCustomComponentsBits>;

constexpr static size_t verticesInPrimitive = 3;
