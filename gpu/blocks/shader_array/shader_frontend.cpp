
#include "gpu/blocks/shader_array/shader_frontend.h"
#include "gpu/isa/isa.h"
#include "gpu/util/error.h"
#include "gpu/util/handshake.h"

void ShaderFrontendBase::requestThread() {
    initIsaCache();

    constexpr bool handshakeOnlyOnce = true;
    constexpr size_t maxShaderInputsCount = 4 * Isa::inputRegistersCount * Isa::simdSize;
    uint32_t shaderInput[maxShaderInputsCount] = {};

    while (true) {
        wait();

        ClientInterface *clientInterface = {};
        size_t *clientIndex = {};
        if (!findClientMakingRequest(&clientInterface, &clientIndex)) {
            continue;
        }

        ShaderUnitInterface *shaderUnitInterface = {};
        ShaderUnitState *shaderUnitState = {};
        if (!findFreeShaderUnit(&shaderUnitInterface, &shaderUnitState)) {
            continue;
        }

        // Read request metadata
        ShaderFrontendRequest request = {};
        request.dword0.raw = Handshake::receive(clientInterface->request.inpSending, clientInterface->request.inpData, clientInterface->request.outReceiving).to_int();
        wait();
        request.dword1.raw = clientInterface->request.inpData.read();
        wait();
        request.dword2.raw = clientInterface->request.inpData.read();

        // Read shader inputs
        // TODO: this could be done in parallel somehow...
        const uint32_t shaderInputsCount = calculateShaderInputsCount(request);
        for (int i = 0; i < shaderInputsCount; i++) {
            wait();
            shaderInput[i] = clientInterface->request.inpData.read();
        }

        // Out shader unit is stateful and the ISA to execute may already be loaded in it. If not, we have to read it from memory
        // and store it in the shader unit
        // TODO this stalls any other requests, that may have ISA already loaded. We could parallelize this somehow. There could be
        // a separate block that would be responsible for loading ISA from memory, caching it and sending to the shader units.
        // However, shader frontend would have to wait for it.
        if (request.dword0.isaAddress != shaderUnitState->loadedIsaAddress) {
            int isaCacheIndex = getIsa(request.dword0.isaAddress);
            storeIsa(*shaderUnitInterface, isaCacheIndex, handshakeOnlyOnce);
        }

        // At this point shader unit already know what it has to execute. We only have to issue the command and send the inputs to it.
        executeIsa(*shaderUnitInterface, handshakeOnlyOnce, shaderInput, shaderInputsCount);

        // Cache data about this request, so we can use it in responseThread
        shaderUnitState->request.isActive = true;
        shaderUnitState->request.clientIndex = *clientIndex;
        shaderUnitState->request.clientToken = request.dword1.clientToken;
        shaderUnitState->request.outputsCount = calculateShaderOutputsCount(request);
    }
}

void ShaderFrontendBase::responseThread() {
    constexpr size_t maxShaderOutputsCount = 4 * Isa::outputRegistersCount * Isa::simdSize + sizeof(ShaderFrontendResponse) / sizeof(uint32_t);
    uint32_t shaderOutputs[maxShaderOutputsCount] = {};

    while (1) {
        wait();

        ShaderUnitInterface *shaderUnitInterface = {};
        ShaderUnitState *shaderUnitState = {};
        ClientInterface *clientInterface = {};
        if (!findShaderUnitSendingResponse(&shaderUnitInterface, &shaderUnitState, &clientInterface)) {
            continue;
        }
        FATAL_ERROR_IF(!shaderUnitState->request.isActive, "Shader unit tried to return results for no reason");

        // Prepare response header
        uint32_t shaderOutputsCount = 0;
        ShaderFrontendResponse response = {};
        response.dword0.clientToken = shaderUnitState->request.clientToken;
        shaderOutputs[shaderOutputsCount++] = response.dword0.raw;

        // Read results from shader unit
        auto &unit = shaderUnitInterface->response;
        shaderOutputs[shaderOutputsCount++] = Handshake::receive(unit.inpSending, unit.inpData, unit.outReceiving);
        for (int i = 1; i < shaderUnitState->request.outputsCount; i++) {
            wait();
            shaderOutputs[shaderOutputsCount++] = unit.inpData.read();
        }

        // Free the shader unit by marking it as not busy
        shaderUnitState->request.isActive = false;

        // Send results to client
        auto &client = clientInterface[shaderUnitState->request.clientIndex].response;
        sc_uint<32> data = shaderOutputs[0];
        Handshake::send(client.inpReceiving, client.outSending, client.outData, data);
        for (int i = 1; i < shaderOutputsCount; i++) {
            client.outData = shaderOutputs[i];
            wait();
        }
    }
}

