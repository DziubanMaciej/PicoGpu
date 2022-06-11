#include "gpu/blocks/rasterizer.h"
#include "gpu/util/handshake.h"
#include "gpu/util/math.h"

void Rasterizer::rasterize() {
    while (true) {
        wait();
        previousBlock.outIsDone = 1;

        if (!previousBlock.inpEnable) {
            continue;
        }

        previousBlock.outIsDone = 0;

        // Iterate over pixels
        const auto width = framebuffer.inpWidth.read();
        const auto height = framebuffer.inpHeight.read();
        const auto v1 = Point{(float)previousBlock.inpTriangleVertices[0].read(), (float)previousBlock.inpTriangleVertices[1].read()};
        const auto v2 = Point{(float)previousBlock.inpTriangleVertices[3].read(), (float)previousBlock.inpTriangleVertices[4].read()};
        const auto v3 = Point{(float)previousBlock.inpTriangleVertices[6].read(), (float)previousBlock.inpTriangleVertices[7].read()};
        ShadedFragment currentFragment{};
        currentFragment.color = randomizeColor();
        for (currentFragment.y = 0; currentFragment.y < height; currentFragment.y++) {
            for (currentFragment.x = 0; currentFragment.x < width; currentFragment.x++) {
                const bool hit = isPointInTriangle(Point{(float)currentFragment.x, (float)currentFragment.y}, v1, v2, v3);
                if (hit) {
                    currentFragment.z = previousBlock.inpTriangleVertices[2]; // TODO: this is incorrect. We should probably interpolate the z-value. Barycentrics???
                    Handshake::send(nextBlock.inpIsReceiving, nextBlock.outIsSending, nextBlock.outFragment, currentFragment);
                }
            }
        }
    }
}

FragmentColorType Rasterizer::randomizeColor() {
    const uint8_t r = (uint8_t)rand();
    const uint8_t g = (uint8_t)rand();
    const uint8_t b = (uint8_t)rand();
    const uint8_t a = 255;
    return (a << 24) | (b << 16) | (g << 8) | r;
}
