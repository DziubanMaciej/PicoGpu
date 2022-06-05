#include "systemc.h"

SC_MODULE(Rasterizer) {
    sc_in_clk inpClock;
    struct {
        sc_in<sc_uint<16>> inpWidth;
        sc_in<sc_uint<16>> inpHeight;
    } framebuffer;
    struct {
        sc_in<sc_uint<32>> inpTriangleVertices[6];
        sc_in<bool> inpEnable;
        sc_out<bool> outIsDone;
    } previousBlock;
    struct {
        sc_out<bool> outEnable;
        sc_fifo_out<sc_uint<32>> outPixels;
        sc_in<bool> inpIsDone;
    } nextBlock;

    SC_CTOR(Rasterizer) {
        SC_CTHREAD(rasterize, inpClock.pos());
    }

    void rasterize();
};
