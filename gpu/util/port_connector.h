#pragma once

#include "gpu/fragment.h"
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

        Chunk(Chunk &&other) : signalsMemory(std::move(other.signalsMemory)),
                               signals(other.signals),
                               usedSignalsCount(other.usedSignalsCount) {
            other.usedSignalsCount = 0;
        }

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

    template <typename DataType, size_t inputsCount>
    void connectPortsA(sc_in<DataType> *(&inp)[inputsCount], sc_out<DataType> &out, const std::string &name) {
        auto &signal = signals<DataType>().get(name);
        for (size_t i = 0; i < inputsCount; i++) {
            (*inp[i])(signal);
        }
        out(signal);
    }

    template <typename SenderType, typename ReceiverType>
    void connectHandshake(SenderType &sender, ReceiverType &receiver, const std::string &signalNamePrefix) {
        using DataType = typename decltype(SenderType::outData)::data_type;

        auto &sending = boolSignals.get(signalNamePrefix + "_sending");
        sender.outSending(sending);
        receiver.inpSending(sending);

        auto &receiving = boolSignals.get(signalNamePrefix + "_receiving");
        sender.inpReceiving(receiving);
        receiver.outReceiving(receiving);

        auto &data = signals<DataType>().get(signalNamePrefix + "_data");
        sender.outData(data);
        receiver.inpData(data);
    }

    template <typename SenderType, typename ReceiverType>
    void connectHandshakeWithParallelPorts(SenderType &sender, ReceiverType &receiver, const std::string &signalNamePrefix) {
        using PortArrayType = decltype(SenderType::outData);
        using PortType = std::remove_extent_t<PortArrayType>;
        using DataType = typename PortType::data_type;

        static_assert(SenderType::portsCount == ReceiverType::portsCount);

        auto &sending = boolSignals.get(signalNamePrefix + "_sending");
        sender.outSending(sending);
        receiver.inpSending(sending);

        auto &receiving = boolSignals.get(signalNamePrefix + "_receiving");
        sender.inpReceiving(receiving);
        receiver.outReceiving(receiving);

        for (size_t i = 0; i < SenderType::portsCount; i++) {
            auto &data = signals<DataType>().get(signalNamePrefix + "_data" + std::to_string(i));
            sender.outData[i](data);
            receiver.inpData[i](data);
        }
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
        shadedFragmentSignals.addSignalsToTrace(trace);
    }

private:
    template <typename DataType>
    SignalVector<DataType> &signals() {
        if constexpr (std::is_same_v<DataType, sc_uint<32>>) {
            return dwordSignals;
        } else if constexpr (std::is_same_v<DataType, bool>) {
            return boolSignals;
        } else if constexpr (std::is_same_v<DataType, ShadedFragment>) {
            return shadedFragmentSignals;
        } else {
            staticNoMatch();
        }
    }

    // This function is needed for implementing something like "else constexpr { static_assert(...) }",
    // which cannot be achieved in regular C++. It uses the fact, that this function template will
    // not be instantiated if any of constexpr ifs in signals() method is evaluated to true.
    template <bool flag = false>
    static void staticNoMatch() { static_assert(flag, "Invalid datatype"); }

    SignalVector<sc_uint<32>> dwordSignals;
    SignalVector<bool> boolSignals;
    SignalVector<ShadedFragment> shadedFragmentSignals;
};
