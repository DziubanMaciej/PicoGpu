#include "gpu/blocks/fragment_shader.h"
#include "gpu/blocks/shader_array/request.h"
#include "gpu/util/conversions.h"
#include "gpu/util/handshake.h"
#include "gpu/util/math.h"

void FragmentShader::perTriangleThread() {
    uint32_t data[maxTriangleAttributesCount];
    while (true) {
        wait();

        const size_t dataToReceiveCount = 9; // TODO calculate it based on vs output state
        Handshake::receiveArrayWithParallelPorts(previousBlock.perTriangle.inpSending, previousBlock.perTriangle.outReceiving,
                                                 previousBlock.perTriangle.inpData, data, dataToReceiveCount);
        for (size_t i = 0; i < dataToReceiveCount; i++) {
            this->triangleAttributes[i] = data[i];
        }
        this->triangleAttributesCount = dataToReceiveCount;
    }
}

void FragmentShader::perFragmentThread() {
    const auto maxThreadsCount = Isa::simdSize;
    const auto verticesInTriangle = 3u;

    const auto perThreadInputDwords = maxThreadsCount * 2;
    const auto perRequestInputDwords = verticesInTriangle * Isa::maxInputOutputRegisters * Isa::registerComponentsCount;
    struct {
        ShaderFrontendRequest header = {};
        uint32_t data[perThreadInputDwords + perRequestInputDwords];
    } request;

    const auto componentsPerOutputFragment = 5; // RGBA + interpolated depth value
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
        size_t dataDwords = 0;
        for (; fragmentsCount < maxThreadsCount; fragmentsCount++) {
            bool success{};
            UnshadedFragment inputFragment = Handshake::receiveWithTimeout(previousBlock.perFragment.inpSending, previousBlock.perFragment.inpData, previousBlock.perFragment.outReceiving, timeout, success);
            if (!success) {
                break;
            }
            request.data[dataDwords++] = Conversions::floatBytesToUint(static_cast<float>(inputFragment.x.to_int()));
            request.data[dataDwords++] = Conversions::floatBytesToUint(static_cast<float>(inputFragment.y.to_int()));

            shadedFragments[fragmentsCount].x = inputFragment.x;
            shadedFragments[fragmentsCount].y = inputFragment.y;
            shadedFragments[fragmentsCount].z = inputFragment.z;
        }
        if (fragmentsCount == 0) {
            continue;
        }

        // Write per dispatch data to the request
        const uint32_t triangleAttributesCount = this->triangleAttributesCount.read().to_int();
        for (auto i = 0u; i < triangleAttributesCount; i++) {
            request.data[dataDwords++] = this->triangleAttributes[i].read().to_int();
        }

        // Send the request to the shading units
        request.header.dword0.isaAddress = inpShaderAddress.read();
        request.header.dword1.clientToken++;
        request.header.dword1.threadCount = intToNonZeroCount(fragmentsCount);
        request.header.dword1.programType = Isa::Command::ProgramType::FragmentShader;
        request.header.dword2.inputsCount = NonZeroCount::One;
        request.header.dword2.inputSize0 = NonZeroCount::Four;
        request.header.dword2.outputsCount = NonZeroCount::Two;
        request.header.dword2.outputSize0 = NonZeroCount::Four;
        request.header.dword2.outputSize1 = NonZeroCount::One;
        const size_t requestSize = sizeof(ShaderFrontendRequest) + dataDwords;
        Handshake::sendArray(shaderFrontend.request.inpReceiving, shaderFrontend.request.outSending,
                             shaderFrontend.request.outData, reinterpret_cast<uint32_t *>(&request), requestSize);

        Handshake::receiveArray(shaderFrontend.response.inpSending, shaderFrontend.response.inpData,
                                shaderFrontend.response.outReceiving, reinterpret_cast<uint32_t *>(&response), sizeof(response) / sizeof(uint32_t));

        // Send results to the next block
        for (size_t i = 0; i < fragmentsCount; i++) {
            shadedFragments[i].color = packRgbaToUint(response.data + i * componentsPerOutputFragment);
            shadedFragments[i].z = Conversions::floatBytesToUint(response.data[i * componentsPerOutputFragment + 4]);
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