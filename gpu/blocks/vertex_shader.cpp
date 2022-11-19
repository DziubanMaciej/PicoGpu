#include "gpu/blocks/shader_array/request.h"
#include "gpu/blocks/vertex_shader.h"
#include "gpu/util/handshake.h"

void VertexShader::main() {
    const auto maxDwordsPerInputPrimitive = verticesInPrimitive * Isa::registerComponentsCount * Isa::maxInputOutputRegisters;
    struct {
        ShaderFrontendRequest header = {};
        uint32_t vertexData[maxDwordsPerInputPrimitive];
    } request;

    const auto maxDwordsPerOutputPrimitive = verticesInPrimitive * Isa::registerComponentsCount * Isa::maxInputOutputRegisters;
    struct {
        ShaderFrontendResponse header = {};
        uint32_t vertexData[maxDwordsPerOutputPrimitive];
    } response;

    while (true) {
        wait();

        // Prepare some info about our input
        CustomShaderComponents inputComponentsInfo{this->inpCustomInputComponents.read().to_uint()};
        const size_t totalInputComponents = verticesInPrimitive * inputComponentsInfo.getCustomComponentsCount();
        const size_t inputRegistersCount = inputComponentsInfo.registersCount;

        // Receive triangle data
        Handshake::receiveArrayWithParallelPorts(previousBlock.inpSending, previousBlock.outReceiving, previousBlock.inpData,
                                                 request.vertexData, totalInputComponents, &profiling.outBusy);

        // Prepare some info about the request
        CustomShaderComponents customOutputComponents{this->inpCustomOutputComponents.read().to_uint()};
        const size_t customOutputRegistersCount = customOutputComponents.registersCount;
        const size_t threadCount = 3;

        // Prepare request to the shading units
        request.header.dword0.isaAddress = inpShaderAddress.read();
        request.header.dword1.clientToken++;
        request.header.dword1.threadCount = intToNonZeroCount(threadCount);
        request.header.dword2.inputsCount = intToNonZeroCount(inputComponentsInfo.registersCount);
        if (inputRegistersCount > 0) {
            request.header.dword2.inputSize0 = inputComponentsInfo.comp0;
        }
        if (inputRegistersCount > 1) {
            request.header.dword2.inputSize1 = inputComponentsInfo.comp1;
        }
        if (inputRegistersCount > 2) {
            request.header.dword2.inputSize2 = inputComponentsInfo.comp2;
        }
        if (inputRegistersCount > 3) {
            request.header.dword2.inputSize3 = inputComponentsInfo.comp3;
        }
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
