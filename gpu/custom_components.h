#pragma once

#include "gpu/isa/isa.h"

#include <systemc.h>

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