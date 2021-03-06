#pragma once

#include "gpu/blocks/memory.h"
#include "gpu/blocks/memory_controller.h"
#include "gpu/blocks/output_merger.h"
#include "gpu/blocks/primitive_assembler.h"
#include "gpu/blocks/rasterizer.h"
#include "gpu/blocks/shader_array/shader_frontend.h"
#include "gpu/blocks/shader_array/shader_unit.h"
#include "gpu/blocks/user_blitter.h"
#include "gpu/blocks/vertex_shader.h"
#include "gpu/util/port_connector.h"

#include <systemc.h>

class VcdTrace;

SC_MODULE(Gpu) {
    constexpr static inline size_t memorySize = 21000;
    SC_HAS_PROCESS(Gpu);
    Gpu(sc_module_name name);

    // Blocks of the GPU
    UserBlitter userBlitter;               // abbreviation: BLT
    MemoryController<4> memoryController;  // abbreviation: MEMCTL
    Memory<memorySize> memory;             // abbreviation: MEM
    ShaderFrontend<1, 2> shaderFrontend;   // abbreviation: SF
    ShaderUnit shaderUnit0;                // abbreviation SU0
    ShaderUnit shaderUnit1;                // abbreviation SU1
    PrimitiveAssembler primitiveAssembler; // abbreviation: PA
    VertexShader vertexShader;             // abbreviation: VS
    Rasterizer rasterizer;                 // abbreviation: RS
    OutputMerger outputMerger;             // abbreviation: OM

    // This structure represents wirings of individual blocks visible to the
    // user. Ideally user should set all of the fields to desired values.
    struct {
        struct {
            sc_in_clk inpClock{"inpClock"};
        } GLOBAL;

        struct {
            sc_signal<bool> inpEnable{"PA_inpEnable"};
            sc_signal<MemoryAddressType> inpVerticesAddress{"PA_inpVerticesAddress"};
            sc_signal<sc_uint<8>> inpVerticesCount{"PA_inpVerticesCount"};
        } PA;

        struct {
            sc_signal<MemoryAddressType> inpShaderAddress{"VS_inpShaderAddress"};
        } VS;

        struct {
            sc_signal<VertexPositionFloatType> framebufferWidth{"RS_OM_framebufferWidth"};
            sc_signal<VertexPositionFloatType> framebufferHeight{"RS_OM_framebufferHeight"};
        } RS_OM;

        struct {
            sc_signal<MemoryAddressType> inpFramebufferAddress{"OM_inpFramebufferAddress"};
            sc_signal<bool> inpDepthEnable{"OM_inpDepthEnable"};
            sc_signal<MemoryAddressType> inpDepthBufferAddress{"OM_inpDepthBufferAddress"};
        } OM;
    } blocks;

    // This structure represents outputs signals to be read by user. The user
    // should not set any of these signals to any value and treat them as read-only.
    struct {
        sc_signal<bool> busy{"busy"};
    } out;

    void addSignalsToVcdTrace(VcdTrace & trace, bool publicPorts, bool internalPorts);
    void addProfilingSignalsToVcdTrace(VcdTrace & trace);

    void waitForIdle(const sc_clock &clock) const;

private:
    void connectClocks();
    void connectInternalPorts();
    void connectPublicPorts();
    void connectProfilingPorts();

    void setBusyValue();

    PortConnector ports;
    PortConnector profilingPorts;
};
