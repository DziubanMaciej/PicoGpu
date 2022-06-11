#pragma once

#include <iostream>
#include <mutex>
#include <sstream>
#include <systemc.h>

class LogBase {
protected:
    static inline std::mutex mutex;
    static inline std::unique_lock<std::mutex> lock{mutex, std::defer_lock};
    static inline sc_time lastTime{};
};

template <bool threadSafe = false, bool separateCycles = false>
struct Log : LogBase {
    Log() {
        if (threadSafe) {
            lock.lock();
        }

        sc_time currentTime = sc_time_stamp();
        if (separateCycles && currentTime != lastTime) {
            message << "\n";
            lastTime = currentTime;
        }
        message << "[" << currentTime << "]\t ";
    }

    ~Log() {
        if (threadSafe) {
            lock.unlock();
        }
        std::cout << message.str() << std::endl;
    }

    template <typename T>
    Log &operator<<(const T &arg) {
        message << arg;
        return *this;
    }

private:
    std::ostringstream message;
};
