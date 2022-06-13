#pragma once

#include "gpu/util/error.h"

struct VectorRegister {
    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t w;

    uint32_t &operator[](size_t index) {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        case 3:
            return w;
        default:
            FATAL_ERROR("Invalid argument to indexed VectorRegister access");
        }
    }
};
