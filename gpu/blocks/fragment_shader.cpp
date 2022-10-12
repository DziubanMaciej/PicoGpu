#include "gpu/blocks/fragment_shader.h"
#include "gpu/blocks/shader_array/request.h"
#include "gpu/util/conversions.h"
#include "gpu/util/handshake.h"
#include "gpu/util/math.h"

void FragmentShader::main() {
    const auto inputDwords = 4;
    struct {
        ShaderFrontendRequest header = {};
        uint32_t data[inputDwords];
    } request;

    const auto outputDwords = 4;
    struct {
        ShaderFrontendResponse header = {};
        float data[outputDwords];
    } response;

    while (true) {
        wait();

        UnshadedFragment inputFragment = Handshake::receive(previousBlock.inpSending, previousBlock.inpData, previousBlock.outReceiving, &profiling.outBusy);
        request.data[0] = Conversions::floatBytesToUint(static_cast<float>(inputFragment.x.to_int()));
        request.data[1] = Conversions::floatBytesToUint(static_cast<float>(inputFragment.y.to_int()));
        request.data[2] = Conversions::floatBytesToUint(inputFragment.z.to_int());
        request.data[3] = Conversions::floatBytesToUint(1.0f); // TODO we could not send it and have the shader unit do it automatically for FS, but we'd need to add a notion of "Shader model"

        request.header.dword0.isaAddress = inpShaderAddress.read();
        request.header.dword1.clientToken++;
        request.header.dword1.threadCount = 1;
        request.header.dword2.inputsCount = NonZeroCount::One;
        request.header.dword2.inputSize0 = NonZeroCount::Four;
        request.header.dword2.outputsCount = NonZeroCount::One;
        request.header.dword2.outputSize0 = NonZeroCount::Four;
        Handshake::sendArray(shaderFrontend.request.inpReceiving, shaderFrontend.request.outSending,
                             shaderFrontend.request.outData, reinterpret_cast<uint32_t *>(&request), sizeof(request) / sizeof(uint32_t));

        Handshake::receiveArray(shaderFrontend.response.inpSending, shaderFrontend.response.inpData,
                                shaderFrontend.response.outReceiving, reinterpret_cast<uint32_t *>(&response), sizeof(response) / sizeof(uint32_t));

        ShadedFragment outputFragment;
        outputFragment.x = inputFragment.x;
        outputFragment.y = inputFragment.y;
        outputFragment.z = inputFragment.z;
        outputFragment.color = packRgbaToUint(response.data);
        Handshake::send(nextBlock.inpReceiving, nextBlock.outSending, nextBlock.outData, outputFragment);
    }
}

uint32_t FragmentShader::packRgbaToUint(float *rgba) {
    uint32_t result = 0;
    result |= static_cast<uint32_t>(saturate(rgba[0]) * 255) << 0;
    result |= static_cast<uint32_t>(saturate(rgba[1]) * 255) << 8;
    result |= static_cast<uint32_t>(saturate(rgba[2]) * 255) << 16;
    result |= static_cast<uint32_t>(saturate(rgba[3]) * 255) << 24;
    return result;
}