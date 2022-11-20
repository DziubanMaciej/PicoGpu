#pragma once

#include "gpu/definitions/shaded_fragment.h"
#include "gpu/definitions/types.h"

#include <systemc.h>

SC_MODULE(OutputMerger) {
    sc_in_clk inpClock;
    struct {
        sc_out<bool> outReceiving;
        sc_in<bool> inpSending;
        sc_in<ShadedFragment> inpData;
    } previousBlock;
    struct {
        sc_in<MemoryAddressType> inpAddress;
        sc_in<VertexPositionFloatType> inpWidth;
        sc_in<VertexPositionFloatType> inpHeight;
    } framebuffer;
    struct {
        sc_in<bool> inpEnable;
        sc_in<MemoryAddressType> inpAddress;
    } depth;
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

    SC_CTOR(OutputMerger) {
        SC_CTHREAD(main, inpClock.pos());
    }

    void main();
};
