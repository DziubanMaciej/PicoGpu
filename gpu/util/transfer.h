#pragma once

#include "gpu/util/error.h"

#include <systemc.h>

struct Transfer {
private:
    template <typename DataT, typename DataToSendT>
    struct SendArgs {
        sc_in<bool> *inpReceiving;   // control signal, which the receiver uses to tell use that it's ready
        sc_out<bool> *outSending;    // control signal, which we use to tell the receiver that we're ready
        sc_out<DataT> *outDataPorts; // array of parallel ports used to send data
        DataToSendT *dataToSend;     // array of data elements to send
        size_t dataPortsCount;       // number of parallel data ports
        size_t dataToSendCount;      // number of data elements to send
        bool performHandshake;       // whether to use control signals to synchronize with the receiver or not
    };

    template <typename DataT, typename DataToSendT>
    static void sendArrayImpl(SendArgs<DataT, DataToSendT> &args) {
        FATAL_ERROR_IF(args.dataToSendCount == 0, "Cannot send empty array");
        FATAL_ERROR_IF(args.dataPortsCount == 0, "Cannot send through zero ports");

        const size_t packagesCount = (args.dataToSendCount + args.dataPortsCount - 1) / args.dataPortsCount;
        size_t dataSent = 0;
        size_t packagesSent = 0;

        // Handshake with the receiver
        if (args.performHandshake) {
            // Tell the receiver that we're able to begin transmission and send first package of data
            args.outSending->write(1);
            for (; dataSent < args.dataToSendCount && dataSent < args.dataPortsCount; dataSent++) {
                args.outDataPorts[dataSent] = args.dataToSend[dataSent];
            }
            packagesSent++;

            // Wait for the receiver to acknowledge the transmission
            do {
                wait();
            } while (!args.inpReceiving->read());
            args.outSending->write(0);
        }

        // Send remaining packages
        for (; packagesSent < packagesCount; packagesSent++) {
            for (size_t port = 0; port < args.dataPortsCount && dataSent < args.dataToSendCount; port++, dataSent++) {
                args.outDataPorts[port] = args.dataToSend[dataSent];
            }
            wait();
        }

        // Clear the data ports. This is optional, but makes it easier to debug.
        for (size_t port = 0; port < args.dataPortsCount; port++) {
            args.outDataPorts[port].write({});
        }
    }

    template <typename DataT, typename DataToSendT>
    struct ReceiveArgs {
        sc_in<bool> *inpSending;                   // control signal, which the sender uses to tell use that it's ready
        sc_out<bool> *outReceiving;                // control signal, which we use to tell the sender that we're ready
        sc_in<DataT> *inpDataPorts;                // array of parallel ports used to receive data
        DataToSendT *dataToReceive;                // array of data elements to fill upon receiving
        size_t dataPortsCount;                     // number of parallel data ports
        size_t dataToReceiveCount;                 // number of data elements to receive
        sc_out<bool> *outBusinessSignal = nullptr; // optional control signal, that will be deactivated when receiving is stalled
        size_t *timeout = nullptr;                 // optional number of clock ticks after which the operation will be cancelled
        bool *success = nullptr;                   // optional success code
        bool performHandshake;                     // whether to use control signals to synchronize with the sender or not
    };
    template <typename DataT, typename DataToSendT>
    static void receiveArrayImpl(ReceiveArgs<DataT, DataToSendT> &args) {
        FATAL_ERROR_IF(args.dataToReceiveCount == 0, "Cannot receive empty array");
        FATAL_ERROR_IF(args.dataPortsCount == 0, "Cannot receive through zero ports");

        const size_t packagesCount = (args.dataToReceiveCount + args.dataPortsCount - 1) / args.dataPortsCount;
        size_t dataReceived = 0;
        size_t packagesReceived = 0;

        // By default consider operation as a success
        if (args.success) {
            *args.success = true;
        }

        // Handshake with the sender
        if (args.performHandshake) {
            // Prepare some variables for timeout behavior
            bool cancelled = false;
            size_t clocksWaiting = 1;

            // Tell the sender that we're able to begin transmission
            args.outReceiving->write(1);

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
            if (cancelled) {
                if (args.success) {
                    *args.success = false;
                }
                return;
            }

            // Receive first package of data
            for (; dataReceived < args.dataPortsCount && dataReceived < args.dataToReceiveCount; dataReceived++) {
                args.dataToReceive[dataReceived] = args.inpDataPorts[dataReceived].read();
            }
            packagesReceived++;
        }

        // Receive remaining packages of data
        for (; packagesReceived < packagesCount; packagesReceived++) {
            wait();
            for (size_t port = 0; port < args.dataPortsCount && dataReceived < args.dataToReceiveCount; port++, dataReceived++) {
                args.dataToReceive[dataReceived] = args.inpDataPorts[port].read();
            }
        }
    }

public:
    template <typename DataT, typename DataToSendT, size_t numberOfPorts>
    static void sendArrayWithParallelPorts(sc_in<bool> &inpReceiving, sc_out<bool> &outSending, sc_out<DataT> (&outData)[numberOfPorts],
                                           DataToSendT *dataToSend, size_t dataToSendCount) {
        SendArgs<DataT, DataToSendT> args = {};
        args.inpReceiving = &inpReceiving;
        args.outSending = &outSending;
        args.outDataPorts = outData;
        args.dataToSend = dataToSend;
        args.dataPortsCount = numberOfPorts;
        args.dataToSendCount = dataToSendCount;
        args.performHandshake = true;
        sendArrayImpl(args);
    }

