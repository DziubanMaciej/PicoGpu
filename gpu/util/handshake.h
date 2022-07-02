#pragma once

#include "gpu/util/error.h"

#include <systemc.h>

struct Handshake {
    // TODO add functions to send/receive arrays of data of size known based on the first element

    template <typename DataT, typename DataToSendT>
    static void sendArrayWithParallelPorts(sc_in<bool> &inpReceiving, sc_out<bool> &outSending,
                                           sc_out<DataT> *outData, size_t outDataCount,
                                           DataToSendT *dataToSend, size_t dataToSendCount) {
        FATAL_ERROR_IF(dataToSendCount == 0, "Cannot send empty array");
        FATAL_ERROR_IF(outDataCount == 0, "Cannot send through zero ports");

        const size_t packagesCount = (dataToSendCount + outDataCount - 1) / outDataCount;

        size_t dataSent = 0;

        // Tell the receiver that we're able to begin transmission and send first package of data
        outSending = 1;
        for (; dataSent < dataToSendCount && dataSent < outDataCount; dataSent++) {
            outData[dataSent] = dataToSend[dataSent];
        }

        // Wait for the receiver to acknowledge the transmission
        do {
            wait();
        } while (!inpReceiving.read());
        outSending = 0;

        // Send remaining packages
        for (size_t packageIndex = 1; packageIndex < packagesCount; packageIndex++) {
            for (size_t port = 0; port < outDataCount && dataSent < dataToSendCount; port++, dataSent++) {
                outData[port] = dataToSend[dataSent];
            }
            wait();
        }
    }

    template <typename DataT, typename DataToSendT>
    static void receiveArrayWithParallelPorts(sc_in<bool> &inpSending, sc_out<bool> &outReceiving,
                                              sc_in<DataT> *inpData, size_t inpDataCount,
                                              DataToSendT *dataToReceive, size_t dataToReceiveCount) {
        FATAL_ERROR_IF(dataToReceiveCount == 0, "Cannot receive empty array");
        FATAL_ERROR_IF(inpDataCount == 0, "Cannot receive through zero ports");

        // Tell the sender that we're able to begin transmission
        outReceiving = 1;

        // Wait for the sender to acknowledge the transmission
        do {
            wait();
        } while (!inpSending.read());
        outReceiving = 0;

        // Receive packages of data
        const size_t packagesCount = (dataToReceiveCount + inpDataCount - 1) / inpDataCount;
        size_t dataReceived = 0;
        for (size_t packageIndex = 0; packageIndex < packagesCount; packageIndex++) {
            if (packageIndex > 0) {
                wait();
            }
            for (size_t port = 0; port < inpDataCount && dataReceived < dataToReceiveCount; port++, dataReceived++) {
                dataToReceive[dataReceived] = inpData[port].read();
            }
        }
    }

    template <typename DataT, typename DataToSendT>
    static inline void sendArray(sc_in<bool> &inpReceiving, sc_out<bool> &outSending, sc_out<DataT> &outData, DataToSendT *dataToSend, size_t dataToSendCount) {
        sendArrayWithParallelPorts(inpReceiving, outSending, &outData, 1, dataToSend, dataToSendCount);
    }

    template <typename DataT, typename DataToReceiveT>
    static inline void receiveArray(sc_in<bool> &inpSending, sc_in<DataT> &inpData, sc_out<bool> &outReceiving, DataToReceiveT *dataToReceive, size_t dataToReceiveCount) {
        receiveArrayWithParallelPorts(inpSending, outReceiving, &inpData, 1, dataToReceive, dataToReceiveCount);
    }

    template <typename DataT, typename DataToSendT>
    static inline void send(sc_in<bool> &inpReceiving, sc_out<bool> &outSending, sc_out<DataT> &outData, DataToSendT &dataToSend) {
        sendArrayWithParallelPorts(inpReceiving, outSending, &outData, 1, &dataToSend, 1);
    }

    template <typename DataT>
    static inline DataT receive(sc_in<bool> &inpSending, sc_in<DataT> &inpData, sc_out<bool> &outReceiving) {
        DataT dataToReceive = {};
        receiveArrayWithParallelPorts(inpSending, outReceiving, &inpData, 1, &dataToReceive, 1);
        return dataToReceive;
    }
};
