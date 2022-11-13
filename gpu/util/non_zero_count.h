#pragma once

#include <cstdint>

enum class NonZeroCount : uint32_t {
    One = 0,
    Two = 1,
    Three = 2,
    Four = 3,
};

constexpr inline NonZeroCount intToNonZeroCount(int arg) { return NonZeroCount(arg - 1); }
constexpr inline int nonZeroCountToInt(NonZeroCount arg) { return int(arg) + 1; }
constexpr inline NonZeroCount operator+(NonZeroCount a, NonZeroCount b) {
    return intToNonZeroCount(nonZeroCountToInt(a) + nonZeroCountToInt(b));
}
