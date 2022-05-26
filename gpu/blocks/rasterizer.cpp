#include "gpu/blocks/rasterizer.h"
#include "gpu/util/math.h"

void Rasterizer::rasterize() {
    outIsDone.write(0);

    while (true) {
        do {
            wait();
        } while (!inpEnable.read());

        // Iterate over pixels
        const auto width = inpFramebufferWidth.read();
        const auto height = inpFramebufferHeight.read();
        const auto v1 = Point{inpTriangleVertices[0].read(), inpTriangleVertices[1].read()};
        const auto v2 = Point{inpTriangleVertices[2].read(), inpTriangleVertices[3].read()};
        const auto v3 = Point{inpTriangleVertices[4].read(), inpTriangleVertices[5].read()};
        Point currentPixel{};
        for (currentPixel.x = 0; currentPixel.x < width; currentPixel.x++) {
            for (currentPixel.y = 0; currentPixel.y < height; currentPixel.y++) {
                // Ensure 1px/clk
                wait();

                // Rasterize a pixel
                const size_t offset = currentPixel.y * width + currentPixel.x;
                const uint32_t value = isPointInTriangle(currentPixel, v1, v2, v3) ? 0xffff00ff : 0xeeeeeeff;
                reinterpret_cast<uint32_t *>(this->output)[offset] = value;
            }
        }

        outIsDone.write(1);
        do {
            wait();
        } while (inpEnable.read());
    }
}
