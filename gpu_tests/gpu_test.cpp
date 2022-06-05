#include "gpu/gpu.h"

#include <memory>
#include <third_party/stb_image_write.h>

int sc_main(int argc, char *argv[]) {
    auto pixels = std::make_unique<uint8_t[]>(100 * 100 * 4);

    Gpu gpu{"Gpu", pixels.get()};
    sc_clock clock("clk", 2, SC_NS, 1, 0, SC_NS, false);
    gpu.blocks.BLT.inpClock(clock);
    gpu.blocks.MEMCTL.inpClock(clock);
    gpu.blocks.MEM.inpClock(clock);
    gpu.blocks.PA.inpClock(clock);
    gpu.blocks.PA.inpEnable = false;
    gpu.blocks.PA.inpVerticesAddress = 0x08;
    gpu.blocks.PA.inpVerticesCount = 3;
    gpu.blocks.RS.inpClock(clock);
    gpu.blocks.RS.framebufferWidth.write(100);
    gpu.blocks.RS.framebufferHeight.write(100);

    // Upload vertex data to the memory
    uint32_t vertices[] = {
        10,
        10,
        10,
        20,
        20,
        10,
    };
    gpu.userBlitter.blitToMemory(0x08, vertices, sizeof(vertices) / sizeof(vertices[0]));
    while (gpu.userBlitter.hasPendingOperation()) {
        sc_start({2, SC_NS});
    }

    // Enable primitive assembler
    gpu.blocks.PA.inpEnable = true;
    sc_start({1, SC_NS});
    gpu.blocks.PA.inpEnable = false;

    // Allow the simulation to proceed and write results to a file
    sc_start({120000, SC_NS});
    stbi_write_png("result.png", 100, 100, 4, pixels.get(), 100 * 4);

    return 0;
}
