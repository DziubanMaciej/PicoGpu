#pragma once

#include <cstdint>

struct Conversions {
    static uint32_t floatBytesToUint(float arg) {
        return *reinterpret_cast<uint32_t *>(&arg);
    }

    static float uintBytesToFloat(uint32_t arg) {
        return *reinterpret_cast<float *>(&arg);
    }

    static float readFloat(sc_in<sc_uint<32>> &in) {
        uint32_t bytes = in.read().to_int();
        return uintBytesToFloat(bytes);
    }

    static void writeFloat(sc_out<sc_uint<32>> &out, float value) {
        uint32_t bytes = floatBytesToUint(value);
        out.write(bytes);
    }
};
