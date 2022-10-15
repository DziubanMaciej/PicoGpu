#include "gpu/blocks/blitter.h"
#include "gpu/util/error.h"
#include "gpu/util/raii_boolean_setter.h"

void Blitter::blitToMemory(MemoryAddressType memoryPtr, uint32_t *userPtr, size_t sizeInDwords) {
    FATAL_ERROR_IF(pendingOperation.isValid, "Blitter has to be used sequentially");

    pendingOperation.isValid = true;
    pendingOperation.isFill = false;
    pendingOperation.toMemory = true;
    pendingOperation.memoryPtr = memoryPtr;
    pendingOperation.userPtr = userPtr;
    pendingOperation.sizeInDwords = sizeInDwords;
}

void Blitter::blitFromMemory(MemoryAddressType memoryPtr, uint32_t *userPtr, size_t sizeInDwords) {
    FATAL_ERROR_IF(pendingOperation.isValid, "Blitter has to be used sequentially");

    pendingOperation.isValid = true;
    pendingOperation.isFill = false;
    pendingOperation.toMemory = false;
    pendingOperation.memoryPtr = memoryPtr;
    pendingOperation.userPtr = userPtr;
    pendingOperation.sizeInDwords = sizeInDwords;
}

void Blitter::fillMemory(MemoryAddressType memoryPtr, uint32_t *userPtr, size_t sizeInDwords) {
    FATAL_ERROR_IF(pendingOperation.isValid, "Blitter has to be used sequentially");

    pendingOperation.isValid = true;
    pendingOperation.isFill = true;
    pendingOperation.toMemory = true;
    pendingOperation.memoryPtr = memoryPtr;
    pendingOperation.userPtr = userPtr;
    pendingOperation.sizeInDwords = sizeInDwords;
}

void Blitter::main() {
    while (true) {
        wait();

        if (!pendingOperation.isValid) {
            continue;
        }

        RaiiBooleanSetter busySetter{profiling.outBusy};

        for (size_t dwordIndex = 0; dwordIndex < pendingOperation.sizeInDwords; dwordIndex++) {
            outEnable = 1;
            outWrite = pendingOperation.toMemory;
            outAddress = pendingOperation.memoryPtr + 4 * dwordIndex;

            if (pendingOperation.toMemory) {
                const size_t userPtrIndex = pendingOperation.isFill ? 0 : dwordIndex;
                outData = pendingOperation.userPtr[userPtrIndex];
            }

            wait(1);
            outEnable = 0;

            while (!inpCompleted) {
                wait(1);
            }

            outWrite = 0;
            outAddress = 0;
            outData = 0;

            if (!pendingOperation.toMemory) {
                pendingOperation.userPtr[dwordIndex] = inpData.read();
            }
        }

        pendingOperation.isValid = false;
    }
}
