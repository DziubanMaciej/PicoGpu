#include "gpu/blocks/output_merger.h"

void OutputMerger::main() {
    while (1) {
        wait();
        previousBlock.outIsDone = 1;

        if (!previousBlock.inpEnable.read()) {
            continue;
        }

        previousBlock.outIsDone = 0;

        // Latch state
        const MemoryAddressType framebufferAddress = framebuffer.inpAddress.read();
        const int framebufferWidth = framebuffer.inpWidth.read();
        const int framebufferHeight = framebuffer.inpHeight.read();

        // Iterate over all pixels in the framebuffer
        sc_uint<32> pixel;
        for (int y = 0; y < framebufferHeight; y++) {
            for (int x = 0; x < framebufferWidth; x++) {
                // Try to load pixel once per cycle
                while (!previousBlock.inpPixels.nb_read(pixel)) {
                    wait();
                }

                // Write pixel to memory
                memory.outEnable = 1;
                memory.outAddress = framebufferAddress + (y * framebufferWidth + x) * 4;
                memory.outData = pixel;
                wait();
                memory.outEnable = 0;

                while (!memory.inpCompleted) {
                    wait();
                }

                memory.outAddress = 0;
            }
        }
    }
}
