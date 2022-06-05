#pragma once

#include "systemc.h"

#include "gpu/types.h"
#include "gpu/util/log.h"

template <unsigned int clientsCount>
SC_MODULE(MemoryController) {
    static_assert(clientsCount > 0);

    sc_in_clk inpClock;

    // Interface with clients
    struct ClientPorts {
        sc_in<bool> inpEnable;
        sc_in<bool> inpWrite;
        sc_in<MemoryAddressType> inpAddress;
        sc_in<MemoryDataType> inpData;
        sc_out<bool> outCompleted;
    } clients[clientsCount];
    sc_out<MemoryDataType> outData;

    // Interface with memory
    struct MemoryPorts {
        sc_out<bool> outEnable;
        sc_out<bool> outWrite;
        sc_out<MemoryAddressType> outAddress;
        sc_out<MemoryDataType> outData;
        sc_in<MemoryDataType> inpData;
        sc_in<bool> inpCompleted;
    } memory;

    // Latched values from clients
    struct ClientLatchedSignals {
        sc_signal<bool, SC_MANY_WRITERS> enable;
        sc_signal<bool> write;
        sc_signal<MemoryAddressType> address;
        sc_signal<MemoryDataType> data;
    } clientsLatched[clientsCount];

    void waitForMemory() {
        do {
            wait();
        } while (!memory.inpCompleted.read());
    }

    void listenClients() {
        while (true) {
            wait();
            for (unsigned int i = 0; i < clientsCount; i++) {
                ClientPorts &clientPorts = clients[i];
                ClientLatchedSignals &clientSignals = clientsLatched[i];

                if (clientPorts.inpEnable.read()) {
                    clientSignals.enable.write(1);
                    clientSignals.write.write(clientPorts.inpWrite);
                    clientSignals.address.write(clientPorts.inpAddress);
                    clientSignals.data.write(clientPorts.inpData);
                }
            }
        }
    }

    void main() {
        unsigned int currentClient = clientsCount - 1;
        int clientForOutputClear = -1;

        while (true) {
            wait();

            for (unsigned int i = 0; i < clientsCount; i++) {
                // If we returned data for some client in previous cycle, now we can clear this data back to 0
                if (clientForOutputClear != -1) {
                    clients[clientForOutputClear].outCompleted.write(0);
                    outData.write(0);
                    clientForOutputClear = -1;
                }

                // Get our client and go to the next one
                currentClient = (currentClient + 1) % clientsCount;
                ClientPorts &client = clients[currentClient];
                ClientLatchedSignals &clientLatched = clientsLatched[currentClient];

                // Skip iteration this client, if it didn't issue any memory operation
                if (!clientLatched.enable.read()) {
                    continue;
                }

                // Make a request to the memory
                const bool memoryWrite = clientLatched.write.read();
                memory.outEnable.write(1);
                memory.outWrite.write(memoryWrite);
                memory.outAddress.write(clientLatched.address.read());
                if (memoryWrite) {
                    memory.outData.write(clientLatched.data.read());

                    wait();
                    memory.outEnable.write(0);
                    memory.outWrite.write(0);

                    waitForMemory();
                } else {
                    wait();
                    memory.outEnable.write(0);

                    waitForMemory();

                    outData.write(memory.inpData.read());
                }

                // Signalize success and save current client index, so the signal will be set back to
                // 0 during the next cycle.
                client.outCompleted.write(1);
                clientForOutputClear = currentClient;

                // Disable latched enable signal, so we don't issue it to the memory the second time
                clientLatched.enable.write(0);

                // We service only one client per clock - break out
                break;
            }
        }
    }

    SC_CTOR(MemoryController) {
        SC_CTHREAD(main, inpClock.pos());
        SC_CTHREAD(listenClients, inpClock.pos());
    }
};
