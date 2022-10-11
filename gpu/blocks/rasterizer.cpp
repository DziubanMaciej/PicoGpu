#include "gpu/blocks/rasterizer.h"
#include "gpu/util/conversions.h"
#include "gpu/util/handshake.h"
#include "gpu/util/math.h"

void Rasterizer::rasterize() {
    while (true) {
        wait();

        const auto verticesInPrimitive = 3; // only triangles
        const auto componentsPerVertex = 4; // x, y, z
        uint32_t receivedVertices[verticesInPrimitive * componentsPerVertex];

        Handshake::receiveArrayWithParallelPorts(previousBlock.inpSending, previousBlock.outReceiving,
                                                 previousBlock.inpData, previousBlock.portsCount,
                                                 receivedVertices, verticesInPrimitive * componentsPerVertex,
                                                 &profiling.outBusy);

        // Iterate over pixels
        const auto width = framebuffer.inpWidth.read();
        const auto height = framebuffer.inpHeight.read();

        const Point v1 = readPoint(receivedVertices, componentsPerVertex, 0);
        const Point v2 = readPoint(receivedVertices, componentsPerVertex, 1);
        const Point v3 = readPoint(receivedVertices, componentsPerVertex, 2);
        ShadedFragment currentFragment{};
        currentFragment.color = randomizeColor();
        for (currentFragment.y = 0; currentFragment.y < height; currentFragment.y++) {
            for (currentFragment.x = 0; currentFragment.x < width; currentFragment.x++) {
                const Point pixel{static_cast<float>(currentFragment.x), static_cast<float>(currentFragment.y)};
                const bool hit = isPointInTriangle(pixel, v1, v2, v3);
                if (hit) {
                    currentFragment.z = previousBlock.inpData[2]; // TODO: this is incorrect. We should probably interpolate the z-value. Barycentrics???
                    Handshake::send(nextBlock.inpReceiving, nextBlock.outSending, nextBlock.outData, currentFragment);
                    profiling.outFragmentsProduced = profiling.outFragmentsProduced.read() + 1;
                }
            }
        }
    }
}

Point Rasterizer::readPoint(const uint32_t *receivedVertices, size_t stride, size_t pointIndex) {
    Point point;
    const float w = Conversions::uintBytesToFloat(receivedVertices[pointIndex * stride + 3]);
    FATAL_ERROR_IF(w == 0, "Homogeneous coordinate is 0");
    point.x = Conversions::uintBytesToFloat(receivedVertices[pointIndex * stride + 0]) / w;
    point.y = Conversions::uintBytesToFloat(receivedVertices[pointIndex * stride + 1]) / w;
    point.z = Conversions::uintBytesToFloat(receivedVertices[pointIndex * stride + 2]) / w;
    point.w = w;
    return point;
}

FragmentColorType Rasterizer::randomizeColor() {
    const uint8_t r = (uint8_t)rand();
    const uint8_t g = (uint8_t)rand();
    const uint8_t b = (uint8_t)rand();
    const uint8_t a = 255;
    return (a << 24) | (b << 16) | (g << 8) | r;
}
