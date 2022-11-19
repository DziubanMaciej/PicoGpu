#include "gpu/blocks/shader_array/request.h"
#include "gpu/blocks/vertex_shader.h"
#include "gpu/util/handshake.h"

void VertexShader::main() {
    const auto verticesPerPrimitive = 3; // only triangles

    const auto componentsPerInputVertex = 3;
    const auto dwordsPerInputPrimitive = verticesPerPrimitive * componentsPerInputVertex;
    struct {
        ShaderFrontendRequest header = {};
        uint32_t vertexData[dwordsPerInputPrimitive];
    } request;

    const auto maxDwordsPerOutputPrimitive = verticesPerPrimitive * Isa::registerComponentsCount * Isa::maxInputOutputRegisters;
    struct {
        ShaderFrontendResponse header = {};
        uint32_t vertexData[maxDwordsPerOutputPrimitive];
    } response;

    while (true) {
        wait();

        // Receive triangle data
        Handshake::receiveArrayWithParallelPorts(previousBlock.inpSending, previousBlock.outReceiving, previousBlock.inpData,
                                                 request.vertexData, dwordsPerInputPrimitive, &profiling.outBusy);

        // Prepare some info about the request
        CustomShaderComponents customOutputComponents{this->inpCustomOutputComponents.read().to_uint()};
        const size_t customOutputRegistersCount = customOutputComponents.registersCount;
        const size_t threadCount = 3;

        // Prepare request to the shading units
        request.header.dword0.isaAddress = inpShaderAddress.read();
        request.header.dword1.clientToken++;
        request.header.dword1.threadCount = intToNonZeroCount(threadCount);
        request.header.dword2.inputsCount = NonZeroCount::One;
        request.header.dword2.inputSize0 = NonZeroCount::Three;
        request.header.dword2.outputsCount = NonZeroCount::One + intToNonZeroCount(customOutputRegistersCount);
        request.header.dword2.outputSize0 = NonZeroCount::Four;
        if (customOutputRegistersCount > 0) {
            request.header.dword2.outputSize1 = customOutputComponents.comp0;
        }
        if (customOutputRegistersCount > 1) {
            request.header.dword2.outputSize2 = customOutputComponents.comp1;
        }
        if (customOutputRegistersCount > 2) {
            request.header.dword2.outputSize3 = customOutputComponents.comp2;
        }

        // Perform the request
        const size_t dwordsToSend = sizeof(request) / sizeof(uint32_t);
        Handshake::sendArray(shaderFrontend.request.inpReceiving, shaderFrontend.request.outSending,
                             shaderFrontend.request.outData, reinterpret_cast<uint32_t *>(&request), dwordsToSend);
        const size_t dwordsToReceive = sizeof(ShaderFrontendResponse) / sizeof(uint32_t) + (4 + customOutputComponents.getCustomComponentsCount()) * threadCount;
        Handshake::receiveArray(shaderFrontend.response.inpSending, shaderFrontend.response.inpData,
                                shaderFrontend.response.outReceiving, reinterpret_cast<uint32_t *>(&response), dwordsToReceive);

        // Send results for rasterization
        Handshake::sendArrayWithParallelPorts(nextBlock.inpReceiving, nextBlock.outSending, nextBlock.outData, response.vertexData, dwordsToReceive);
    }
}
