#pragma once

#include "gpu/util/log.h"

#define TESTER(name, _expectedSummaryCount)                                                                                                 \
    size_t summaryCount = 0;                                                                                                                \
    size_t summarySuccessCount = 0;                                                                                                         \
                                                                                                                                            \
    int verify() {                                                                                                                          \
        int fail = 0;                                                                                                                       \
        if (summaryCount < (_expectedSummaryCount)) {                                                                                       \
            Log() << name << ": FAILED! Incorrect summary count (" << summaryCount << "). Did the test ran simulation for too short time?"; \
            fail = 1;                                                                                                                       \
        } else if (summaryCount > (_expectedSummaryCount)) {                                                                                \
            Log() << name << ": FAILED! Incorrect summary count. Did you call the test too many times?";                                    \
            fail = 1;                                                                                                                       \
        } else if (summarySuccessCount != summaryCount) {                                                                                   \
            Log() << name << ": FAILED! Some checks failed.";                                                                               \
            fail = 1;                                                                                                                       \
        } else {                                                                                                                            \
            Log() << name << ": PASSED";                                                                                                    \
        }                                                                                                                                   \
        return fail;                                                                                                                        \
    }

#define ASSERT_EQ(a, b)                                                                                                                            \
    wait(SC_ZERO_TIME);                                                                                                                            \
    {                                                                                                                                              \
        if ((a) != (b)) {                                                                                                                          \
            success = false;                                                                                                                       \
            Log() << "ASSERT_EQ(" #a ", " #b ") at " << __FILE__ << ":" << __LINE__ << " failed (expected: " << (a) << ", actual: " << (b) << ")"; \
        }                                                                                                                                          \
    }

#define SUMMARY_RESULT(NAME)                                      \
    summaryCount++;                                               \
    if (success)                                                  \
        summarySuccessCount++;                                    \
    Log() << NAME << " " << ((success) ? "SUCCEEDED" : "FAILED"); \
    success = true;
