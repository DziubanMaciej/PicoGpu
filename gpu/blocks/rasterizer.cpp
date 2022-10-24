#include "gpu/blocks/rasterizer.h"
#include "gpu/isa/isa.h"
#include "gpu/util/conversions.h"
#include "gpu/util/handshake.h"
#include "gpu/util/math.h"

void Rasterizer::rasterize() {
    while (true) {
        wait();

        // Receive vertices from vertex shader
        constexpr static size_t verticesInPrimitive = 3; // only triangles
        constexpr static size_t componentsPerVertex = 4; // x, y, z
        uint32_t receivedVertices[verticesInPrimitive * componentsPerVertex];

        Handshake::receiveArrayWithParallelPorts(previousBlock.inpSending, previousBlock.outReceiving, previousBlock.inpData,
                                                 receivedVertices, verticesInPrimitive * componentsPerVertex, &profiling.outBusy);
        const Point vertices[verticesInPrimitive] = {
            readPoint(receivedVertices, componentsPerVertex, 0),
            readPoint(receivedVertices, componentsPerVertex, 1),
            readPoint(receivedVertices, componentsPerVertex, 2),
        };

        // Setup state of the fragment shader
        // TODO break this up to separate methods
        const size_t maxDataToSendCount = Isa::maxInputOutputRegisters * Isa::registerComponentsCount * verticesInPrimitive;
        uint32_t dataToSend[maxDataToSendCount];
        uint32_t dataToSendCount = 0;
        for (size_t i = 0; i < verticesInPrimitive; i++) {
            dataToSend[dataToSendCount++] = Conversions::floatBytesToUint(vertices[i].x);
            dataToSend[dataToSendCount++] = Conversions::floatBytesToUint(vertices[i].y);
            dataToSend[dataToSendCount++] = Conversions::floatBytesToUint(vertices[i].z);
        }
        Handshake::sendArrayWithParallelPorts(nextBlock.perTriangle.inpReceiving, nextBlock.perTriangle.outSending, nextBlock.perTriangle.outData,
                                              dataToSend, dataToSendCount);

        // Iterate over pixels
        const auto width = framebuffer.inpWidth.read();
        const auto height = framebuffer.inpHeight.read();
        UnshadedFragment currentFragment{};
        for (currentFragment.y = 0; currentFragment.y < height; currentFragment.y++) {
            for (currentFragment.x = 0; currentFragment.x < width; currentFragment.x++) {
                const Point pixel{static_cast<float>(currentFragment.x), static_cast<float>(currentFragment.y)};
                const bool hit = isPointInTriangle(pixel, vertices[0], vertices[1], vertices[2]);
                if (hit) {
                    currentFragment.z = previousBlock.inpData[2]; // TODO: this is incorrect. We should probably interpolate the z-value. Barycentrics???
                    Handshake::send(nextBlock.perFragment.inpReceiving, nextBlock.perFragment.outSending, nextBlock.perFragment.outData, currentFragment);
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
