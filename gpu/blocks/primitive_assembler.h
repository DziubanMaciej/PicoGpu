#include "systemc.h"

SC_MODULE(PrimitiveAssembler) {
    sc_in_clk inpClock;
    sc_in<bool> inpIsNextBlockDone;
    sc_out<bool> outEnableNextBlock;
    sc_out<sc_uint<32>> outTriangleVertices[6];

    // TODO: currently we hardcode to input only one triangle
    bool worked = false;

    SC_CTOR(PrimitiveAssembler) {
        SC_CTHREAD(assemble, inpClock.pos());
    }

    void assemble();
};
