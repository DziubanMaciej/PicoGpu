#include "systemc.h"

SC_MODULE(Rasterizer) {
    sc_in_clk inpClock;
    sc_in<bool> inpEnable;
    sc_in<sc_uint<32>> inpTriangleVertices[6];
    sc_in<sc_uint<16>> inpFramebufferWidth;
    sc_in<sc_uint<16>> inpFramebufferHeight;
    sc_out<bool> outIsDone;

    SC_HAS_PROCESS(Rasterizer);
    Rasterizer(sc_module_name name, uint8_t * output) : output(output) {
        SC_CTHREAD(rasterize, inpClock.pos());
    }

    void rasterize();

private:
    // TODO write output to some other module instead of a raw uint8_t array
    uint8_t *output{};
};
