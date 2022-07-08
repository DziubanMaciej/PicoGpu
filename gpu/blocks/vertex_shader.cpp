#include "gpu/blocks/shader_array/request.h"
#include "gpu/blocks/vertex_shader.h"
#include "gpu/util/handshake.h"

constexpr static size_t verticesPerPrimitive = 3; // only triangles
constexpr static size_t componentsPerVertex = 3;  // x, y, z
constexpr static size_t dwordsPerPrimitive = verticesPerPrimitive * componentsPerVertex;

constexpr static size_t maxComponentsPerVertex = 16;
constexpr static size_t maxVerticesPerDispatch = Isa::simdSize;
constexpr static size_t maxDwordsPerDispatch = maxVerticesPerDispatch * maxComponentsPerVertex * sizeof(uint32_t);

void VertexShader::processReceiveFromPreviousBlock() {
    while (true) {
        wait();

        uint32_t *vertexData = nullptr;
        while (vertexData == nullptr) {
            vertexData = verticesBeforeShade.acquireWriteBuffer(dwordsPerPrimitive);
            wait();
        }

        Handshake::receiveArrayWithParallelPorts(previousBlock.inpSending, previousBlock.outReceiving,
                                                 previousBlock.inpData, previousBlock.portsCount,
                                                 vertexData, dwordsPerPrimitive,
                                                 &profiling.outBusy);

        verticesBeforeShade.releaseWriteBuffer(vertexData, dwordsPerPrimitive);
    }
}

void VertexShader::processSendForShading() {
    struct {
        ShaderFrontendRequest header = {};
        uint32_t vertexData[maxDwordsPerDispatch];
    } request;

    while (true) {
        wait();

        // Retrieve vertices to shade from another process
        size_t vertexDataSize = 0;
        const uint32_t *vertexData = nullptr;
        while (vertexData == nullptr) {
            vertexData = verticesBeforeShade.acquireReadBuffer(dwordsPerPrimitive, vertexDataSize);
            wait();
        }

        // Prepare request packet
        request.header.dword0.isaAddress = inpShaderAddress.read();
        request.header.dword1.clientToken++;
        request.header.dword1.threadCount = vertexDataSize / componentsPerVertex;
        request.header.dword2.inputsCount = NonZeroCount::One;
        request.header.dword2.inputSize0 = NonZeroCount::Three;
        request.header.dword2.outputsCount = NonZeroCount::One;
        request.header.dword2.outputSize0 = NonZeroCount::Three;
        std::copy_n(vertexData, vertexDataSize, request.vertexData);

        // Send request packet to shading units
        Handshake::sendArray(shaderFrontend.request.inpReceiving, shaderFrontend.request.outSending, shaderFrontend.request.outData,
                             reinterpret_cast<uint32_t *>(&request), vertexDataSize + sizeof(ShaderFrontendRequest) / sizeof(uint32_t));
        verticesBeforeShade.releaseReadBuffer(vertexData);
        shadingTasks[request.header.dword1.clientToken] = request.header.dword1.threadCount;
    }
}

void VertexShader::processReceiveFromShading() {
    struct {
        ShaderFrontendResponse header = {};
        uint32_t vertexData[maxDwordsPerDispatch];
    } response;

    while (1) {
        wait();

        // Receive response packet header from ShaderFrontend
        Handshake::receiveArray(shaderFrontend.response.inpSending, shaderFrontend.response.inpData, shaderFrontend.response.outReceiving,
                                reinterpret_cast<uint32_t *>(&response.header), sizeof(ShaderFrontendResponse) / sizeof(uint32_t));
        const auto taskIterator = shadingTasks.find(response.header.dword0.clientToken);
        FATAL_ERROR_IF(taskIterator == shadingTasks.end(), "Unexpected response from ShaderFrontend");
        const auto threadsCount = taskIterator->second;
        shadingTasks.erase(taskIterator);

        // Receive the rest of response packet
        const uint32_t vertexDataSize = threadsCount * componentsPerVertex;
        for (uint32_t i = 0; i < vertexDataSize; i++) {
            wait();
            response.vertexData[i] = shaderFrontend.response.inpData.read();
        }

        // Pass the shaded vertex data to next process
        uint32_t *vertexData = nullptr;
        while (vertexData == nullptr) {
            vertexData = verticesAfterShade.acquireWriteBuffer(vertexDataSize);
            wait();
        }
        std::copy_n(response.vertexData, vertexDataSize, vertexData);
        verticesAfterShade.releaseWriteBuffer(vertexData, vertexDataSize);
    }
}

void VertexShader::processSendToNextBlock() {
    while (1) {
        wait();

        // 
        const uint32_t *vertexData = nullptr;
        size_t vertexDataSize = 0;
        while (vertexData == nullptr) {
            vertexData = verticesAfterShade.acquireReadBuffer(dwordsPerPrimitive, vertexDataSize);
            wait();
        }

        const size_t primitiveCount = vertexDataSize / dwordsPerPrimitive;
        for (size_t i = 0; i < primitiveCount; i++) {
            Handshake::sendArrayWithParallelPorts(nextBlock.inpReceiving, nextBlock.outSending, nextBlock.outData,
                                                  nextBlock.portsCount, vertexData + i * dwordsPerPrimitive, dwordsPerPrimitive);
        }

        verticesAfterShade.releaseReadBuffer(vertexData);
    }
}
