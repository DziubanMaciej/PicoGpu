#pragma once

#include "gpu/isa/isa.h"
#include "gpu/util/error.h"

#include <systemc.h>

union CustomShaderComponents {
    struct {
        uint32_t registersCount : 3;
        NonZeroCount comp0 : Isa::registerComponentsCountExponent;
        NonZeroCount comp1 : Isa::registerComponentsCountExponent;
        NonZeroCount comp2 : Isa::registerComponentsCountExponent;
    };
    uint32_t raw;

    CustomShaderComponents(uint32_t raw) : raw(raw) {}

    uint32_t getTotalCustomComponents() const {
        const uint32_t registersCount = this->registersCount;
        uint32_t result = 0;
        for (uint32_t registerIndex = 0; registerIndex < registersCount; registerIndex++) {
            result += getCustomComponents(registerIndex);
        }
        return result;
    }

    uint32_t getCustomComponents(uint32_t registerIndex) const {
        switch (registerIndex) {
        case 0:
            return nonZeroCountToInt(comp0);
        case 1:
            return nonZeroCountToInt(comp1);
        case 2:
            return nonZeroCountToInt(comp2);
        default:
            FATAL_ERROR("Invalid registerIndex");
        }
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
