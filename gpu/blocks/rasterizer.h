#pragma once

#include "gpu/fragment.h"

#include <systemc.h>

struct Point;

SC_MODULE(Rasterizer) {

    sc_in_clk inpClock;
    struct {
        sc_in<VertexPositionFloatType> inpWidth;
        sc_in<VertexPositionFloatType> inpHeight;
    } framebuffer;
    struct PreviousBlock {
        sc_in<bool> inpSending;
        sc_out<bool> outReceiving;
        constexpr static inline ssize_t portsCount = 12;
        sc_in<VertexPositionFloatType> inpData[portsCount];
    } previousBlock;
    struct {
        sc_in<bool> inpReceiving;
        sc_out<bool> outSending;
        sc_out<ShadedFragment> outData;
    } nextBlock;
    struct {
        sc_out<bool> outBusy;
        sc_out<sc_uint<32>> outFragmentsProduced;
    } profiling;

    SC_CTOR(Rasterizer) {
        SC_CTHREAD(rasterize, inpClock.pos());
    }

    void rasterize();

private:
    Point readPoint(const uint32_t *receivedVertices, size_t stride, size_t pointIndex);
    FragmentColorType randomizeColor();
};
