#include "gpu/types.h"

#include <systemc.h>

SC_MODULE(OutputMerger) {
    sc_in_clk inpClock;
    struct {
        sc_in<bool> inpEnable;
        sc_fifo_in<sc_uint<32>> inpPixels;
        sc_out<bool> outIsDone;
    } previousBlock;
    struct {
        sc_in<MemoryAddressType> inpAddress;
        sc_in<sc_uint<16>> inpWidth;
        sc_in<sc_uint<16>> inpHeight;
    } framebuffer;
    struct {
        sc_out<bool> outEnable;
        sc_out<MemoryAddressType> outAddress;
        sc_out<MemoryDataType> outData;
        sc_in<bool> inpCompleted;
    } memory;

    SC_CTOR(OutputMerger) {
        SC_CTHREAD(main, inpClock.pos());
    }

    void main();
};
