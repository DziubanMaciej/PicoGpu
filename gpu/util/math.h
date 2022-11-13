#pragma once

#include <cstdint>

constexpr static inline size_t maxCustomComponents = (Isa::maxInputOutputRegisters - 1) * Isa::registerComponentsCount;
struct Point {
    float x;
    float y;
    float z;
    float w;
    float customComponents[maxCustomComponents];
};

inline float sign(Point p1, Point p2, Point p3) {
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

inline bool isPointInTriangle(Point pt, Point v1, Point v2, Point v3) {
    float d1, d2, d3;
    bool has_neg, has_pos;

    d1 = sign(pt, v1, v2);
    d2 = sign(pt, v2, v3);
    d3 = sign(pt, v3, v1);

    has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}

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
