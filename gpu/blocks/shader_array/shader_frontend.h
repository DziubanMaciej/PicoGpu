#pragma once

#include "gpu/blocks/shader_array/request.h"
#include "gpu/definitions/types.h"
#include "gpu/isa/isa.h"
#include "gpu/util/entry_cache.h"

#include <systemc.h>

SC_MODULE(ShaderFrontendBase) {
    sc_in_clk inpClock;
    struct {
        sc_out<bool> outEnable;
        sc_out<MemoryAddressType> outAddress;
        sc_in<MemoryDataType> inpData;
        sc_in<bool> inpCompleted;
    } memory;
    struct {
        sc_signal<bool> requestThreadBusy;
        sc_signal<bool> responseThreadBusy;
        sc_out<bool> outBusy;
        sc_out<sc_uint<32>> outIsaFetches;
    } profiling;

    SC_CTOR(ShaderFrontendBase) {
        SC_CTHREAD(requestThread, inpClock.pos());
        SC_CTHREAD(responseThread, inpClock.pos());
        SC_METHOD(busySignalMethod);
        sensitive << profiling.responseThreadBusy << profiling.requestThreadBusy;
    }

protected:
    // Structures to hold values dependent on clientsCount and shaderUnitsCount, which will be instantiated in templated subclass
    struct ClientInterface {
        struct {
            sc_in<bool> inpSending;
            sc_in<sc_uint<32>> inpData;
            sc_out<bool> outReceiving;
        } request;

        struct {
            sc_out<bool> outSending;
            sc_out<sc_uint<32>> outData;
            sc_in<bool> inpReceiving;
        } response;
    };
    struct ShaderUnitInterface {
        struct {
            sc_out<bool> outSending;
            sc_out<sc_uint<32>> outData;
            sc_in<bool> inpReceiving;
        } request;

        struct {
            sc_out<bool> outReceiving;
            sc_in<bool> inpSending;
            sc_in<sc_uint<32>> inpData;
        } response;
    };
    struct ShaderUnitState {
        MemoryAddressType loadedIsaAddress = 0xffffffff;
        struct {
            bool isActive = false;
            int clientIndex{};
            int clientToken{};
            int outputsCount{};
        } request;
    };

    // Methods for traversing clients and shader units. Moved to a templated subclass, since this class doesn't know about them.
    virtual bool findFreeShaderUnit(ShaderUnitInterface * *outUnitInterface, ShaderUnitState * *outState) = 0;
    virtual bool findClientMakingRequest(ClientInterface * *outClientInterface, size_t * outIndex) = 0;
    virtual bool findShaderUnitSendingResponse(ShaderUnitInterface * *outUnitInterface, ShaderUnitState * *outState, ClientInterface * *outClientInterface) = 0;

private:
    // Methods implementing SystemC processes
    void requestThread();
    void responseThread();
    void busySignalMethod();

    // ShaderFrontend must read ISA from memory and store it into its ShaderUnits before a shader is executed. The
    // code is cached, so we don't have to read it all the way from memory each time.
    struct IsaCacheEntry {
        IsaCacheEntry() = default;
        IsaCacheEntry &operator=(IsaCacheEntry &&other) {
            std::copy_n(other.data, other.dataSize, this->data);
            this->dataSize = other.dataSize;
            return *this;
        }
        Isa::Command::CommandStoreIsa &getMetadata() {
            return reinterpret_cast<Isa::Command::CommandStoreIsa &>(data[0]);
        }
        uint32_t data[Isa::commandSizeInDwords + Isa::maxIsaSize];
        uint32_t dataSize = 0;
    };
    constexpr static inline size_t isaCacheSize = 2;
    constexpr static inline size_t invalidAddress = 0xffffffff;
    EntryCache<uint32_t, IsaCacheEntry, isaCacheSize, invalidAddress> isaCache;

    // Methods for manipulating and executing ISA
    IsaCacheEntry &getIsa(uint32_t isaAddress);
    void storeIsa(ShaderUnitInterface & shaderUnitInterface, IsaCacheEntry & indexInIsaCache, bool hasNextCommand);
    void executeIsa(ShaderUnitInterface & shaderUnitInterface, bool handshakeAlreadyDone, const uint32_t *shaderInputs, NonZeroCount threadCount, uint32_t shaderInputsCount);

    // Methods for utility purposes
    size_t calculateShaderInputsCount(const ShaderFrontendRequest &request);
    size_t calculateShaderOutputsCount(const ShaderFrontendRequest &request);
    void validateRequest(const ShaderFrontendRequest &request, Isa::Command::CommandStoreIsa &isaCommand);
};

template <size_t clientsCount, size_t shaderUnitsCount>
struct ShaderFrontend : ShaderFrontendBase {
    using ShaderFrontendBase::ShaderFrontendBase;
    ClientInterface clientInterfaces[clientsCount];
    ShaderUnitInterface shaderUnitInterfaces[shaderUnitsCount];

protected:
    bool findFreeShaderUnit(ShaderUnitInterface **outUnitInterface, ShaderUnitState **outState) override {
        for (size_t shaderUnitIndex = 0; shaderUnitIndex < shaderUnitsCount; shaderUnitIndex++) {
            if (!shaderUnitStates[shaderUnitIndex].request.isActive) {
                *outUnitInterface = &shaderUnitInterfaces[shaderUnitIndex];
                *outState = &shaderUnitStates[shaderUnitIndex];
                return true;
            }
        }
        return false;
    }

    bool findClientMakingRequest(ClientInterface **outClientInterface, size_t *outIndex) override {
        for (size_t clientIndex = 0; clientIndex < clientsCount; clientIndex++) {
            if (clientInterfaces[clientIndex].request.inpSending.read()) {
                *outClientInterface = &clientInterfaces[clientIndex];
                *outIndex = clientIndex;
                return true;
            }
        }
        return false;
    }

    bool findShaderUnitSendingResponse(ShaderUnitInterface **outUnitInterface, ShaderUnitState **outState, ClientInterface **outClientInterface) override {
        for (size_t shaderUnitIndex = 0; shaderUnitIndex < shaderUnitsCount; shaderUnitIndex++) {
            if (shaderUnitInterfaces[shaderUnitIndex].response.inpSending.read()) {
                *outUnitInterface = &shaderUnitInterfaces[shaderUnitIndex];
                *outState = &shaderUnitStates[shaderUnitIndex];
                *outClientInterface = &clientInterfaces[shaderUnitStates[shaderUnitIndex].request.clientIndex];
                return true;
            }
        }
        return false;
    }

private:
    // This array holds states that we programmed for shader unit. It is cached, so we don't have to
    // ask the shader units about their states and transfer too much data.
    ShaderUnitState shaderUnitStates[shaderUnitsCount];
};
