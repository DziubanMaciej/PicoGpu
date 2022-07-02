#include "gpu/blocks/rasterizer.h"
#include "gpu/util/conversions.h"
#include "gpu/util/handshake.h"
#include "gpu/util/math.h"

void Rasterizer::rasterize() {
    while (true) {
        wait();

        const auto verticesInPrimitive = 3; // only triangles
        const auto componentsPerVertex = 3; // x, y, z
        uint32_t receivedVertices[verticesInPrimitive * componentsPerVertex];

        Handshake::receiveArrayWithParallelPorts(previousBlock.inpSending, previousBlock.outReceiving,
                                                 previousBlock.inpData, previousBlock.portsCount,
                                                 receivedVertices, verticesInPrimitive * componentsPerVertex);

        // Iterate over pixels
        const auto width = framebuffer.inpWidth.read();
        const auto height = framebuffer.inpHeight.read();

        const Point v1{Conversions::uintBytesToFloat(receivedVertices[0]), Conversions::uintBytesToFloat(receivedVertices[1])};
        const Point v2{Conversions::uintBytesToFloat(receivedVertices[3]), Conversions::uintBytesToFloat(receivedVertices[4])};
        const Point v3{Conversions::uintBytesToFloat(receivedVertices[6]), Conversions::uintBytesToFloat(receivedVertices[7])};
        ShadedFragment currentFragment{};
        currentFragment.color = randomizeColor();
        for (currentFragment.y = 0; currentFragment.y < height; currentFragment.y++) {
            for (currentFragment.x = 0; currentFragment.x < width; currentFragment.x++) {
                const Point pixel{static_cast<float>(currentFragment.x), static_cast<float>(currentFragment.y)};
                const bool hit = isPointInTriangle(pixel, v1, v2, v3);
                if (hit) {
                    currentFragment.z = previousBlock.inpData[2]; // TODO: this is incorrect. We should probably interpolate the z-value. Barycentrics???
                    Handshake::send(nextBlock.inpReceiving, nextBlock.outSending, nextBlock.outData, currentFragment);
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
