#pragma once

#include "gpu/definitions/custom_components.h"
#include "gpu/definitions/types.h"

#include <systemc.h>

SC_MODULE(VertexShader) {
    sc_in_clk inpClock;
    sc_in<MemoryAddressType> inpShaderAddress;
    sc_in<CustomShaderComponentsType> inpCustomInputComponents;
    sc_in<CustomShaderComponentsType> inpCustomOutputComponents;

    struct PreviousBlock {
        sc_in<bool> inpSending;
        sc_out<bool> outReceiving;
        constexpr static inline size_t portsCount = 9;
        sc_in<VertexPositionFloatType> inpData[portsCount];
    } previousBlock;

    struct NextBlock {
        sc_in<bool> inpReceiving;
        sc_out<bool> outSending;
        constexpr static inline size_t portsCount = 12;
        sc_out<VertexPositionFloatType> outData[portsCount];
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

    SC_CTOR(VertexShader) {
        SC_CTHREAD(main, inpClock.pos());
    }

private:
    void main();
};
