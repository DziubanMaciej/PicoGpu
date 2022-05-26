#include "gpu/gpu.h"

#include <memory>
#include <third_party/stb_image_write.h>

extern "C" int sc_main(int argc, char *argv[]) {

    auto pixels = std::make_unique<uint8_t[]>(100 * 100 * 4);

    Gpu gpu{"Gpu", pixels.get()};
    sc_clock clock("clk", 2, SC_NS, 1, 0, SC_NS, false);
    gpu.inpPaClock(clock);
    gpu.inpRsClock(clock);
    // sc_start();
    sc_start({120000, SC_NS});

    stbi_write_png("result.png", 100, 100, 4, pixels.get(), 100 * 4);

    return 0;
}
