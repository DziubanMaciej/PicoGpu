#pragma once

#include "gpu/blocks/blitter.h"
#include "gpu/blocks/command_streamer.h"
#include "gpu/blocks/fragment_shader.h"
#include "gpu/blocks/memory.h"
#include "gpu/blocks/memory_controller.h"
#include "gpu/blocks/output_merger.h"
#include "gpu/blocks/primitive_assembler.h"
#include "gpu/blocks/rasterizer.h"
#include "gpu/blocks/shader_array/shader_frontend.h"
#include "gpu/blocks/shader_array/shader_unit.h"
#include "gpu/blocks/vertex_shader.h"
#include "gpu/util/port_connector.h"

#include <systemc.h>

class VcdTrace;

SC_MODULE(Gpu) {
    constexpr static inline size_t memorySize = 21000;
    SC_HAS_PROCESS(Gpu);
    Gpu(sc_module_name name, sc_clock &clock);

    // Blocks of the GPU
    CommandStreamer commandStreamer;       // abbreviation: CS
    Blitter blitter;                       // abbreviation: BLT
    MemoryController<4> memoryController;  // abbreviation: MEMCTL
    Memory<memorySize> memory;             // abbreviation: MEM
    ShaderFrontend<2, 2> shaderFrontend;   // abbreviation: SF
    ShaderUnit shaderUnit0;                // abbreviation SU0
    ShaderUnit shaderUnit1;                // abbreviation SU1
    PrimitiveAssembler primitiveAssembler; // abbreviation: PA
    VertexShader vertexShader;             // abbreviation: VS
    Rasterizer rasterizer;                 // abbreviation: RS
    FragmentShader fragmentShader;         // abbreviation: FS
    OutputMerger outputMerger;             // abbreviation: OM

    // This structure represents wirings of individual blocks visible to the
    // user. Ideally user should set all of the fields to desired values.
    struct {
        struct {
            sc_signal<CustomShaderComponentsType> vsCustomInputComponents{"GLOBAL_vsCustomInputComponents"};
            sc_signal<CustomShaderComponentsType> vsPsCustomComponents{"GLOBAL_vsPsCustomComponents"};
            sc_signal<VertexPositionFloatType> framebufferWidth{"GLOBAL_framebufferWidth"};
            sc_signal<VertexPositionFloatType> framebufferHeight{"GLOBAL_framebufferHeight"};
        } GLOBAL;

        struct {
            sc_signal<MemoryAddressType> verticesAddress{"PA_verticesAddress"};
            sc_signal<sc_uint<8>> verticesCount{"PA_verticesCount"};
        } PA;

        struct {
            sc_signal<MemoryAddressType> shaderAddress{"VS_shaderAddress"};
            sc_signal<CustomShaderComponentsType> uniforms{"VS_uniforms"};
            sc_signal<VertexPositionFloatType> uniformsData[Isa::maxInputOutputRegisters][Isa::registerComponentsCount];
        } VS;

        struct {
            sc_signal<MemoryAddressType> shaderAddress{"FS_shaderAddress"};
            sc_signal<CustomShaderComponentsType> uniforms{"FS_uniforms"};
            sc_signal<VertexPositionFloatType> uniformsData[Isa::maxInputOutputRegisters][Isa::registerComponentsCount];
        } FS;

        struct {
            sc_signal<MemoryAddressType> framebufferAddress{"OM_framebufferAddress"};
            sc_signal<bool> depthEnable{"OM_depthEnable"};
            sc_signal<MemoryAddressType> depthBufferAddress{"OM_depthBufferAddress"};
        } OM;
    } config;

    // This structure represents outputs signals to be read by user. The user
    // should not set any of these signals to any value and treat them as read-only.
    struct {
        sc_signal<bool> busyNoCs{"busyNoCs"};
        sc_signal<bool> busy{"busy"};
    } out;

    void addSignalsToVcdTrace(VcdTrace & trace, bool publicPorts, bool internalPorts);
    void addProfilingSignalsToVcdTrace(VcdTrace & trace);

private:
    void connectClocks(sc_clock &clock);
    void connectInternalPorts();
    void connectPublicPorts();
    void connectProfilingPorts();

    void setBusyValue();

    sc_in_clk clock{"clock"};
    PortConnector ports;
    PortConnector profilingPorts;
};
