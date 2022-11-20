#pragma once

#include "gpu/definitions/types.h"

#include <systemc.h>

template <unsigned int Size = 1>
SC_MODULE(Memory) {
    sc_in_clk inpClock;
    sc_in<bool> inpEnable;
    sc_in<bool> inpWrite;
    sc_in<MemoryAddressType> inpAddress;
    sc_in<MemoryDataType> inpData;
    sc_out<MemoryDataType> outData;
    sc_out<bool> outCompleted;

    SC_CTOR(Memory) {
        SC_CTHREAD(work, inpClock.pos());
    }

    void work();

protected:
    sc_signal<MemoryDataType> rawMemory[Size];
};

template <unsigned int Size>
void Memory<Size>::work() {
    while (true) {
        wait();

        outCompleted.write(0);
        outData.write(0);

        if (!inpEnable.read()) {
            continue;
        }

        const auto addr = inpAddress.read().to_int() / memoryDataTypeByteSize;
        if (inpWrite.read()) {
            rawMemory[addr].write(inpData.read());
        } else {
            outData.write(rawMemory[addr].read());
        }

        outCompleted.write(1);
    }
}
