#pragma once

#include "gpu/types.h"
#include "gpu/fragment.h"

#include <systemc.h>

SC_MODULE(FragmentShader) {
    sc_in_clk inpClock;
    sc_in<MemoryAddressType> inpShaderAddress;

    struct PreviousBlock{
        sc_out<bool> outReceiving;
        sc_in<bool> inpSending;
        sc_in<UnshadedFragment> inpData;
    } previousBlock;

    struct NextBlock {
        sc_in<bool> inpReceiving;
        sc_out<bool> outSending;
        sc_out<ShadedFragment> outData;
    } nextBlock;

    struct {
        struct {
            sc_out<bool> outSending;
            sc_out<sc_uint<32>> outData;
            sc_in<bool> inpReceiving;
        } request;
        struct {
            sc_out<bool> outReceiving;
            sc_in<bool> inpSending;
            sc_in<sc_uint<32>> inpData;
        } response;
    } shaderFrontend;

    struct {
        sc_out<bool> outBusy;
    } profiling;

    SC_CTOR(FragmentShader) {
        SC_CTHREAD(main, inpClock.pos());
    }

private:
    void main();
    static uint32_t packRgbaToUint(float *rgba);
};
