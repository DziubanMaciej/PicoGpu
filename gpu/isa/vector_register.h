#pragma once

#include "gpu/util/error.h"

struct VectorRegister {
    int32_t x;
    int32_t y;
    int32_t z;
    int32_t w;

    int32_t &operator[](size_t index) {
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

    const int32_t &operator[](size_t index) const {
        VectorRegister &nonConst = *const_cast<VectorRegister *>(this);
        return nonConst[index];
    }
};
