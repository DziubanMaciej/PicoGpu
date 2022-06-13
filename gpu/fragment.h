#pragma once

#include "gpu/types.h"

#include <ostream>
#include <systemc.h>

struct ShadedFragment {
    VertexPositionIntegerType x;
    VertexPositionIntegerType y;
    VertexPositionFloatType z;
    FragmentColorType color;

    ShadedFragment &operator=(const ShadedFragment &rhs) {
        x = rhs.x;
        y = rhs.y;
        z = rhs.z;
        color = rhs.color;
        return *this;
    }

    bool operator==(const ShadedFragment &rhs) {
        return x == rhs.x && y == rhs.y && z == rhs.z && color == rhs.color;
    }
};

inline std::ostream &operator<<(std::ostream &os, const ShadedFragment &val) {
    return os
           << "x = " << val.x
           << "; y = " << val.y
           << "; z = " << val.z
           << "; color = " << val.color
           << std::endl;
}

inline void sc_trace(sc_trace_file *&f, const ShadedFragment &val, const std::string &name) {
    sc_trace(f, val.x, name + "_x");
    sc_trace(f, val.y, name + "_y");
    sc_trace(f, val.z, name + "_z");
    sc_trace(f, val.color, name + "_color");
}
