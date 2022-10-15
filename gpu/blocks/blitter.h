#pragma once

#include "memory.h"

#include <systemc.h>

SC_MODULE(Blitter) {
    sc_in_clk inpClock;
    sc_out<bool> outEnable;
    sc_out<bool> outWrite;
    sc_out<MemoryAddressType> outAddress;
    sc_out<MemoryDataType> outData;
    sc_in<MemoryDataType> inpData;
    sc_in<bool> inpCompleted;

    struct {
        sc_out<bool> outBusy;
    } profiling;

    SC_CTOR(Blitter) {
        SC_CTHREAD(main, inpClock.pos());
    }

    void blitToMemory(MemoryAddressType memoryPtr, uint32_t * userPtr, size_t sizeInDwords);
    void blitFromMemory(MemoryAddressType memoryPtr, uint32_t * userPtr, size_t sizeInDwords);
    void fillMemory(MemoryAddressType memoryPtr, uint32_t * userPtr, size_t sizeInDwords);

    void main();
    bool hasPendingOperation() const { return pendingOperation.isValid; }

private:
    struct BlitOperation {
        bool isValid = false;
        bool isFill = false;
        bool toMemory = false;
        uint32_t *userPtr = nullptr;
        MemoryAddressType memoryPtr = 0;
        size_t sizeInDwords = 0;
    } pendingOperation = {};
};
