#include "gpu/blocks/rasterizer.h"
#include "gpu/util/math.h"

void Rasterizer::rasterize() {
    while (true) {
        wait();
        outIsDone = 1;

        if (!inpEnable) {
            continue;
        }

        outIsDone = 0;

        // Iterate over pixels
        const auto width = inpFramebufferWidth.read();
        const auto height = inpFramebufferHeight.read();
        const auto v1 = Point{(float)inpTriangleVertices[0].read(), (float)inpTriangleVertices[1].read()};
        const auto v2 = Point{(float)inpTriangleVertices[2].read(), (float)inpTriangleVertices[3].read()};
        const auto v3 = Point{(float)inpTriangleVertices[4].read(), (float)inpTriangleVertices[5].read()};
        Point currentPixel{};
        for (currentPixel.x = 0; currentPixel.x < width; currentPixel.x++) {
            for (currentPixel.y = 0; currentPixel.y < height; currentPixel.y++) {
                // Ensure 1px/clk
                wait();

                // Rasterize a pixel
                const size_t offset = currentPixel.y * width + currentPixel.x;
                const bool hit = isPointInTriangle(currentPixel, v1, v2, v3);
                if (hit) {
                    reinterpret_cast<uint32_t *>(this->output)[offset] = 0xffff00ff;
                }
            }
        }
    }
}
