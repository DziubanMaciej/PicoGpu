#pragma once

#include "gpu/util/error.h"

#include <systemc.h>

template <typename PortT>
class RaiiBooleanSetter {
public:
    static_assert(std::is_same_v<PortT, sc_out<bool>> || std::is_same_v<PortT, sc_signal<bool>>);

    RaiiBooleanSetter(PortT &port) : port(port) {
        FATAL_ERROR_IF(port.read(), "Output port should be false");
        port = true;
    }

    ~RaiiBooleanSetter() {
        FATAL_ERROR_IF(!port.read(), "Output port should be true");
        port = false;
    }

private:
    PortT &port;
};
