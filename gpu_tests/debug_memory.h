#pragma once

#include "gpu/blocks/memory.h"

template <unsigned int Size>
struct DebugMemory : Memory<Size> {
    using Memory<Size>::Memory;

    void blitToMemory(MemoryAddressType memoryPtr, uint32_t *userPtr, size_t sizeInDwords) {
        for (size_t dwordIndex = 0; dwordIndex < sizeInDwords; dwordIndex++) {
            const size_t memoryIndex = memoryPtr / 4 + dwordIndex;
            const size_t userPtrIndex = dwordIndex;
            this->rawMemory[memoryIndex] = userPtr[userPtrIndex];
        }
    }

    void blitFromMemory(MemoryAddressType memoryPtr, uint32_t *userPtr, size_t sizeInDwords) {
        for (size_t dwordIndex = 0; dwordIndex < sizeInDwords; dwordIndex++) {
            const size_t memoryIndex = memoryPtr / 4 + dwordIndex;
            const size_t userPtrIndex = dwordIndex;
            userPtr[userPtrIndex] = this->rawMemory[memoryIndex];
        }
    }
};
