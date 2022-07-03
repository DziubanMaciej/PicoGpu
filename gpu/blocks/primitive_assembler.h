#pragma once

#include "gpu/types.h"

#include <systemc.h>

SC_MODULE(PrimitiveAssembler) {
    sc_in_clk inpClock;
    sc_in<bool> inpEnable;
    sc_in<MemoryAddressType> inpVerticesAddress;
    sc_in<sc_uint<8>> inpVerticesCount;

    struct {
        sc_out<bool> outEnable;
        sc_out<MemoryAddressType> outAddress;
        sc_in<MemoryDataType> inpData;
        sc_in<bool> inpCompleted;
    } memory;

    struct NextBlock {
        sc_in<bool> inpReceiving;
        sc_out<bool> outSending;
        constexpr static inline ssize_t portsCount = 9;
        sc_out<VertexPositionFloatType> outData[portsCount];
    } nextBlock;

    struct {
        sc_out<bool> outBusy;
        sc_out<sc_uint<32>> outPrimitivesProduced;
    } profiling;

    SC_CTOR(PrimitiveAssembler) {
        SC_CTHREAD(assemble, inpClock.pos());
    }

    void assemble();
};
