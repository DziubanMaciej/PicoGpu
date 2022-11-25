#include "gpu/blocks/rasterizer.h"
#include "gpu/isa/isa.h"
#include "gpu/util/conversions.h"
#include "gpu/util/transfer.h"

void Rasterizer::receiveFromVs(uint32_t customComponentsPerVertex, Point *outVertices) {
    const uint32_t componentsPerVertex = 4 + customComponentsPerVertex; // x,y,z,w position + custom attributes

    uint32_t receivedVertices[verticesInPrimitive * componentsPerVertex];
    Transfer::receiveArrayWithParallelPorts(previousBlock.inpSending, previousBlock.outReceiving, previousBlock.inpData,
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

    Transfer::sendArrayWithParallelPorts(nextBlock.perTriangle.inpReceiving, nextBlock.perTriangle.outSending, nextBlock.perTriangle.outData,
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
                Transfer::send(nextBlock.perFragment.inpReceiving, nextBlock.perFragment.outSending, nextBlock.perFragment.outData, currentFragment);
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

float Rasterizer::sign(Point p1, Point p2, Point p3) {
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

bool Rasterizer::isPointInTriangle(Point pt, Point v1, Point v2, Point v3) {
    float d1, d2, d3;
    bool has_neg, has_pos;

    d1 = sign(pt, v1, v2);
    d2 = sign(pt, v2, v3);
    d3 = sign(pt, v3, v1);

    has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}

Rasterizer::Point Rasterizer::readPoint(const uint32_t *receivedVertices, size_t stride, size_t customComponentsCount, size_t pointIndex) {
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
