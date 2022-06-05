#include "systemc.h"

#include "gpu/blocks/memory.h"
#include "gpu/blocks/memory_controller.h"
#include "gpu/blocks/primitive_assembler.h"
#include "gpu/blocks/rasterizer.h"
#include "gpu/blocks/user_blitter.h"

class VcdTrace;

SC_MODULE(Gpu) {
    Gpu(sc_module_name name, uint8_t * pixels);

    // Blocks of the GPU
    UserBlitter userBlitter;               // abbreviation: BLT
    MemoryController<2> memoryController;  // abbreviation: MEMCTL
    Memory<10> memory;                     // abbreviation: MEM
    PrimitiveAssembler primitiveAssembler; // abbreviation: PA
    Rasterizer rasterizer;                 // abbreviation: RS

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
            sc_signal<sc_uint<16>> framebufferWidth{"RS_framebufferWidth"};
            sc_signal<sc_uint<16>> framebufferHeight{"RS_framebufferHeight"};
        } RS;
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
            sc_signal<bool> write{"MEMCTL_PA_write"}; // unused, always 0
            sc_signal<MemoryAddressType> address{"MEMCTL_PA_address"};
            sc_signal<MemoryDataType> dataForWrite{"MEMCTL_PA_dataForWrite"}; // unused, always 0
            sc_signal<bool> completed{"MEMCTL_PA_completed"};
        } MEMCTL_PA;

        struct {
            sc_signal<bool> isEnabled{"MEMCTL_PA_isEnabled"};
            sc_signal<bool> isDone{"MEMCTL_PA_isDone"};
            sc_signal<sc_uint<32>> vertices[6];
        } PA_RS;

    } internalSignals;
};
