#pragma once

#include <cstdint>

template <typename Integral>
inline void setBit(Integral &bits, uint32_t bitNumber, bool set = true) {
    bits |= (1 << bitNumber);
}

template <typename Integral>
inline bool isBitSet(Integral bits, uint32_t bitNumber) {
    return (bits & (1 << bitNumber)) != 0;
}

template <typename Integral>
inline size_t countBits(Integral number) {
    size_t count = 0;
    while (number) {
        count += number & 1;
        number >>= 1;
    }
    return count;
}

template <typename Integral>
inline int32_t findBit(Integral bits, bool findSetBit, uint32_t startBit = 0) {
    // There are some super-performant platform-specific intrinsics for bit scanning, but this is fine too.

    const auto bitCount = sizeof(Integral) * 8;
    for (uint32_t bitIndex = startBit; bitIndex < bitCount; bitIndex++) {
        if (isBitSet(bits, bitIndex) == findSetBit) {
            return bitIndex;
        }
    }
    return -1;
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
