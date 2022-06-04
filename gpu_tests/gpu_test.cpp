#include "gpu/gpu.h"

#include <memory>
#include <third_party/stb_image_write.h>

int sc_main(int argc, char *argv[]) {
    auto pixels = std::make_unique<uint8_t[]>(100 * 100 * 4);

    Gpu gpu{"Gpu", pixels.get()};
    sc_clock clock("clk", 2, SC_NS, 1, 0, SC_NS, false);
    gpu.blocks.PA.inpClock(clock);
    gpu.blocks.RS.inpClock(clock);
    gpu.blocks.RS.framebufferWidth.write(100);
    gpu.blocks.RS.framebufferHeight.write(100);
    sc_start({120000, SC_NS});

    stbi_write_png("result.png", 100, 100, 4, pixels.get(), 100 * 4);

    return 0;
}
