#pragma once

#include <systemc.h>

struct Timer {

    void begin() {
        beginTime = sc_time_stamp();
    }

    sc_time end() {
        return sc_time_stamp() - beginTime;
    }

private:
    sc_time beginTime;
};
