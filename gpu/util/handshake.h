#pragma once

#include "gpu/util/error.h"

#include <systemc.h>

struct Handshake {
    // TODO add functions to send/receive arrays of data of size known based on the first element

    template <typename DataT, typename DataToSendT>
    static void sendArray(sc_in<bool> &inpReceiving, sc_out<bool> &outSending, sc_out<DataT> &outData, DataToSendT *dataToSend, size_t dataToSendCount) {
        FATAL_ERROR_IF(dataToSendCount == 0, "Cannot send empty array");

        send(inpReceiving, outSending, outData, dataToSend[0]);
        for (int i = 1; i < dataToSendCount; i++) {
            outData = dataToSend[i];
            wait();
        }
        outData.write({});
    }

    template <typename DataT, typename DataToReceiveT>
    static void receiveArray(sc_in<bool> &inpSending, sc_in<DataT> &inpData, sc_out<bool> &outReceiving, DataToReceiveT *dataToReceive, size_t dataToReceiveCount) {
        FATAL_ERROR_IF(dataToReceiveCount == 0, "Cannot receive empty array");

        dataToReceive[0] = Handshake::receive(inpSending, inpData, outReceiving);
        for (int i = 1; i < dataToReceiveCount; i++) {
            wait();
            dataToReceive[i] = inpData.read();
        }
    }

    template <typename DataT, typename DataToSendT>
    static void send(sc_in<bool> &inpReceiving, sc_out<bool> &outSending, sc_out<DataT> &outData, DataToSendT &dataToSend) {
        outSending = 1;
        outData = dataToSend;
        do {
            wait();
        } while (!inpReceiving.read());
        outSending = 0;
        outData.write({});
    }

    template <typename DataT>
    static DataT receive(sc_in<bool> &inpSending, sc_in<DataT> &inpData, sc_out<bool> &outReceiving) {
        outReceiving = 1;
        do {
            wait();
        } while (!inpSending.read());
        outReceiving = 0;
        return inpData.read();
    }
};
