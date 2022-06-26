#pragma once

#include "gpu/util/error.h"
#include "gpu/util/vcd_trace.h"

#include <systemc.h>
#include <vector>

template <typename DataType, size_t chunkSize = 32>
struct SignalVector {
    using SignalType = sc_signal<DataType>;

    SignalType &get(const std::string &name) {
        if (chunks.size() == 0 || chunks.back().isFull()) {
            chunks.emplace_back();
        }
        return chunks.back().get(name);
    }

    void addSignalsToTrace(VcdTrace &trace) {
        for (auto &chunk : chunks) {
            for (int i = 0; i < chunk.usedSignalsCount; i++) {
                trace.trace(chunk.signals[i]);
            }
        }
    }

private:
    struct Chunk {
        Chunk() {
            signalsMemory = std::make_unique<char[]>(chunkSize * sizeof(SignalType));
            signals = reinterpret_cast<SignalType *>(signalsMemory.get());
        }

        Chunk(Chunk &&other) = default;

        ~Chunk() {
            for (int i = 0; i < usedSignalsCount; i++) {
                signals[i].~SignalType();
            }
        }

        SignalType &get(const std::string &name) {
            FATAL_ERROR_IF(isFull(), "SignalVector chunk is full");
            SignalType *signal = &signals[usedSignalsCount++];
            signal = new (signal) SignalType(name.c_str());
            return *signal;
        }

        bool isFull() {
            return chunkSize == usedSignalsCount;
        }

        std::unique_ptr<char[]> signalsMemory;
        SignalType *signals;
        size_t usedSignalsCount = 0;
    };
    std::vector<Chunk> chunks;
};

enum class MemoryClientType {
    ReadWrite,
    ReadOnly,
    WriteOnly,
};
enum class MemoryServerType {
    Normal,
    SeparateOutData,
};

struct PortConnector {
    template <typename DataType>
    void connectPorts(sc_in<DataType> &inp, sc_out<DataType> &out, const std::string &name) {
        auto &signal = signals<DataType>().get(name);
        inp(signal);
        out(signal);
    }

    template <typename DataType>
    void connectPorts(sc_in<DataType> &inp0, sc_in<DataType> &inp1, sc_out<DataType> &out, const std::string &name) {
        auto &signal = signals<DataType>().get(name);
        inp0(signal);
        inp1(signal);
        out(signal);
    }

    template <MemoryClientType clientType = MemoryClientType::ReadWrite, MemoryServerType serverType = MemoryServerType::Normal, typename MemoryClient, typename MemoryServer>
    void connectMemoryToClient(MemoryClient &client, MemoryServer &memory, const std::string &signalNamePrefix) {
        auto &enable = boolSignals.get(signalNamePrefix + "_enable");
        memory.inpEnable(enable);
        client.outEnable(enable);

        auto &write = boolSignals.get(signalNamePrefix + "_write");
        memory.inpWrite(write);
        if constexpr (clientType == MemoryClientType::ReadWrite) {
            client.outWrite(write);
        } else {
            write = (clientType == MemoryClientType::ReadOnly) ? 0 : 1;
        }

        auto &address = dwordSignals.get(signalNamePrefix + "_address");
        memory.inpAddress(address);
        client.outAddress(address);

        auto &dataForWrite = dwordSignals.get(signalNamePrefix + "_dataForWrite");
        memory.inpData(dataForWrite);
        if constexpr (clientType != MemoryClientType::ReadOnly) {
            client.outData(dataForWrite);
        }

        if constexpr (serverType == MemoryServerType::Normal) {
            auto &dataForRead = dwordSignals.get(signalNamePrefix + "_dataForRead");
            memory.outData(dataForRead);
            if constexpr (clientType != MemoryClientType::WriteOnly) {
                client.inpData(dataForRead);
            }
        }

        auto &completed = boolSignals.get(signalNamePrefix + "_completed");
        memory.outCompleted(completed);
        client.inpCompleted(completed);
    }

    void addSignalsToTrace(VcdTrace &trace) {
        dwordSignals.addSignalsToTrace(trace);
        boolSignals.addSignalsToTrace(trace);
    }

private:
    template <typename DataType>
    SignalVector<DataType> &signals() {
        if constexpr (std::is_same_v<DataType, sc_uint<32>>) {
            return dwordSignals;
        }
        if constexpr (std::is_same_v<DataType, bool>) {
            return boolSignals;
        }
    }

    SignalVector<sc_uint<32>> dwordSignals;
    SignalVector<bool> boolSignals;
};
