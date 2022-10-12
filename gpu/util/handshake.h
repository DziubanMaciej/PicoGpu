#pragma once

#include "gpu/util/error.h"

#include <systemc.h>

struct Handshake {
private:
    template <typename DataT, typename DataToSendT, size_t numberOfPorts>
    struct SendArgs {
        sc_in<bool> *inpReceiving; // control signal, which the receiver uses to tell use that it's ready
        sc_out<bool> *outSending;  // control signal, which we use to tell the receiver that we're ready
        sc_out<DataT> *outData;    // array of parallel ports used to send data
        DataToSendT *dataToSend;   // array of data elements to send
        size_t dataToSendCount;    // number of data elements to send
    };

    template <typename DataT, typename DataToSendT, size_t numberOfPorts>
    static void sendArrayImpl(SendArgs<DataT, DataToSendT, numberOfPorts> &args) {
        FATAL_ERROR_IF(args.dataToSendCount == 0, "Cannot send empty array");
        FATAL_ERROR_IF(numberOfPorts == 0, "Cannot send through zero ports");

        const size_t packagesCount = (args.dataToSendCount + numberOfPorts - 1) / numberOfPorts;

        size_t dataSent = 0;

        // Tell the receiver that we're able to begin transmission and send first package of data
        args.outSending->write(1);
        for (; dataSent < args.dataToSendCount && dataSent < numberOfPorts; dataSent++) {
            args.outData[dataSent] = args.dataToSend[dataSent];
        }

        // Wait for the receiver to acknowledge the transmission
        do {
            wait();
        } while (!args.inpReceiving->read());
        args.outSending->write(0);

        // Send remaining packages
        for (size_t packageIndex = 1; packageIndex < packagesCount; packageIndex++) {
            for (size_t port = 0; port < numberOfPorts && dataSent < args.dataToSendCount; port++, dataSent++) {
                args.outData[port] = args.dataToSend[dataSent];
            }
            wait();
        }

        // Clear the data ports. This is optional, but makes it easier to debug.
        for (size_t port = 0; port < numberOfPorts; port++) {
            args.outData[port].write({});
        }
    }

    template <typename DataT, typename DataToSendT, size_t numberOfPorts>
    struct ReceiveArgs {
        sc_in<bool> *inpSending;                   // control signal, which the sender uses to tell use that it's ready
        sc_out<bool> *outReceiving;                // control signal, which we use to tell the sender that we're ready
        sc_in<DataT> *inpData;                     // array of parallel ports used to receive data
        DataToSendT *dataToReceive;                // array of data elements to fill upon receiving
        size_t dataToReceiveCount;                 // number of data elements to receive
        sc_out<bool> *outBusinessSignal = nullptr; // optional control signal, that will be deactivated when receiving is stalled
        size_t *timeout = nullptr;                 // optional number of clock ticks after which the operation will be cancelled
        bool *success = nullptr;                   // optional success code
    };
    template <typename DataT, typename DataToSendT, size_t numberOfPorts>
    static void receiveArrayImpl(ReceiveArgs<DataT, DataToSendT, numberOfPorts> &args) {
        FATAL_ERROR_IF(args.dataToReceiveCount == 0, "Cannot receive empty array");
        FATAL_ERROR_IF(numberOfPorts == 0, "Cannot receive through zero ports");

        // Tell the sender that we're able to begin transmission
        args.outReceiving->write(1);

        // Prepare some variables for timeout behavior
        bool cancelled = false;
        size_t clocksWaiting = 1;

        // Wait for the sender to acknowledge the transmission
        wait();
        while (!args.inpSending->read()) {
            if (args.outBusinessSignal) {
                *args.outBusinessSignal = false;
            }
            wait();

            if (args.timeout && *args.timeout >= (++clocksWaiting)) {
                cancelled = true;
                break;
            }
        }
        if (args.outBusinessSignal) {
            *args.outBusinessSignal = true;
        }
        args.outReceiving->write(0);

        // Handle timeout behavior
        if (args.success) {
            *args.success = !cancelled;
        }
        if (cancelled) {
            return;
        }

        // Receive packages of data
        const size_t packagesCount = (args.dataToReceiveCount + numberOfPorts - 1) / numberOfPorts;
        size_t dataReceived = 0;
        for (size_t packageIndex = 0; packageIndex < packagesCount; packageIndex++) {
            if (packageIndex > 0) {
                wait();
            }
            for (size_t port = 0; port < numberOfPorts && dataReceived < args.dataToReceiveCount; port++, dataReceived++) {
                args.dataToReceive[dataReceived] = args.inpData[port].read();
            }
        }
    }

public:
    template <typename DataT, typename DataToSendT, size_t numberOfPorts>
    static void sendArrayWithParallelPorts(sc_in<bool> &inpReceiving, sc_out<bool> &outSending, sc_out<DataT> (&outData)[numberOfPorts],
                                           DataToSendT *dataToSend, size_t dataToSendCount) {
        SendArgs<DataT, DataToSendT, numberOfPorts> args = {};
        args.inpReceiving = &inpReceiving;
        args.outSending = &outSending;
        args.outData = outData;
        args.dataToSend = dataToSend;
        args.dataToSendCount = dataToSendCount;
        sendArrayImpl(args);
    }

