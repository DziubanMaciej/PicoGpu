#include "gpu/gpu.h"
#include "gpu/util/vcd_trace.h"

#include <memory>
#include <third_party/stb_image_write.h>

int sc_main(int argc, char *argv[]) {
    VcdTrace trace{TEST_NAME};
    Gpu gpu{"Gpu"};
    gpu.addSignalsToVcdTrace(trace, true, true, true);
    sc_clock clock("clock", 1, SC_NS, 0.5, 0, SC_NS, true);
    gpu.blocks.BLT.inpClock(clock);
    gpu.blocks.MEMCTL.inpClock(clock);
    gpu.blocks.MEM.inpClock(clock);
    gpu.blocks.PA.inpClock(clock);
    gpu.blocks.PA.inpEnable = false;
    gpu.blocks.PA.inpVerticesAddress = 0x08;
    gpu.blocks.PA.inpVerticesCount = 9;
    gpu.blocks.RS.inpClock(clock);
    gpu.blocks.RS_OM.framebufferWidth.write(100);
    gpu.blocks.RS_OM.framebufferHeight.write(100);
    gpu.blocks.OM.inpClock(clock);
    gpu.blocks.OM.inpFramebufferAddress = 0x50;
    gpu.blocks.OM.inpDepthEnable = 1;
    gpu.blocks.OM.inpDepthBufferAddress = 0x09c90;

    // Our memory layout:
    // vertexBuffer - 0x00008 - 0x00050 (18 dwords)
    // frambuffer   - 0x00050 - 0x09c90 (10000 dwords)
    // depth buffer - 0x09c90 - 0x138d0 (10000 dwords)

    // Upload vertex data to the memory
    uint32_t vertices[] = {
        10,
        10,
        200,
        10,
        20,
        200,
        20,
        10,
        200,

        10 + 30,
        10,
        200,
        10 + 30,
        20,
        200,
        20 + 30,
        10,
        200,

        5,
        15,
        100,
        80,
        15,
        100,
        40,
        40,
    };
    gpu.userBlitter.blitToMemory(0x08, vertices, sizeof(vertices) / sizeof(vertices[0]));
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
    gpu.userBlitter.blitFromMemory(0x50, pixels.get(), 100 * 100);
    while (gpu.userBlitter.hasPendingOperation()) {
        sc_start({1, SC_NS});
    }
    stbi_write_png("result.png", 100, 100, 4, pixels.get(), 100 * 4);

    return 0;
}
