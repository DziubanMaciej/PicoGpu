#include "gpu/blocks/output_merger.h"
#include "gpu/util/handshake.h"

void OutputMerger::main() {
    while (true) {
        ShadedFragment fragment = Handshake::receive(previousBlock.inpIsSending, previousBlock.inpFragment, previousBlock.outIsReceiving);

        // Latch state
        const MemoryAddressType framebufferAddress = framebuffer.inpAddress.read();
        const int framebufferWidth = framebuffer.inpWidth.read();
        const int framebufferHeight = framebuffer.inpHeight.read();

        // Write pixel to memory
        memory.outEnable = 1;
        memory.outAddress = framebuffer.inpAddress.read() + (fragment.y * framebufferWidth + fragment.x) * 4;
        memory.outData = fragment.color;
        wait();
        memory.outEnable = 0;
        while (!memory.inpCompleted) {
            wait();
        }
        memory.outAddress = 0;
        memory.outData = 0;
    }
}
