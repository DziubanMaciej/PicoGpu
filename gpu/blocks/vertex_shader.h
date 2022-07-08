#pragma once

#include "gpu/types.h"
#include "gpu/util/double_buffer.h"

#include <systemc.h>
#include <unordered_map>

SC_MODULE(VertexShader) {
    sc_in_clk inpClock;
    sc_in<MemoryAddressType> inpShaderAddress;

    struct PreviousBlock {
        sc_in<bool> inpSending;
        sc_out<bool> outReceiving;
        constexpr static inline size_t portsCount = 9;
        sc_in<VertexPositionFloatType> inpData[portsCount];
    } previousBlock;

    struct NextBlock {
        sc_in<bool> inpReceiving;
        sc_out<bool> outSending;
        constexpr static inline size_t portsCount = 9;
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
        SC_CTHREAD(processReceiveFromPreviousBlock, inpClock.pos());
        SC_CTHREAD(processSendForShading, inpClock.pos());
        SC_CTHREAD(processReceiveFromShading, inpClock.pos());
        SC_CTHREAD(processSendToNextBlock, inpClock.pos());
    }

private:
    void main();
    void processReceiveFromPreviousBlock();
    void processSendForShading();
    void processReceiveFromShading();
    void processSendToNextBlock();

    DoubleBuffer<uint32_t, 32> verticesBeforeShade;
    std::unordered_map<uint16_t, size_t> shadingTasks; // key is clientToken, value is thread count
    DoubleBuffer<uint32_t, 32> verticesAfterShade;
};
