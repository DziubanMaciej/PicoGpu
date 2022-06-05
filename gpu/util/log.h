#pragma once

#include <iostream>
#include <systemc>

inline std::ostream &scLog() {
    static sc_time lastTime{};
    sc_time currentTime = sc_time_stamp();
    if (currentTime != lastTime) {
        std::cout << "\n";
        lastTime = currentTime;
    }
    std::cout << currentTime << "\t";
    return std::cout;
}
