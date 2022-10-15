#pragma once

#include "memory.h"

#include "gpu/types.h"

#include <systemc.h>

SC_MODULE(Blitter) {
    sc_in_clk inpClock;

    enum class CommandType {
        None,
        CopyToMem,
        CopyFromMem,
        FillMem,
    };

    struct {
        sc_in<sc_uint<2>> inpCommandType;
        sc_in<MemoryAddressType> inpMemoryPtr;
        sc_in<PointerType> inpUserPtr;
        sc_in<sc_uint<16>> inpSizeInDwords;
    } command;

    struct {
        sc_out<bool> outEnable;
        sc_out<bool> outWrite;
        sc_out<MemoryAddressType> outAddress;
        sc_out<MemoryDataType> outData;
        sc_in<MemoryDataType> inpData;
        sc_in<bool> inpCompleted;
    } memory;

    struct {
        sc_out<bool> outBusy;
    } profiling;

    SC_CTOR(Blitter) {
        SC_CTHREAD(main, inpClock.pos());
    }

private:
    void main();
};
