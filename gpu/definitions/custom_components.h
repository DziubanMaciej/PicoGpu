#pragma once

#include "gpu/isa/isa.h"
#include "gpu/util/error.h"

#include <systemc.h>

union CustomShaderComponents {
    constexpr static inline size_t registersCountBits = 2;
    constexpr static inline size_t structBits = registersCountBits + Isa::maxInputOutputRegisters * Isa::registerComponentsCountExponent;

    struct {
        uint32_t registersCount : registersCountBits;
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
using CustomShaderComponentsType = sc_uint<CustomShaderComponents::structBits>;
