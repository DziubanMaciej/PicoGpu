#include "systemc.h"

#include "gpu/blocks/primitive_assembler.h"
#include "gpu/blocks/rasterizer.h"

SC_MODULE(Gpu) {
    Gpu(sc_module_name name, uint8_t * pixels);

    // Blocks of the GPU
    PrimitiveAssembler primitiveAssembler; // abbreviation: PA
    Rasterizer rasterizer;                 // abbreviation: RS

    // This structure represents wirings of individual blocks visible to the
    // user. Ideally user should set all of the fields to desired values.
    struct {
        struct {
            sc_in_clk inpClock;
        } PA;

        struct {
            sc_in_clk inpClock;
            sc_signal<sc_uint<16>> framebufferWidth;
            sc_signal<sc_uint<16>> framebufferHeight;
        } RS;
    } blocks;

private:
    // This structure represents internal wirings between individual blocks
    // The user should not care about them, they are a GPU's implementation
    // detail.
    struct {

        struct {
            sc_signal<bool> isEnabled;
            sc_signal<bool> isDone;
            sc_signal<sc_uint<32>> vertices[6];
        } PA_RS;

    } internalSignals;
};
