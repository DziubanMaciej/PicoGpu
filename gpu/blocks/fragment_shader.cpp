#include "gpu/blocks/fragment_shader.h"
#include "gpu/blocks/shader_array/request.h"
#include "gpu/util/conversions.h"
#include "gpu/util/handshake.h"
#include "gpu/util/math.h"

void FragmentShader::main() {
    const auto maxThreadsCount = Isa::simdSize;

    const auto componentsPerInputFragment = 4;
    const auto inputDwords = maxThreadsCount * componentsPerInputFragment;
    struct {
        ShaderFrontendRequest header = {};
        uint32_t data[inputDwords];
    } request;

    const auto componentsPerOutputFragment = 4;
    const auto outputDwords = maxThreadsCount * componentsPerOutputFragment;
    struct {
        ShaderFrontendResponse header = {};
        float data[outputDwords];
    } response;

    ShadedFragment shadedFragments[maxThreadsCount];

    while (true) {
        wait();

        // Receive fragments to shade from previous block. Accumulate them and dispatch together.
        const size_t timeout = 5;
        size_t fragmentsCount = 0;
        for (; fragmentsCount < maxThreadsCount; fragmentsCount++) {
            bool success{};
            UnshadedFragment inputFragment = Handshake::receiveWithTimeout(previousBlock.inpSending, previousBlock.inpData, previousBlock.outReceiving, timeout, success);
            if (!success) {
                break;
            }
            request.data[fragmentsCount * componentsPerInputFragment + 0] = Conversions::floatBytesToUint(static_cast<float>(inputFragment.x.to_int()));
            request.data[fragmentsCount * componentsPerInputFragment + 1] = Conversions::floatBytesToUint(static_cast<float>(inputFragment.y.to_int()));
            request.data[fragmentsCount * componentsPerInputFragment + 2] = Conversions::floatBytesToUint(inputFragment.z.to_int());
            request.data[fragmentsCount * componentsPerInputFragment + 3] = Conversions::floatBytesToUint(1.0f); // TODO we could not send it and have the shader unit do it automatically for FS, but we'd need to add a notion of "Shader model"

            shadedFragments[fragmentsCount].x = inputFragment.x;
            shadedFragments[fragmentsCount].y = inputFragment.y;
            shadedFragments[fragmentsCount].z = inputFragment.z;
        }
        if (fragmentsCount == 0) {
            continue;
        }

        // Send the request to the shading units
        request.header.dword0.isaAddress = inpShaderAddress.read();
        request.header.dword1.clientToken++;
        request.header.dword1.threadCount = intToNonZeroCount(fragmentsCount);
        request.header.dword2.inputsCount = NonZeroCount::One;
        request.header.dword2.inputSize0 = NonZeroCount::Four;
        request.header.dword2.outputsCount = NonZeroCount::One;
        request.header.dword2.outputSize0 = NonZeroCount::Four;
        Handshake::sendArray(shaderFrontend.request.inpReceiving, shaderFrontend.request.outSending,
                             shaderFrontend.request.outData, reinterpret_cast<uint32_t *>(&request), sizeof(request) / sizeof(uint32_t));

        Handshake::receiveArray(shaderFrontend.response.inpSending, shaderFrontend.response.inpData,
                                shaderFrontend.response.outReceiving, reinterpret_cast<uint32_t *>(&response), sizeof(response) / sizeof(uint32_t));

        // Send results to the next block
        for (size_t i = 0; i < fragmentsCount; i++) {
            shadedFragments[i].color = packRgbaToUint(response.data + i * componentsPerOutputFragment);
            Handshake::send(nextBlock.inpReceiving, nextBlock.outSending, nextBlock.outData, shadedFragments[i]);
        }
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