    template <typename DataT, typename DataToReceiveT, size_t numberOfPorts>
    static void receiveArrayWithParallelPorts(sc_in<bool> &inpSending, sc_out<bool> &outReceiving, sc_in<DataT> (&inpData)[numberOfPorts],
                                              DataToReceiveT *dataToReceive, size_t dataToReceiveCount, sc_out<bool> *outBusinessSignal = nullptr) {
        ReceiveArgs<DataT, DataToReceiveT, numberOfPorts> args = {};
        args.inpSending = &inpSending;
        args.outReceiving = &outReceiving;
        args.inpData = inpData;
        args.dataToReceive = dataToReceive;
        args.dataToReceiveCount = dataToReceiveCount;
        args.outBusinessSignal = outBusinessSignal;
        receiveArrayImpl(args);
    }

    template <typename DataT, typename DataToSendT>
    static inline void sendArray(sc_in<bool> &inpReceiving, sc_out<bool> &outSending, sc_out<DataT> &outData, DataToSendT *dataToSend, size_t dataToSendCount) {
        SendArgs<DataT, DataToSendT, 1> args = {};
        args.inpReceiving = &inpReceiving;
        args.outSending = &outSending;
        args.outData = &outData;
        args.dataToSend = dataToSend;
        args.dataToSendCount = dataToSendCount;
        sendArrayImpl(args);
    }

    template <typename DataT, typename DataToReceiveT>
    static inline void receiveArray(sc_in<bool> &inpSending, sc_in<DataT> &inpData, sc_out<bool> &outReceiving,
                                    DataToReceiveT *dataToReceive, size_t dataToReceiveCount,
                                    sc_out<bool> *outBusinessSignal = nullptr) {
        ReceiveArgs<DataT, DataToReceiveT, 1> args = {};
        args.inpSending = &inpSending;
        args.outReceiving = &outReceiving;
        args.inpData = &inpData;
        args.dataToReceive = dataToReceive;
        args.dataToReceiveCount = dataToReceiveCount;
        args.outBusinessSignal = outBusinessSignal;
        receiveArrayImpl(args);
    }

    template <typename DataT, typename DataToSendT>
    static inline void send(sc_in<bool> &inpReceiving, sc_out<bool> &outSending, sc_out<DataT> &outData, DataToSendT &dataToSend) {
        SendArgs<DataT, DataToSendT, 1> args = {};
        args.inpReceiving = &inpReceiving;
        args.outSending = &outSending;
        args.outData = &outData;
        args.dataToSend = &dataToSend;
        args.dataToSendCount = 1;
        sendArrayImpl(args);
    }

    template <typename DataT, typename DataToSendT = DataT>
    static inline DataT receive(sc_in<bool> &inpSending, sc_in<DataT> &inpData, sc_out<bool> &outReceiving, sc_out<bool> *outBusinessSignal = nullptr) {
        DataT dataToReceive = {};
        ReceiveArgs<DataT, DataToSendT, 1> args = {};
        args.inpSending = &inpSending;
        args.outReceiving = &outReceiving;
        args.inpData = &inpData;
        args.dataToReceive = &dataToReceive;
        args.dataToReceiveCount = 1;
        args.outBusinessSignal = outBusinessSignal;
        receiveArrayImpl(args);
        return dataToReceive;
    }

    template <typename DataT, typename DataToSendT = DataT>
    static inline DataT receiveWithTimeout(sc_in<bool> &inpSending, sc_in<DataT> &inpData, sc_out<bool> &outReceiving, size_t timeout, bool &success) {
        DataT dataToReceive = {};
        ReceiveArgs<DataT, DataToSendT, 1> args = {};
        args.inpSending = &inpSending;
        args.outReceiving = &outReceiving;
        args.inpData = &inpData;
        args.dataToReceive = &dataToReceive;
        args.dataToReceiveCount = 1;
        args.timeout = &timeout;
        args.success = &success;
        receiveArrayImpl(args);
        return dataToReceive;
    }
};
