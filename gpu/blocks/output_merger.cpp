#include "gpu/blocks/output_merger.h"
#include "gpu/util/conversions.h"
#include "gpu/util/handshake.h"

void OutputMerger::main() {
    while (true) {
        const ShadedFragment fragment = Handshake::receive(previousBlock.inpIsSending, previousBlock.inpFragment, previousBlock.outIsReceiving);
        const uint32_t fragmentX = fragment.x;
        const uint32_t fragmentY = fragment.y;
        const float fragmentZ = Conversions::uintBytesToFloat(fragment.z);

        // Perform depth test
        // TODO this can only handle 32-bit depth buffer
        if (depth.inpEnable) {
            const MemoryDataType depthAddress = depth.inpAddress.read() + (fragmentY * framebuffer.inpWidth.read() + fragmentX) * depthTypeByteSize;

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
            const float currentDepth = Conversions::readFloat(memory.inpData);
            if (currentDepth > fragmentZ) {
                continue;
            }

            // If depth test passed, update the value
            if (currentDepth != fragmentZ) {
                memory.outEnable = 1;
                memory.outWrite = 1;
                memory.outAddress = depthAddress;
                memory.outData = Conversions::floatBytesToUint(fragmentZ);
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
        memory.outAddress = framebuffer.inpAddress.read() + (fragmentY * framebuffer.inpWidth.read() + fragmentX) * fragmentColorTypeByteSize;
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
