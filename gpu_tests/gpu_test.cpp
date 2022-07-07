#include "gpu/gpu.h"
#include "gpu/isa/assembler/assembler.h"
#include "gpu/util/vcd_trace.h"

#include <memory>
#include <third_party/stb_image_write.h>

struct AddressAllocator {
    AddressAllocator(size_t memorySize) : memorySize(memorySize) {}

    size_t allocate(size_t size, const char *purpose) {
        FATAL_ERROR_IF(currentOffset + size > memorySize, "Too small memory");
        FATAL_ERROR_IF(size % 4 != 0, "Size must be a multiple of 4. Got ", size);
        const size_t result = currentOffset;
        currentOffset += size;

        printf("  %16s: 0x%08x - 0x%08x\n", purpose, result, currentOffset);
        return result;
    }

private:
    size_t currentOffset = 0;
    size_t memorySize;
};

int sc_main(int argc, char *argv[]) {
    // Compile shaders
    Isa::PicoGpuBinary vs = {};
    const char *vsCode = R"code(
            #input i0.xyz
            #output o0.xyz
            mov o0 i0

            finit r0 100.f
            fsub o0.y r0 o0
        )code";
    FATAL_ERROR_IF(Isa::assembly(vsCode, &vs), "Failed to assemble VS");

    // Prepare addresses
    AddressAllocator addressAllocator{Gpu::memorySize * 4};
    MemoryAddressType vertexBufferAddress = addressAllocator.allocate(27 * 4, "vertexBuffer");
    MemoryAddressType framebufferAddress = addressAllocator.allocate(100 * 100 * 4, "frameBuffer");
    MemoryAddressType depthBufferAddress = addressAllocator.allocate(100 * 100 * 4, "depthBuffer");
    MemoryAddressType vsAddress = addressAllocator.allocate(vs.getSizeInBytes(), "vertexShaderIsa");

    // Initialize GPU
    Gpu gpu{"Gpu"};
    sc_clock clock("clock", 1, SC_NS, 0.5, 0, SC_NS, true);
    gpu.blocks.GLOBAL.inpClock(clock);
    gpu.blocks.PA.inpEnable = false;
    gpu.blocks.PA.inpVerticesAddress = vertexBufferAddress;
    gpu.blocks.PA.inpVerticesCount = 9;
    gpu.blocks.VS.inpShaderAddress = vsAddress;
    gpu.blocks.RS_OM.framebufferWidth.write(100);
    gpu.blocks.RS_OM.framebufferHeight.write(100);
    gpu.blocks.OM.inpFramebufferAddress = framebufferAddress;
    gpu.blocks.OM.inpDepthEnable = 1;
    gpu.blocks.OM.inpDepthBufferAddress = depthBufferAddress;

    // Create vcd traces
    VcdTrace trace{TEST_NAME};
    gpu.addSignalsToVcdTrace(trace, true, true);
    VcdTrace profilingTrace{TEST_NAME "Profiling"};
    gpu.addProfilingSignalsToVcdTrace(profilingTrace);

    // Upload vertex shader to the memory
    gpu.userBlitter.blitToMemory(vsAddress, vs.getData().data(), vs.getSizeInDwords());
    gpu.waitForIdle(clock);

    // Upload vertex data to the memory
    struct Vertex {
        float x, y, z;
    };
    Vertex vertices[] = {
        Vertex{10, 10, 200},
        Vertex{10, 20, 200},
        Vertex{20, 10, 200},

        Vertex{40, 10, 200},
        Vertex{40, 20, 200},
        Vertex{50, 10, 200},

        Vertex{10, 15, 100},
        Vertex{80, 15, 100},
        Vertex{40, 40, 100},
    };
    gpu.userBlitter.blitToMemory(vertexBufferAddress, (uint32_t *)vertices, sizeof(vertices) / 4);
    gpu.waitForIdle(clock);

    // Clear framebuffer
    uint32_t clearColor = 0xffcccccc;
    gpu.userBlitter.fillMemory(framebufferAddress, &clearColor, 100 * 100);
    gpu.waitForIdle(clock);

    // Enable primitive assembler
    gpu.blocks.PA.inpEnable = true;
    sc_start({1, SC_NS});
    gpu.blocks.PA.inpEnable = false;
    gpu.waitForIdle(clock);

    // Blit results to a normal user buffer and output it to a file
    auto pixels = std::make_unique<uint32_t[]>(100 * 100);
    gpu.userBlitter.blitFromMemory(framebufferAddress, pixels.get(), 100 * 100);
    gpu.waitForIdle(clock);
    stbi_write_png("result.png", 100, 100, 4, pixels.get(), 100 * 4);

    return 0;
}