    template <typename DataT, typename DataToReceiveT, size_t numberOfPorts>
    static void receiveArrayWithParallelPorts(sc_in<bool> &inpSending, sc_out<bool> &outReceiving, sc_in<DataT> (&inpData)[numberOfPorts],
                                              DataToReceiveT *dataToReceive, size_t dataToReceiveCount, sc_out<bool> *outBusinessSignal = nullptr) {
        ReceiveArgs<DataT, DataToReceiveT> args = {};
        args.inpSending = &inpSending;
        args.outReceiving = &outReceiving;
        args.inpDataPorts = inpData;
        args.dataToReceive = dataToReceive;
        args.dataPortsCount = numberOfPorts;
        args.dataToReceiveCount = dataToReceiveCount;
        args.outBusinessSignal = outBusinessSignal;
        args.performHandshake = true;
        receiveArrayImpl(args);
    }

    template <typename DataT, typename DataToSendT>
    static inline void sendArray(sc_in<bool> &inpReceiving, sc_out<bool> &outSending, sc_out<DataT> &outData, DataToSendT *dataToSend, size_t dataToSendCount, bool performHandshake = true) {
        SendArgs<DataT, DataToSendT> args = {};
        args.inpReceiving = &inpReceiving;
        args.outSending = &outSending;
        args.outDataPorts = &outData;
        args.dataToSend = dataToSend;
        args.dataPortsCount = 1;
        args.dataToSendCount = dataToSendCount;
        args.performHandshake = performHandshake;
        sendArrayImpl(args);
    }

    template <typename DataT, typename DataToReceiveT>
    static inline void receiveArray(sc_in<bool> &inpSending, sc_in<DataT> &inpData, sc_out<bool> &outReceiving,
                                    DataToReceiveT *dataToReceive, size_t dataToReceiveCount,
                                    sc_out<bool> *outBusinessSignal = nullptr, bool performHandshake = true) {
        ReceiveArgs<DataT, DataToReceiveT> args = {};
        args.inpSending = &inpSending;
        args.outReceiving = &outReceiving;
        args.inpDataPorts = &inpData;
        args.dataToReceive = dataToReceive;
        args.dataPortsCount = 1;
        args.dataToReceiveCount = dataToReceiveCount;
        args.outBusinessSignal = outBusinessSignal;
        args.performHandshake = performHandshake;
        receiveArrayImpl(args);
    }

    template <typename DataT, typename DataToSendT>
    static inline void send(sc_in<bool> &inpReceiving, sc_out<bool> &outSending, sc_out<DataT> &outData, DataToSendT &dataToSend) {
        SendArgs<DataT, DataToSendT> args = {};
        args.inpReceiving = &inpReceiving;
        args.outSending = &outSending;
        args.outDataPorts = &outData;
        args.dataToSend = &dataToSend;
        args.dataPortsCount = 1;
        args.dataToSendCount = 1;
        args.performHandshake = true;
        sendArrayImpl(args);
    }

    template <typename DataT, typename DataToSendT = DataT>
    static inline DataT receive(sc_in<bool> &inpSending, sc_in<DataT> &inpData, sc_out<bool> &outReceiving, sc_out<bool> *outBusinessSignal = nullptr) {
        DataT dataToReceive = {};
        ReceiveArgs<DataT, DataToSendT> args = {};
        args.inpSending = &inpSending;
        args.outReceiving = &outReceiving;
        args.inpDataPorts = &inpData;
        args.dataToReceive = &dataToReceive;
        args.dataPortsCount = 1;
        args.dataToReceiveCount = 1;
        args.outBusinessSignal = outBusinessSignal;
        args.performHandshake = true;
        receiveArrayImpl(args);
        return dataToReceive;
    }

    template <typename DataT, typename DataToSendT = DataT>
    static inline DataT receiveWithTimeout(sc_in<bool> &inpSending, sc_in<DataT> &inpData, sc_out<bool> &outReceiving, size_t timeout, bool &success) {
        DataT dataToReceive = {};
        ReceiveArgs<DataT, DataToSendT> args = {};
        args.inpSending = &inpSending;
        args.outReceiving = &outReceiving;
        args.inpDataPorts = &inpData;
        args.dataToReceive = &dataToReceive;
        args.dataPortsCount = 1;
        args.dataToReceiveCount = 1;
        args.timeout = &timeout;
        args.success = &success;
        args.performHandshake = true;
        receiveArrayImpl(args);
        return dataToReceive;
    }
};
