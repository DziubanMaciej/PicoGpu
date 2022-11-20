#pragma once

#include "gpu/definitions/types.h"

#include <ostream>
#include <systemc.h>

struct UnshadedFragment {
    VertexPositionIntegerType x;
    VertexPositionIntegerType y;

    UnshadedFragment &operator=(const UnshadedFragment &rhs) {
        x = rhs.x;
        y = rhs.y;
        return *this;
    }

    bool operator==(const UnshadedFragment &rhs) {
        return x == rhs.x && y == rhs.y;
    }
};

inline std::ostream &operator<<(std::ostream &os, const UnshadedFragment &val) {
    return os
           << "x = " << val.x
           << "; y = " << val.y
           << std::endl;
}

inline void sc_trace(sc_trace_file *&f, const UnshadedFragment &val, const std::string &name) {
    sc_trace(f, val.x, name + "_x");
    sc_trace(f, val.y, name + "_y");
}
