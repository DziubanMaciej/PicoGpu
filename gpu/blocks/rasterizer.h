#pragma once

#include "gpu/fragment.h"

#include <systemc.h>

SC_MODULE(Rasterizer) {
    sc_in_clk inpClock;
    struct {
        sc_in<VertexPositionFloatType> inpWidth;
        sc_in<VertexPositionFloatType> inpHeight;
    } framebuffer;
    struct {
        sc_in<VertexPositionFloatType> inpTriangleVertices[9];
        sc_in<bool> inpEnable;
        sc_out<bool> outIsDone;
    } previousBlock;
    struct {
        sc_in<bool> inpIsReceiving;
        sc_out<bool> outIsSending;
        sc_out<ShadedFragment> outFragment;
    } nextBlock;

    SC_CTOR(Rasterizer) {
        SC_CTHREAD(rasterize, inpClock.pos());
    }

    void rasterize();

private:
    FragmentColorType randomizeColor();
};
