#pragma once

#include <iostream>

[[noreturn]] inline void performAbort() {
    throw std::exception{};
}

inline void dumpLog(std::ostream &out) {
    out << std::endl;
}

template <typename Arg, typename... Args>
inline void dumpLog(std::ostream &out, Arg &&arg, Args &&...args) {
    out << arg;
    dumpLog(out, std::forward<Args>(args)...);
}

#define FATAL_ERROR(...)             \
    dumpLog(std::cerr, __VA_ARGS__); \
    performAbort();

#define FATAL_ERROR_IF(condition, ...) \
    if (condition) {                   \
        FATAL_ERROR(__VA_ARGS__);      \
    }

#ifdef _DEBUG
#define DEBUG_ERROR(...) FATAL_ERROR(__VA_ARGS__)
#define DEBUG_ERROR_IF(condition, ...) FATAL_ERROR_IF(condition, __VA_ARGS__)
#else
#define DEBUG_ERROR(...)
#define DEBUG_ERROR_IF(condition, ...)
#endif

#define UNREACHABLE_CODE FATAL_ERROR("Unreachable code")
