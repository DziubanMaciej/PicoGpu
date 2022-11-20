#pragma once

#include "gpu/definitions/custom_components.h"
#include "gpu/definitions/unshaded_fragment.h"

#include <systemc.h>

SC_MODULE(Rasterizer) {
private:
    constexpr static inline size_t maxCustomVsPsComponents = (Isa::maxInputOutputRegisters - 1) * Isa::registerComponentsCount;
    struct Point {
        float x;
        float y;
        float z;
        float w;
        float customComponents[maxCustomVsPsComponents];
    };

public:
    sc_in_clk inpClock;
    sc_in<CustomShaderComponentsType> inpCustomVsPsComponents;

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
    struct NextBlock {
        struct PerTriangle {
            sc_in<bool> inpReceiving;
            sc_out<bool> outSending;
            constexpr static inline ssize_t portsCount = 3;
            sc_out<sc_uint<32>> outData[portsCount];
        } perTriangle;
        struct PerFragment {
            sc_in<bool> inpReceiving;
            sc_out<bool> outSending;
            sc_out<UnshadedFragment> outData;
        } perFragment;
    } nextBlock;
    struct {
        sc_out<bool> outBusy;
        sc_out<sc_uint<32>> outFragmentsProduced;
    } profiling;

    SC_CTOR(Rasterizer) {
        SC_CTHREAD(main, inpClock.pos());
    }

private:
    void main();
    void receiveFromVs(uint32_t customComponentsPerVertex, Point * outVertices);
    void setupPerTriangleFsState(uint32_t customComponentsPerVertex, Point * vertices);
    void rasterize(Point * vertices);

    static bool isPointInTriangle(Point pt, Point v1, Point v2, Point v3);
    static float sign(Point p1, Point p2, Point p3);
    Point readPoint(const uint32_t *receivedVertices, size_t stride, size_t customComponentsCount, size_t pointIndex);
};