void ShaderFrontendBase::initIsaCache() {
    for (auto &entry : isaCache.entries) {
        entry.address = std::numeric_limits<uint32_t>::max();
        entry.dataSize = 0;
    }
    for (int i = 0; i < isaCacheSize; i++) {
        isaCache.lru[i] = i;
    }
}

int ShaderFrontendBase::getIsa(uint32_t isaAddress) {
    // First look in cache
    bool cacheHit = false;
    int indexInCache = -1;
    for (int i = 0; i < isaCacheSize; i++) {
        if (isaCache.entries[i].address == isaAddress) {
            cacheHit = true;
            indexInCache = i;
            break;

            // TODO promote this index to the beginning of LRU
        }
    }

    // If did not found in cache, load from memory and store in cache
    if (!cacheHit) {
        // Index of least (not last) recently used entry is at the end of LRU structure. Shift it to the right and
        // store new entry at index 0. This could be implemented more efficiently with a ring buffer, but cache size
        // will generally be small, so it shouldn't matter that much.
        indexInCache = isaCache.lru[isaCacheSize - 1];
        for (int i = isaCacheSize - 1; i >= 1; i--) {
            isaCache.lru[i] = isaCache.lru[i - 1];
        }
        isaCache.lru[0] = indexInCache;
        auto &cacheEntry = isaCache.entries[indexInCache];
        cacheEntry.address = isaAddress;

        // Memory loads
        size_t dwordsLoaded = 0;
        size_t dwordsToLoad = 1;
        while (dwordsToLoad > 0) {
            FATAL_ERROR_IF(dwordsLoaded >= Isa::maxIsaSize, "Too big ISA to fit in cache");

            // Load from memory
            memory.outAddress = isaAddress + 4 * dwordsLoaded;
            memory.outEnable = 1;
            wait();
            memory.outEnable = 0;
            while (!memory.inpCompleted) {
                wait();
            }
            memory.outAddress = 0;

            // Store in cache
            uint32_t dword = memory.inpData.read().to_int();
            cacheEntry.data[dwordsLoaded] = dword;

            // Update counters
            dwordsLoaded++;
            dwordsToLoad--;

            // If this is a first dword we've read, we're looking at the command header with metadata, which we have to process
            if (dwordsLoaded == 1) {
                auto command = reinterpret_cast<Isa::Command::CommandStoreIsa &>(dword);
                FATAL_ERROR_IF(command.commandType != Isa::Command::CommandType::StoreIsa, "Invalid command header");
                dwordsToLoad = command.programLength;
            }
        }

        cacheEntry.dataSize = dwordsLoaded;
    }

    // Return result
    return indexInCache;
}

void ShaderFrontendBase::storeIsa(ShaderUnitInterface &shaderUnitInterface, int indexInIsaCache, bool hasNextCommand) {
    auto &cache = isaCache.entries[indexInIsaCache];
    auto &unit = shaderUnitInterface.request;

    reinterpret_cast<Isa::Command::CommandStoreIsa &>(cache.data[0]).hasNextCommand = hasNextCommand;

    sc_uint<32> data = cache.data[0];
    Handshake::send(unit.inpReceiving, unit.outSending, unit.outData, data);
    for (int i = 1; i < cache.dataSize; i++) {
        data = cache.data[i];
        unit.outData = data;
        wait();
    }
}

