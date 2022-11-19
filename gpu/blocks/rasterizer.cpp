#include "gpu/blocks/rasterizer.h"
#include "gpu/isa/isa.h"
#include "gpu/util/conversions.h"
#include "gpu/util/handshake.h"
#include "gpu/util/math.h"

void Rasterizer::receiveFromVs(uint32_t customComponentsPerVertex, Point *outVertices) {
    const uint32_t componentsPerVertex = 4 + customComponentsPerVertex; // x,y,z,w position + custom attributes

    uint32_t receivedVertices[verticesInPrimitive * componentsPerVertex];
    Handshake::receiveArrayWithParallelPorts(previousBlock.inpSending, previousBlock.outReceiving, previousBlock.inpData,
                                             receivedVertices, verticesInPrimitive * componentsPerVertex, &profiling.outBusy);

    for (uint32_t i = 0; i < verticesInPrimitive; i++) {
        outVertices[i] = readPoint(receivedVertices, componentsPerVertex, customComponentsPerVertex, i);
    }
}

void Rasterizer::setupPerTriangleFsState(uint32_t customComponentsPerVertex, Point *vertices) {
    const size_t maxDataToSendCount = Isa::maxInputOutputRegisters * Isa::registerComponentsCount * verticesInPrimitive;
    uint32_t dataToSend[maxDataToSendCount];
    uint32_t dataToSendCount = 0;
    for (size_t i = 0; i < verticesInPrimitive; i++) {
        dataToSend[dataToSendCount++] = Conversions::floatBytesToUint(vertices[i].x);
        dataToSend[dataToSendCount++] = Conversions::floatBytesToUint(vertices[i].y);
        dataToSend[dataToSendCount++] = Conversions::floatBytesToUint(vertices[i].z);
        for (size_t j = 0; j < customComponentsPerVertex; j++) {
            dataToSend[dataToSendCount++] = Conversions::floatBytesToUint(vertices[i].customComponents[j]);
        }
    }

    Handshake::sendArrayWithParallelPorts(nextBlock.perTriangle.inpReceiving, nextBlock.perTriangle.outSending, nextBlock.perTriangle.outData,
                                          dataToSend, dataToSendCount);
}

void Rasterizer::rasterize(Point *vertices) {
    const auto width = framebuffer.inpWidth.read();
    const auto height = framebuffer.inpHeight.read();

    UnshadedFragment currentFragment{};
    for (currentFragment.y = 0; currentFragment.y < height; currentFragment.y++) {
        for (currentFragment.x = 0; currentFragment.x < width; currentFragment.x++) {
            const Point pixel{static_cast<float>(currentFragment.x), static_cast<float>(currentFragment.y)};
            const bool hit = isPointInTriangle(pixel, vertices[0], vertices[1], vertices[2]);
            if (hit) {
                Handshake::send(nextBlock.perFragment.inpReceiving, nextBlock.perFragment.outSending, nextBlock.perFragment.outData, currentFragment);
                profiling.outFragmentsProduced = profiling.outFragmentsProduced.read() + 1;
            }
        }
    }
}

void Rasterizer::main() {
    Point vertices[verticesInPrimitive];

    while (true) {
        wait();

        const uint32_t customComponentsPerVertex = CustomShaderComponents(this->inpCustomVsPsComponents.read().to_uint()).getCustomComponentsCount();
        receiveFromVs(customComponentsPerVertex, vertices);
        setupPerTriangleFsState(customComponentsPerVertex, vertices);
        rasterize(vertices);
    }
}

Point Rasterizer::readPoint(const uint32_t *receivedVertices, size_t stride, size_t customComponentsCount, size_t pointIndex) {
    Point point;
    const float w = Conversions::uintBytesToFloat(receivedVertices[pointIndex * stride + 3]);
    FATAL_ERROR_IF(w == 0, "Homogeneous coordinate is 0");
    point.x = Conversions::uintBytesToFloat(receivedVertices[pointIndex * stride + 0]) / w;
    point.y = Conversions::uintBytesToFloat(receivedVertices[pointIndex * stride + 1]) / w;
    point.z = Conversions::uintBytesToFloat(receivedVertices[pointIndex * stride + 2]) / w;
    point.w = w;

    for (size_t i = 0u; i < customComponentsCount; i++) {
        point.customComponents[i] = Conversions::uintBytesToFloat(receivedVertices[pointIndex * stride + 4 + i]);
    }

    return point;
}
