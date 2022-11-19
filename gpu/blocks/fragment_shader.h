#pragma once

#include "gpu/custom_components.h"
#include "gpu/fragment.h"
#include "gpu/isa/isa.h"
#include "gpu/types.h"

#include <systemc.h>

SC_MODULE(FragmentShader) {
    sc_in_clk inpClock;
    sc_in<MemoryAddressType> inpShaderAddress;
    sc_in<CustomShaderComponentsType> inpCustomInputComponents;

    struct PreviousBlock {
        struct PerTriangle {
            sc_out<bool> outReceiving;
            sc_in<bool> inpSending;
            constexpr static inline ssize_t portsCount = 3;
            sc_in<sc_uint<32>> inpData[portsCount];
        } perTriangle;
        struct PerFragment {
            sc_out<bool> outReceiving;
            sc_in<bool> inpSending;
            sc_in<UnshadedFragment> inpData;
        } perFragment;
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
        SC_CTHREAD(perTriangleThread, inpClock.pos());
        SC_CTHREAD(perFragmentThread, inpClock.pos());
    }

private:
    void perTriangleThread();
    void perFragmentThread();

    static uint32_t packRgbaToUint(float *rgba);
    static uint32_t calculateTriangleAttributesCount(CustomShaderComponents customComponents);

    // Passed from perTriangleThread to perFragmentThread
    constexpr static size_t maxPerTriangleAttribsCount = Isa::maxInputOutputRegisters * Isa::registerComponentsCount * 3;
    sc_signal<sc_uint<32>> perTriangleAttribs[maxPerTriangleAttribsCount] = {}; // values for all attribs for all 3 vertices of a triangle
};
