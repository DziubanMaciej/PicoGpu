#pragma once

#include "gpu/blocks/memory.h"
#include "gpu/blocks/memory_controller.h"
#include "gpu/blocks/output_merger.h"
#include "gpu/blocks/primitive_assembler.h"
#include "gpu/blocks/rasterizer.h"
#include "gpu/blocks/user_blitter.h"
#include "gpu/util/port_connector.h"

#include <systemc.h>

class VcdTrace;

SC_MODULE(Gpu) {
    constexpr static inline size_t memorySize = 21000;
    Gpu(sc_module_name name);

    // Blocks of the GPU
    UserBlitter userBlitter;               // abbreviation: BLT
    MemoryController<3> memoryController;  // abbreviation: MEMCTL
    Memory<memorySize> memory;             // abbreviation: MEM
    PrimitiveAssembler primitiveAssembler; // abbreviation: PA
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
            sc_signal<VertexPositionFloatType> framebufferWidth{"RS_OM_framebufferWidth"};
            sc_signal<VertexPositionFloatType> framebufferHeight{"RS_OM_framebufferHeight"};
        } RS_OM;

        struct {
            sc_signal<MemoryAddressType> inpFramebufferAddress{"OM_inpFramebufferAddress"};
            sc_signal<bool> inpDepthEnable{"OM_inpDepthEnable"};
            sc_signal<MemoryAddressType> inpDepthBufferAddress{"OM_inpDepthBufferAddress"};
        } OM;
    } blocks;

    void addSignalsToVcdTrace(VcdTrace & trace, bool publicPorts, bool internalPorts);

private:
    void connectClocks();
    void connectInternalPorts();
    void connectPublicPorts();

    PortConnector ports;
};
