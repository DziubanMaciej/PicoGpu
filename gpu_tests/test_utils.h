#pragma once

#include "gpu/util/log.h"

#define TESTER(name, _expectedSummaryCount)                                                                           \
    size_t summaryCount = 0;                                                                                          \
    size_t summarySuccessCount = 0;                                                                                   \
                                                                                                                      \
    int verify() {                                                                                                    \
        int fail = 0;                                                                                                 \
        if (summaryCount != _expectedSummaryCount) {                                                                  \
            scLog() << name << ": FAILED! Incorrect summary count. Did the test ran simulation for to short time?\n"; \
            fail = 1;                                                                                                 \
        } else if (summarySuccessCount != summaryCount) {                                                             \
            scLog() << name << ": FAILED! Some checks failed.\n";                                                     \
            fail = 1;                                                                                                 \
        } else {                                                                                                      \
            scLog() << name << ": PASSED\n";                                                                          \
        }                                                                                                             \
        return fail;                                                                                                  \
    }

#define ASSERT_EQ(a, b)                                                                               \
    wait(SC_ZERO_TIME);                                                                               \
    {                                                                                                 \
        if ((a) != (b)) {                                                                             \
            success = false;                                                                          \
            scLog() << "ASSERT_EQ(" #a ", " #b ") at " << __FILE__ << ":" << __LINE__ << " failed\n"; \
        }                                                                                             \
    }

#define SUMMARY_RESULT(NAME)                                            \
    summaryCount++;                                                     \
    if (success)                                                        \
        summarySuccessCount++;                                          \
    scLog() << NAME << " " << ((success) ? "SUCCEEDED\n" : "FAILED\n"); \
    success = true;
