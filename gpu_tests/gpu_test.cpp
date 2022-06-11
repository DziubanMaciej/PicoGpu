#include "gpu/gpu.h"
#include "gpu/util/vcd_trace.h"

#include <memory>
#include <third_party/stb_image_write.h>

struct AddressAllocator {
    AddressAllocator(size_t memorySize) : memorySize(memorySize) {}

    size_t allocate(size_t size, const char *purpose) {
        FATAL_ERROR_IF(currentOffset + size > memorySize, "Too small memory");
        FATAL_ERROR_IF(size % 4 != 0, "Size must be a multiple of 4");
        const size_t result = currentOffset;
        currentOffset += size;

        printf("  %14s: 0x%08x - 0x%08x\n", purpose, result, currentOffset);
        return result;
    }

private:
    size_t currentOffset = 0;
    size_t memorySize;
};

int sc_main(int argc, char *argv[]) {
    // Prepare addresses
    AddressAllocator addressAllocator{Gpu::memorySize * 4};
    MemoryAddressType vertexBufferAddress = addressAllocator.allocate(27 * 4, "vertexBuffer");
    MemoryAddressType framebufferAddress = addressAllocator.allocate(100 * 100 * 4, "frameBuffer");
    MemoryAddressType depthBufferAddress = addressAllocator.allocate(100 * 100 * 4, "depthBuffer");

    // Initialize GPU
    Gpu gpu{"Gpu"};
    sc_clock clock("clock", 1, SC_NS, 0.5, 0, SC_NS, true);
    gpu.blocks.BLT.inpClock(clock);
    gpu.blocks.MEMCTL.inpClock(clock);
    gpu.blocks.MEM.inpClock(clock);
    gpu.blocks.PA.inpClock(clock);
    gpu.blocks.PA.inpEnable = false;
    gpu.blocks.PA.inpVerticesAddress = vertexBufferAddress;
    gpu.blocks.PA.inpVerticesCount = 9;
    gpu.blocks.RS.inpClock(clock);
    gpu.blocks.RS_OM.framebufferWidth.write(100);
    gpu.blocks.RS_OM.framebufferHeight.write(100);
    gpu.blocks.OM.inpClock(clock);
    gpu.blocks.OM.inpFramebufferAddress = framebufferAddress;
    gpu.blocks.OM.inpDepthEnable = 1;
    gpu.blocks.OM.inpDepthBufferAddress = depthBufferAddress;

    // Create vcd trace
    VcdTrace trace{TEST_NAME};
    gpu.addSignalsToVcdTrace(trace, true, true, true);

    // Upload vertex data to the memory
    struct Vertex {
        uint32_t x, y, z;
    };
    Vertex vertices[] = {
        Vertex{10, 10, 200},
        Vertex{10, 20, 200},
        Vertex{20, 10, 200},

        Vertex{40, 10, 200},
        Vertex{40, 20, 200},
        Vertex{50, 10, 200},

        Vertex{10, 15, 200},
        Vertex{80, 15, 200},
        Vertex{40, 40, 200},
    };
    gpu.userBlitter.blitToMemory(vertexBufferAddress, (uint32_t *)vertices, sizeof(vertices) / 4);
    while (gpu.userBlitter.hasPendingOperation()) {
        sc_start({1, SC_NS});
    }

    // Clear framebuffer
    uint32_t clearColor = 0xffcccccc;
    gpu.userBlitter.fillMemory(framebufferAddress, &clearColor, 100 * 100);
    while (gpu.userBlitter.hasPendingOperation()) {
        sc_start({1, SC_NS});
    }

    // Enable primitive assembler
    gpu.blocks.PA.inpEnable = true;
    sc_start({1, SC_NS});
    gpu.blocks.PA.inpEnable = false;

    // Allow the simulation to proceed and write results to a file
    sc_start({120000, SC_NS}); // TODO implement some better way of waiting, e.g. some rendered triangles counter in the GPU

    // Blit results to a normal user buffer and output it to a file
    auto pixels = std::make_unique<uint32_t[]>(100 * 100);
    gpu.userBlitter.blitFromMemory(framebufferAddress, pixels.get(), 100 * 100);
    while (gpu.userBlitter.hasPendingOperation()) {
        sc_start({1, SC_NS});
    }
    stbi_write_png("result.png", 100, 100, 4, pixels.get(), 100 * 4);

    return 0;
}
