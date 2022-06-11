#pragma once

#include "gpu/blocks/memory.h"
#include "gpu/blocks/memory_controller.h"
#include "gpu/blocks/output_merger.h"
#include "gpu/blocks/primitive_assembler.h"
#include "gpu/blocks/rasterizer.h"
#include "gpu/blocks/user_blitter.h"

#include <systemc.h>

class VcdTrace;

SC_MODULE(Gpu) {
    Gpu(sc_module_name name);

    // Blocks of the GPU
    UserBlitter userBlitter;               // abbreviation: BLT
    MemoryController<3> memoryController;  // abbreviation: MEMCTL
    Memory<12000> memory;                  // abbreviation: MEM
    PrimitiveAssembler primitiveAssembler; // abbreviation: PA
    Rasterizer rasterizer;                 // abbreviation: RS
    OutputMerger outputMerger;             // abbreviation: OM

    // This structure represents wirings of individual blocks visible to the
    // user. Ideally user should set all of the fields to desired values.
    struct {
        struct {
            sc_in_clk inpClock{"BLT_inpClock"};
        } BLT;

        struct {
            sc_in_clk inpClock{"MEMCTL_inpClock"};
        } MEMCTL;

        struct {
            sc_in_clk inpClock{"MEM_inpClock"};
        } MEM;

        struct {
            sc_in_clk inpClock{"PA_inpClock"};
            sc_signal<bool> inpEnable{"PA_inpEnable"};
            sc_signal<MemoryAddressType> inpVerticesAddress{"PA_inpVerticesAddress"};
            sc_signal<sc_uint<8>> inpVerticesCount{"PA_inpVerticesCount"};
        } PA;

        struct {
            sc_in_clk inpClock{"RS_inpClock"};

        } RS;

        struct {
            sc_signal<VertexPositionType> framebufferWidth{"RS_OM_framebufferWidth"};
            sc_signal<VertexPositionType> framebufferHeight{"RS_OM_framebufferHeight"};
        } RS_OM;

        struct {
            sc_in_clk inpClock{"OM_inpClock"};
            sc_signal<MemoryAddressType> inpFramebufferAddress{"OM_inpFramebufferAddress"};
            sc_signal<bool> inpDepthEnable{"OM_inpDepthEnable"};
            sc_signal<MemoryAddressType> inpDepthBufferAddress{"OM_inpDepthBufferAddress"};
        } OM;
    } blocks;

    void addSignalsToVcdTrace(VcdTrace & trace, bool allClocksTheSame, bool publicPorts, bool internalPorts);

private:
    // This structure represents internal wirings between individual blocks
    // The user should not care about them, they are a GPU's implementation
    // detail.
    struct {
        struct {
            sc_signal<bool> enable{"BLT_MEMCTL_enable"};
            sc_signal<bool> write{"BLT_MEMCTL_write"};
            sc_signal<MemoryAddressType> address{"BLT_MEMCTL_address"};
            sc_signal<MemoryDataType> dataForWrite{"BLT_MEMCTL_dataForWrite"};
            sc_signal<bool> completed{"BLT_MEMCTL_completed"};
        } BLT_MEMCTL;

        struct {
            sc_signal<MemoryDataType> dataForRead{"MEMCTL_dataForRead"};
        } MEMCTL;

        struct {
            sc_signal<bool> enable{"MEMCTL_MEM_enable"};
            sc_signal<bool> write{"MEMCTL_MEM_write"};
            sc_signal<MemoryAddressType> address{"MEMCTL_MEM_address"};
            sc_signal<MemoryDataType> dataForWrite{"MEMCTL_MEM_dataForWrite"};
            sc_signal<MemoryDataType> dataForRead{"MEMCTL_MEM_dataForRead"};
            sc_signal<bool> completed{"MEMCTL_MEM_completed"};
        } MEMCTL_MEM;

        struct {
            sc_signal<bool> enable{"MEMCTL_PA_enable"};
            sc_signal<bool> write{"MEMCTL_PA_write", 0}; // unused, always 0
            sc_signal<MemoryAddressType> address{"MEMCTL_PA_address"};
            sc_signal<MemoryDataType> dataForWrite{"MEMCTL_PA_dataForWrite", 0}; // unused, always 0
            sc_signal<bool> completed{"MEMCTL_PA_completed"};
        } MEMCTL_PA;

        struct {
            sc_signal<bool> enable{"MEMCTL_OM_enable"};
            sc_signal<bool> write{"MEMCTL_OM_write"};
            sc_signal<MemoryAddressType> address{"MEMCTL_OM_address"};
            sc_signal<MemoryDataType> dataForWrite{"MEMCTL_OM_dataForWrite"};
            sc_signal<bool> completed{"MEMCTL_OM_completed"};
        } MEMCTL_OM;

        struct {
            sc_signal<bool> isEnabled{"PA_RS_isEnabled"};
            sc_signal<bool> isDone{"PA_RS_isDone"};
            sc_signal<VertexPositionType> vertices[9];
        } PA_RS;

        struct {
            sc_signal<bool> isReceiving{"RS_OM_isReceiving"};
            sc_signal<bool> isSending{"RS_OM_isSending"};
            sc_signal<ShadedFragment> fragment{"RS_OM_fragment"};
        } RS_OM;

    } internalSignals;
};
