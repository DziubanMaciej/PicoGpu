#include "gpu/blocks/user_blitter.h"

void UserBlitter::blitToMemory(MemoryAddressType memoryPtr, uint32_t *userPtr, size_t sizeInDwords) {
    // TODO add check for isValid

    pendingOperation.isValid = true;
    pendingOperation.toMemory = true;
    pendingOperation.memoryPtr = memoryPtr;
    pendingOperation.userPtr = userPtr;
    pendingOperation.sizeInDwords = sizeInDwords;
}

void UserBlitter::blitFromMemory(MemoryAddressType memoryPtr, uint32_t *userPtr, size_t sizeInDwords) {
    // TODO add check for isValid

    pendingOperation.isValid = true;
    pendingOperation.toMemory = false;
    pendingOperation.memoryPtr = memoryPtr;
    pendingOperation.userPtr = userPtr;
    pendingOperation.sizeInDwords = sizeInDwords;
}

void UserBlitter::main() {
    while (true) {
        wait();

        if (!pendingOperation.isValid) {
            continue;
        }

        for (size_t dwordIndex = 0; dwordIndex < pendingOperation.sizeInDwords; dwordIndex++) {
            outEnable = 1;
            outWrite = pendingOperation.toMemory;
            outAddress = pendingOperation.memoryPtr + 4 * dwordIndex;
            if (pendingOperation.toMemory) {
                outData = pendingOperation.userPtr[dwordIndex];
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
