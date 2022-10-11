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

    const auto componentsPerOutputVertex = 4; // x, y, z
    const auto dwordsPerOutputPrimitive = verticesPerPrimitive * componentsPerOutputVertex;
    struct {
        ShaderFrontendResponse header = {};
        uint32_t vertexData[dwordsPerOutputPrimitive];
    } response;

    while (true) {
        wait();

        Handshake::receiveArrayWithParallelPorts(previousBlock.inpSending, previousBlock.outReceiving,
                                                 previousBlock.inpData, previousBlock.portsCount,
                                                 request.vertexData, dwordsPerInputPrimitive,
                                                 &profiling.outBusy);

        request.header.dword0.isaAddress = inpShaderAddress.read();
        request.header.dword1.clientToken++;
        request.header.dword1.threadCount = 3;
        request.header.dword2.inputsCount = NonZeroCount::One;
        request.header.dword2.inputSize0 = NonZeroCount::Three;
        request.header.dword2.outputsCount = NonZeroCount::One;
        request.header.dword2.outputSize0 = NonZeroCount::Four;
        Handshake::sendArray(shaderFrontend.request.inpReceiving, shaderFrontend.request.outSending,
                             shaderFrontend.request.outData, reinterpret_cast<uint32_t *>(&request), sizeof(request) / sizeof(uint32_t));

        Handshake::receiveArray(shaderFrontend.response.inpSending, shaderFrontend.response.inpData,
                                shaderFrontend.response.outReceiving, reinterpret_cast<uint32_t *>(&response), sizeof(response) / sizeof(uint32_t));

        Handshake::sendArrayWithParallelPorts(nextBlock.inpReceiving, nextBlock.outSending, nextBlock.outData,
                                              nextBlock.portsCount, response.vertexData, dwordsPerOutputPrimitive);
    }
}
