#pragma once

#include "gpu/isa/isa.h"

#include <systemc.h>

union CustomShaderComponents {
    struct {
        uint32_t registersCount : 3;
        NonZeroCount comp0 : Isa::registerComponentsCountExponent;
        NonZeroCount comp1 : Isa::registerComponentsCountExponent;
        NonZeroCount comp2 : Isa::registerComponentsCountExponent;
        NonZeroCount comp3 : Isa::registerComponentsCountExponent;
    };
    uint32_t raw;

    CustomShaderComponents(uint32_t raw) : raw(raw) {}

    // TOOD: rename to getTotalComponentsCount
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
        if (registersCount > 3) {
            result += nonZeroCountToInt(this->comp3);
        }
        return result;
    }
};
static_assert(sizeof(CustomShaderComponents) == sizeof(uint32_t));
constexpr inline size_t customShaderComponentsBits = 3 + 4 * Isa::registerComponentsCountExponent;
using CustomShaderComponentsType = sc_uint<customShaderComponentsBits>;

// TODO rethink memory layout of this. Currently it's made to fit the worst-case scenario, but:
// - input VS components: there can be 1,2,3 or 4 vectors
// - VS-PS components:    there can be 0,1,2 or 3 vectors
//
// So 2 bits will be sufficient for registersCount
// We also don't need comp3 for VS-PS components
