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

    struct {
        sc_in<bool> inpIsDone;
        sc_out<bool> outEnable;
        sc_out<VertexPositionType> outTriangleVertices[9];
    } nextBlock;

    SC_CTOR(PrimitiveAssembler) {
        SC_CTHREAD(assemble, inpClock.pos());
    }

    void assemble();
};
