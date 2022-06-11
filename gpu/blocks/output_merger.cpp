#include "gpu/blocks/output_merger.h"
#include "gpu/util/handshake.h"

void OutputMerger::main() {
    while (true) {
        ShadedFragment fragment = Handshake::receive(previousBlock.inpIsSending, previousBlock.inpFragment, previousBlock.outIsReceiving);

        // Perform depth test
        // TODO this can only handle 32-bit depth buffer
        if (depth.inpEnable) {
            const MemoryDataType depthAddress = depth.inpAddress.read() + (fragment.y * framebuffer.inpWidth.read() + fragment.x) * depthTypeByteSize;

            // Read current depth
            memory.outEnable = 1;
            memory.outAddress = depthAddress;
            memory.outWrite = 0;
            wait();
            memory.outEnable = 0;
            while (!memory.inpCompleted) {
                wait();
            }
            memory.outAddress = 0;

            // Actual depth test. Discard fragment if not passed
            const uint32_t currentDepth = memory.inpData.read().to_uint();
            const uint32_t newDepth = fragment.z.to_uint();
            if (currentDepth > newDepth) {
                continue;
            }

            // If depth test passed, update the value
            if (currentDepth != newDepth) {
                memory.outEnable = 1;
                memory.outWrite = 1;
                memory.outAddress = depthAddress;
                memory.outData = fragment.z;
                wait();
                memory.outEnable = 0;
                while (!memory.inpCompleted) {
                    wait();
                }
                memory.outAddress = 0;
                memory.outData = 0;
            }
        }

        // Write pixel to memory
        memory.outEnable = 1;
        memory.outWrite = 1;
        memory.outAddress = framebuffer.inpAddress.read() + (fragment.y * framebuffer.inpWidth.read() + fragment.x) * fragmentColorTypeByteSize;
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