void ShaderFrontendBase::executeIsa(ShaderUnitInterface &shaderUnitInterface, bool handshakeAlreadyDone, const uint32_t *shaderInputs, uint32_t shaderInputsCount) {
    auto &unit = shaderUnitInterface.request;

    // Send the command
    Isa::Command::CommandExecuteIsa command;
    command.commandType = Isa::Command::CommandType::ExecuteIsa;
    command.hasNextCommand = 0;
    command.threadCount = 1;
    if (handshakeAlreadyDone) {
        unit.outData = command.raw;
        wait();
    } else {
        sc_uint<32> data = command.raw;
        Handshake::send(unit.inpReceiving, unit.outSending, unit.outData, data);
    }

    // Send the shader inputs
    for (int i = 0; i < shaderInputsCount; i++) {
        unit.outData = shaderInputs[i];
        wait();
    }
}

size_t ShaderFrontendBase::calculateShaderInputsCount(const ShaderFrontendRequest &request) {
    size_t components = 0;

    int registersCount = Isa::Command::nonZeroCountToInt(request.dword2.inputsCount);
    if (registersCount > 0) {
        components += Isa::Command::nonZeroCountToInt(request.dword2.inputSize0);
    }
    if (registersCount > 1) {
        components += Isa::Command::nonZeroCountToInt(request.dword2.inputSize1);
    }
    if (registersCount > 2) {
        components += Isa::Command::nonZeroCountToInt(request.dword2.inputSize2);
    }
    if (registersCount > 3) {
        components += Isa::Command::nonZeroCountToInt(request.dword2.inputSize3);
    }

    components *= request.dword1.threadCount;
    return components;
}

size_t ShaderFrontendBase::calculateShaderOutputsCount(const ShaderFrontendRequest &request) {
    size_t components = 0;

    int registersCount = Isa::Command::nonZeroCountToInt(request.dword2.outputsCount);
    if (registersCount > 0) {
        components += Isa::Command::nonZeroCountToInt(request.dword2.outputSize0);
    }
    if (registersCount > 1) {
        components += Isa::Command::nonZeroCountToInt(request.dword2.outputSize1);
    }
    if (registersCount > 2) {
        components += Isa::Command::nonZeroCountToInt(request.dword2.outputSize2);
    }
    if (registersCount > 3) {
        components += Isa::Command::nonZeroCountToInt(request.dword2.outputSize3);
    }

    components *= request.dword1.threadCount;
    return components;
}

void ShaderFrontendBase::validateRequest(const ShaderFrontendRequest &request, Isa::Command::CommandStoreIsa &isaCommand) {
    FATAL_ERROR_IF(isaCommand.inputsCount != request.dword2.inputsCount, "Invalid inputs count");
    FATAL_ERROR_IF(isaCommand.inputSize0 != request.dword2.inputSize0, "Invalid inputSize0");
    FATAL_ERROR_IF(isaCommand.inputSize1 != request.dword2.inputSize1, "Invalid inputSize1");
    FATAL_ERROR_IF(isaCommand.inputSize2 != request.dword2.inputSize2, "Invalid inputSize2");
    FATAL_ERROR_IF(isaCommand.inputSize3 != request.dword2.inputSize3, "Invalid inputSize3");
    FATAL_ERROR_IF(isaCommand.outputsCount != request.dword2.outputsCount, "Invalid outputs count");
    FATAL_ERROR_IF(isaCommand.outputSize0 != request.dword2.outputSize0, "Invalid outputSize0");
    FATAL_ERROR_IF(isaCommand.outputSize1 != request.dword2.outputSize1, "Invalid outputSize1");
    FATAL_ERROR_IF(isaCommand.outputSize2 != request.dword2.outputSize2, "Invalid outputSize2");
    FATAL_ERROR_IF(isaCommand.outputSize3 != request.dword2.outputSize3, "Invalid outputSize3");
}
