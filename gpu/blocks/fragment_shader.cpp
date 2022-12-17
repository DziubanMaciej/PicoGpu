#include "gpu/blocks/fragment_shader.h"
#include "gpu/blocks/shader_array/request.h"
#include "gpu/util/conversions.h"
#include "gpu/util/math.h"
#include "gpu/util/transfer.h"

void FragmentShader::perTriangleThread() {
    uint32_t data[maxPerTriangleAttribsCount];
    while (true) {
        wait();

        const uint32_t dataToReceiveCount = calculateTriangleAttributesCount(CustomShaderComponents(inpCustomInputComponents.read().to_int()));
        Transfer::receiveArrayWithParallelPorts(previousBlock.perTriangle.inpSending, previousBlock.perTriangle.outReceiving,
                                                previousBlock.perTriangle.inpData, data, dataToReceiveCount);
        for (size_t i = 0; i < dataToReceiveCount; i++) {
            this->perTriangleAttribs[i] = data[i];
        }
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

        // Prepare some info about the request
        CustomShaderComponents customInputComponents{inpCustomInputComponents.read().to_uint()};
        const size_t customInputRegistersCount = customInputComponents.registersCount;
        const uint32_t triangleAttributesCount = calculateTriangleAttributesCount(customInputComponents);

        // Receive fragments to shade from previous block. Accumulate them and dispatch together.
        const size_t timeout = 5;
        size_t fragmentsCount = 0;
        size_t dataDwords = 0;
        for (; fragmentsCount < maxThreadsCount; fragmentsCount++) {
            bool success{};
            UnshadedFragment inputFragment = Transfer::receiveWithTimeout(previousBlock.perFragment.inpSending, previousBlock.perFragment.inpData, previousBlock.perFragment.outReceiving, timeout, success);
            if (!success) {
                break;
            }
            request.data[dataDwords++] = Conversions::floatBytesToUint(static_cast<float>(inputFragment.x.to_int()));
            request.data[dataDwords++] = Conversions::floatBytesToUint(static_cast<float>(inputFragment.y.to_int()));

            shadedFragments[fragmentsCount].x = inputFragment.x;
            shadedFragments[fragmentsCount].y = inputFragment.y;
        }
        if (fragmentsCount == 0) {
            continue;
        }

        // Write uniforms (per-request data)
        const CustomShaderComponents uniformsInfo{this->inpUniforms.read().to_uint()};
        const size_t totalUniformsCount = uniformsInfo.registersCount;
        for (size_t uniformIndex = 0u; uniformIndex < totalUniformsCount; uniformIndex++) {
            const uint32_t componentsCount = uniformsInfo.getCustomComponents(uniformIndex);
            for (size_t componentIndex = 0u; componentIndex < componentsCount; componentIndex++) {
                const uint32_t value = inpUniformsData[uniformIndex][componentIndex].read().to_int();
                request.data[dataDwords++] = value;
            }
        }

        // Write triangle attribs (per-request data)
        for (auto i = 0u; i < triangleAttributesCount; i++) {
            request.data[dataDwords++] = this->perTriangleAttribs[i].read().to_int();
        }

        // Send the request to the shading units
        request.header.dword0.isaAddress = inpShaderAddress.read();
        request.header.dword1.clientToken++;
        request.header.dword1.threadCount = intToNonZeroCount(fragmentsCount);
        request.header.dword1.programType = Isa::Command::ProgramType::FragmentShader;
        request.header.dword2.inputsCount = NonZeroCount::One + intToNonZeroCount(customInputRegistersCount);
        request.header.dword2.inputSize0 = NonZeroCount::Two; // First input is always x,y position
        request.header.dword2.inputSize1 = customInputComponents.comp0;
        request.header.dword2.inputSize2 = customInputComponents.comp1;
        request.header.dword2.outputsCount = NonZeroCount::Two;
        request.header.dword2.outputSize0 = NonZeroCount::Four; // color data r,g,b,a
        request.header.dword2.outputSize1 = NonZeroCount::One;  // interpolated z
        request.header.dword2.uniformsCount = uniformsInfo.registersCount;
        request.header.dword2.uniformSize0 = uniformsInfo.comp0;
        request.header.dword2.uniformSize1 = uniformsInfo.comp1;
        request.header.dword2.uniformSize2 = uniformsInfo.comp2;
        const size_t requestSize = sizeof(ShaderFrontendRequest) / sizeof(uint32_t) + dataDwords;
        Transfer::sendArray(shaderFrontend.request.inpReceiving, shaderFrontend.request.outSending,
                            shaderFrontend.request.outData, reinterpret_cast<uint32_t *>(&request), requestSize);

        Transfer::receiveArray(shaderFrontend.response.inpSending, shaderFrontend.response.inpData,
                               shaderFrontend.response.outReceiving, reinterpret_cast<uint32_t *>(&response), sizeof(response) / sizeof(uint32_t));

        // Send results to the next block
        for (size_t i = 0; i < fragmentsCount; i++) {
            shadedFragments[i].color = packRgbaToUint(response.data + i * componentsPerOutputFragment);
            shadedFragments[i].z = Conversions::floatBytesToUint(response.data[i * componentsPerOutputFragment + 4]);
            Transfer::send(nextBlock.inpReceiving, nextBlock.outSending, nextBlock.outData, shadedFragments[i]);
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

uint32_t FragmentShader::calculateTriangleAttributesCount(CustomShaderComponents customComponents) {
    const uint32_t fixedComponentsPerVertex = 3; // x,y,z
    const uint32_t customComponentsPerVertex = customComponents.getCustomComponentsCount();
    const uint32_t componentsPerVertex = fixedComponentsPerVertex + customComponentsPerVertex;
    return componentsPerVertex * verticesInPrimitive;
}
