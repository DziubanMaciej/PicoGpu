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

struct RaiiTimer {
    RaiiTimer(const char *format) : format(format) {
        timer.begin();
    }

    ~RaiiTimer() {
        std::string time = timer.end().to_string();
        printf(format, time.c_str());
    }

private:
    const char *format;
    Timer timer;
};
