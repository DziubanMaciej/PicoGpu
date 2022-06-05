#include "gpu/blocks/rasterizer.h"
#include "gpu/util/math.h"

void Rasterizer::rasterize() {
    while (true) {
        wait();
        previousBlock.outIsDone = 1;

        if (!previousBlock.inpEnable) {
            continue;
        }

        previousBlock.outIsDone = 0;

        // Wait for next block
        while (!nextBlock.inpIsDone) {
            wait();
        }

        // Create enable pulse
        nextBlock.outEnable = true;
        wait();
        nextBlock.outEnable = false;

        // Iterate over pixels
        const auto width = framebuffer.inpWidth.read();
        const auto height = framebuffer.inpHeight.read();
        const auto v1 = Point{(float)previousBlock.inpTriangleVertices[0].read(), (float)previousBlock.inpTriangleVertices[1].read()};
        const auto v2 = Point{(float)previousBlock.inpTriangleVertices[2].read(), (float)previousBlock.inpTriangleVertices[3].read()};
        const auto v3 = Point{(float)previousBlock.inpTriangleVertices[4].read(), (float)previousBlock.inpTriangleVertices[5].read()};
        Point currentPixel{};
        for (currentPixel.x = 0; currentPixel.x < width; currentPixel.x++) {
            for (currentPixel.y = 0; currentPixel.y < height; currentPixel.y++) {
                // Ensure 1px/clk
                wait();

                // Rasterize a pixel
                const size_t offset = currentPixel.y * width + currentPixel.x;
                const bool hit = isPointInTriangle(currentPixel, v1, v2, v3); // TODO use this properly
                const uint32_t color = hit ? 0xffff00ff : 0x00000000;
                nextBlock.outPixels.write(color);
            }
        }
    }
}
