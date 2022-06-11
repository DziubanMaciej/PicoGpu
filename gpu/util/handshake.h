#include <systemc.h>

struct Handshake {
    template <typename DataT>
    static void send(sc_in<bool> &inpReceiving, sc_out<bool> &outSending, sc_out<DataT> &outData, DataT &dataToSend) {
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
