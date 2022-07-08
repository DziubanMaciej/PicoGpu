#pragma once

#include "gpu/util/error.h"

#include <cstddef>

#define AAA(message) printf("------------ buffer%d: %s   (states: [%d, %d])\n", i, message, int(bufferStates[0]), int(bufferStates[1]));
//#define AAA(message)

template <typename T, size_t size>
class DoubleBuffer {
    enum class BufferState {
        None = 0,
        Write = 1,
        Written = 2,
        Read = 3,
    };

public:
    constexpr static inline size_t buffersCount = 2;
    constexpr static inline size_t bufferSize = size;

    T *acquireWriteBuffer(size_t minSizeLeft) {
        for (BufferState state : {BufferState::Written, BufferState::None}) {
            for (size_t i = 0u; i < buffersCount; i++) {
                if (bufferStates[i] == state && bufferSize - bufferWrittenSizes[i] >= minSizeLeft) {
                    bufferStates[i] = BufferState::Write;
                    AAA("To write");
                    return buffers[i] + bufferWrittenSizes[i];
                }
            }
        }
        return nullptr;
    }

    void releaseWriteBuffer(T *buffer, size_t elementsWritten) {
        for (size_t i = 0u; i < buffersCount; i++) {
            if (buffer == buffers[i] + bufferWrittenSizes[i]) {
                FATAL_ERROR_IF(bufferStates[i] != BufferState::Write, "Buffer should be in Write state");
                FATAL_ERROR_IF(bufferWrittenSizes[i] + elementsWritten > bufferSize, "Buffer size exceeded");
                bufferStates[i] = BufferState::Written;
                bufferWrittenSizes[i] += elementsWritten;
                AAA("To written");
            }
        }
    }

    const T *acquireReadBuffer(size_t minWrittenSize, size_t &outWrittenSize) {
        for (size_t i = 0u; i < buffersCount; i++) {
            if (bufferStates[i] == BufferState::Written && bufferWrittenSizes[i] >= minWrittenSize) {
                bufferStates[i] = BufferState::Read;
                outWrittenSize = bufferWrittenSizes[i];
                bufferWrittenSizes[i] = 0;
                AAA("To Read");
                return buffers[i];
            }
        }
        return nullptr;
    }

    void releaseReadBuffer(const T *buffer) {
        for (size_t i = 0u; i < buffersCount; i++) {
            if (buffer == buffers[i]) {
                FATAL_ERROR_IF(bufferStates[i] != BufferState::Read, "Buffer should be in Read state");
                bufferStates[i] = BufferState::None;
                AAA("To None");
            }
        }
    }

private:
    T buffers[size][buffersCount] = {};
    BufferState bufferStates[buffersCount] = {BufferState::None, BufferState::None};
    size_t bufferWrittenSizes[buffersCount] = {};
};
