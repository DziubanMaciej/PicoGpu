#include "gpu/gpu.h"
#include "gpu/isa/assembler/assembler.h"
#include "gpu/util/vcd_trace.h"

#include <map>
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
    sc_report_handler::set_actions(SC_INFO, SC_DO_NOTHING);

    // Compile shaders
    Isa::PicoGpuBinary vs = {};
    const char *vsCode = R"code(
            #vertexShader
            #input r0.xyz
            #input r1.xyz
            #output r12.xyzw
            #output r13.xyz

            // Passthrough attributes
            mov r12 r0
            mov r13 r1
            finit r12.w 1.f

            // y-flip
            finit r1 100.f
            fsub r12.y r1 r12
        )code";
    FATAL_ERROR_IF(Isa::assembly(vsCode, &vs), "Failed to assemble VS");
    Isa::PicoGpuBinary fs = {};
    const char *fsCode = R"code(
            #fragmentShader
            #input r0.xyzw
            #input r1.xyz
            #output r12.xyzw

            mov r12.xyz r1
            finit r12.w 1.f
        )code";
    FATAL_ERROR_IF(Isa::assembly(fsCode, &fs), "Failed to assemble FS");
    FATAL_ERROR_IF(!Isa::PicoGpuBinary::areShadersCompatible(vs, fs), "VS is not compatible with FS");

    // Prepare addresses
    printf("Memory layout:\n");
    AddressAllocator addressAllocator{Gpu::memorySize * 4};
    MemoryAddressType vertexBufferAddress = addressAllocator.allocate(54 * 4, "vertexBuffer");
    MemoryAddressType framebufferAddress = addressAllocator.allocate(100 * 100 * 4, "frameBuffer");
    MemoryAddressType depthBufferAddress = addressAllocator.allocate(100 * 100 * 4, "depthBuffer");
    MemoryAddressType vsAddress = addressAllocator.allocate(vs.getSizeInBytes(), "vertexShaderIsa");
    MemoryAddressType fsAddress = addressAllocator.allocate(fs.getSizeInBytes(), "fragmentShaderIsa");
    printf("\n");

    // Prepare profiling storage
    std::map<const char *, sc_time> profiling;

    // Initialize GPU
    Gpu gpu{"Gpu"};
    sc_clock clock("clock", 1, SC_NS, 0.5, 0, SC_NS, true);
    gpu.blocks.GLOBAL.inpClock(clock);
    gpu.blocks.GLOBAL.inpVsCustomInputComponents = vs.getVsCustomInputComponents().raw;
    gpu.blocks.GLOBAL.inpVsPsCustomComponents = vs.getVsPsCustomComponents().raw;
    gpu.blocks.PA.inpVerticesAddress = vertexBufferAddress;
    gpu.blocks.PA.inpVerticesCount = 6;
    gpu.blocks.VS.inpShaderAddress = vsAddress;
    gpu.blocks.RS_OM.framebufferWidth.write(100);
    gpu.blocks.RS_OM.framebufferHeight.write(100);
    gpu.blocks.FS.inpShaderAddress = fsAddress;
    gpu.blocks.OM.inpFramebufferAddress = framebufferAddress;
    gpu.blocks.OM.inpDepthEnable = 1;
    gpu.blocks.OM.inpDepthBufferAddress = depthBufferAddress;

    // Create vcd traces
    VcdTrace trace{TEST_NAME};
    gpu.addSignalsToVcdTrace(trace, true, true);
    VcdTrace profilingTrace{TEST_NAME "Profiling"};
    gpu.addProfilingSignalsToVcdTrace(profilingTrace);

    // Upload shaders ISA to the memory
    gpu.commandStreamer.blitToMemory(vsAddress, vs.getData().data(), vs.getSizeInDwords(), &profiling["Upload VS"]);
    gpu.commandStreamer.blitToMemory(fsAddress, fs.getData().data(), fs.getSizeInDwords(), &profiling["Upload FS"]);

    // Upload vertex data to the memory
    struct Vertex {
        float x, y, z, r, g, b;
    };
    Vertex vertices[] = {
        Vertex{10, 10, 10,   1.0, 0.0, 0.0 },
        Vertex{45, 80, 80,   0.0, 0.0, 1.0 },
        Vertex{90, 10, 10,   0.0, 1.0, 0.0 },

        Vertex{10, 40, 0,    0.2, 0.2, 0.2 },
        Vertex{90, 40, 0,    1.0, 1.0, 1.0 },
        Vertex{45, 20, 130,  0.0, 0.0, 0.0 },
    };
    gpu.commandStreamer.blitToMemory(vertexBufferAddress, (uint32_t *)vertices, sizeof(vertices) / 4, &profiling["Upload VB"]);

    // Clear framebuffer
    uint32_t clearColor = 0xffcccccc;
    gpu.commandStreamer.fillMemory(framebufferAddress, &clearColor, 100 * 100, &profiling["Clear screen"]);

    // Issue a drawcall
    gpu.commandStreamer.draw(&profiling["Draw"]);

    // Blit results to a normal user buffer
    auto pixels = std::make_unique<uint32_t[]>(100 * 100);
    gpu.commandStreamer.blitFromMemory(framebufferAddress, pixels.get(), 100 * 100, &profiling["Read screen"]);

    // Print profiling results
    gpu.waitForIdle(clock);
    printf("Profiling data:\n");
    for (auto it : profiling) {
        printf("\t%s: %s\n", it.first, it.second.to_string().c_str());
    }

    // Save to a file
    stbi_write_png("result.png", 100, 100, 4, pixels.get(), 100 * 4);

    return 0;
}
