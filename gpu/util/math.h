#pragma once

#include <cstdint>

inline bool isBitSet(uint32_t bits, uint32_t bitNumber) {
    uint32_t mask = 1 << bitNumber;
    bits &= mask;
    return bits != 0;
}

inline int countBits(uint32_t number) {
    int count = 0;
    while (number) {
        count += number & 1;
        number >>= 1;
    }
    return count;
}

inline float saturate(float f) {
    if (f < 0) {
        return 0;
    } else if (f > 1) {
        return 1;
    } else {
        return f;
    }
}
