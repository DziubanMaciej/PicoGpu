#include "systemc.h"

#include "gpu/blocks/primitive_assembler.h"
#include "gpu/blocks/rasterizer.h"

SC_MODULE(Gpu) {
    sc_in_clk inpPaClock;
    sc_in_clk inpRsClock;

    PrimitiveAssembler primitiveAssembler;
    Rasterizer rasterizer;

    sc_signal<bool> rasterizerIsEnabled;
    sc_signal<bool> rasterizerIsDone;
    sc_signal<sc_uint<32>> rasterizerVertices[6];

    sc_signal<sc_uint<16>> rasterizeFramebufferWidth;
    sc_signal<sc_uint<16>> rasterizeFramebufferHeight;

    Gpu(sc_module_name name, uint8_t * pixels);
};
