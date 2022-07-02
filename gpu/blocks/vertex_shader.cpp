#include "gpu/blocks/shader_array/request.h"
#include "gpu/blocks/vertex_shader.h"
#include "gpu/util/handshake.h"

void VertexShader::main() {
    const auto verticesPerPrimitive = 3; // only triangles
    const auto componentsPerVertex = 3;  // x, y, z
    const auto dwordsPerPrimitive = verticesPerPrimitive * componentsPerVertex;
    struct {
        ShaderFrontendRequest header = {};
        uint32_t vertexData[dwordsPerPrimitive];
    } request;
    struct {
        ShaderFrontendResponse header = {};
        uint32_t vertexData[dwordsPerPrimitive];
    } response;

    while (true) {
        wait();

        Handshake::receiveArrayWithParallelPorts(previousBlock.inpSending, previousBlock.outReceiving,
                                                 previousBlock.inpData, previousBlock.portsCount,
                                                 request.vertexData, dwordsPerPrimitive);

        request.header.dword0.isaAddress = inpShaderAddress.read();
        request.header.dword1.clientToken++;
        request.header.dword1.threadCount = 3;
        request.header.dword2.inputsCount = NonZeroCount::One;
        request.header.dword2.inputSize0 = NonZeroCount::Three;
        request.header.dword2.outputsCount = NonZeroCount::One;
        request.header.dword2.outputSize0 = NonZeroCount::Three;
        Handshake::sendArray(shaderFrontend.request.inpReceiving, shaderFrontend.request.outSending,
                             shaderFrontend.request.outData, reinterpret_cast<uint32_t *>(&request), sizeof(request) / sizeof(uint32_t));

        Handshake::receiveArray(shaderFrontend.response.inpSending, shaderFrontend.response.inpData,
                                shaderFrontend.response.outReceiving, reinterpret_cast<uint32_t *>(&response), sizeof(response) / sizeof(uint32_t));

        Handshake::sendArrayWithParallelPorts(nextBlock.inpReceiving, nextBlock.outSending, nextBlock.outData,
                                              nextBlock.portsCount, response.vertexData, dwordsPerPrimitive);
    }
}
