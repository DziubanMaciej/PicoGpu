#include "gpu/blocks/memory.h"
#include "gpu/blocks/memory_controller.h"
#include "gpu/util/vcd_trace.h"

#include <systemc.h>

// TODO refactor these macros and put them in a shared file
#define ASSERT_EQ(a, b)                                                                               \
    wait(SC_ZERO_TIME);                                                                               \
    {                                                                                                 \
        if ((a) != (b)) {                                                                             \
            success = false;                                                                          \
            scLog() << "ASSERT_EQ(" #a ", " #b ") at " << __FILE__ << ":" << __LINE__ << " failed\n"; \
        }                                                                                             \
    }
#define SUMMARY_RESULT(NAME)                                            \
    scLog() << NAME << " " << ((success) ? "SUCCEEDED\n" : "FAILED\n"); \
    success = true;

SC_MODULE(Client0) {
    sc_in_clk inpClk;
    sc_out<bool> outEnable;
    sc_out<bool> outWrite;
    sc_out<MemoryAddressType> outAddress;
    sc_out<MemoryDataType> outData;
    sc_in<MemoryDataType> inpData;
    sc_in<bool> inpCompleted;

    SC_CTOR(Client0) {
        SC_THREAD(main);
        sensitive << inpClk.pos();
    }

    void main() {
        bool success = true;
        wait(3);

        // Issue a memory write
        outEnable = 1;
        outWrite = 1;
        outAddress = 0x4;
        outData = 0xdeadbeef;
        ASSERT_EQ(false, inpCompleted);
        wait(1); // first cycle of latency - latch in memory controller
        outEnable = 0;
        outWrite = 0;
        wait(1); // second cycle of latency - transfering inputs from memory controller to memory
        ASSERT_EQ(false, inpCompleted);
        wait(1); // third cycle of latency - actual write
        ASSERT_EQ(false, inpCompleted);
        wait(1); // fourth cycle of latency - transfering output from memory to memory controller
        ASSERT_EQ(true, inpCompleted);
        wait(1);
        ASSERT_EQ(false, inpCompleted); // ensure this signal was only a pulse
        SUMMARY_RESULT("Writing 0xdeadbeef at 0x4 address with client0");

        // Issue a memory write simultaneously with the other client
        wait(26);
        outEnable = 1;
        outWrite = 1;
        outAddress = 0x8;
        outData = 0x12345678;
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        outEnable = 0;
        outWrite = 0;
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(true, inpCompleted);
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        SUMMARY_RESULT("Writing 0x12345678 at 0x8 address with client0");

        // Issue a memory read simultaneously with the other client
        wait(13);
        outEnable = 1;
        outWrite = 0;
        outAddress = 0xc;
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        outEnable = 0;
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(true, inpCompleted);
        ASSERT_EQ(0xabcdef12, inpData.read());
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        ASSERT_EQ(0, inpData.read());
        SUMMARY_RESULT("Reading 0xabcdef12 at 0xc address with client0");
    }
};

SC_MODULE(Client1) {
    sc_in_clk inpClk;
    sc_out<bool> outEnable;
    sc_out<bool> outWrite;
    sc_out<MemoryAddressType> outAddress;
    sc_out<MemoryDataType> outData;
    sc_in<MemoryDataType> inpData;
    sc_in<bool> inpCompleted;

    SC_CTOR(Client1) {
        SC_THREAD(main);
        sensitive << inpClk.pos();
    }

    void main() {
        bool success = true;
        wait(14);

        // Issue a memory read as client 1
        outEnable = 1;
        outWrite = 0;
        outAddress = 0x4;
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        outEnable = 0;
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(true, inpCompleted);
        ASSERT_EQ(0xdeadbeef, inpData.read());
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        ASSERT_EQ(0, inpData.read());
        SUMMARY_RESULT("Reading 0xdeadbeef at 0x4 address with client1");

        // Issue a memory write simultaneously with the other client
        wait(15);
        outEnable = 1;
        outWrite = 1;
        outAddress = 0xc;
        outData = 0xabcdef12;
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        outEnable = 0;
        outWrite = 0;
        // there should be a total of 7 cycles of latency
        // - 4 for the write on the other client (latch + transfer + operation + transfer)
        // - 3 for the write on this client (transfer + operation + transfer). Latch latency is hidden.
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(true, inpCompleted);
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        SUMMARY_RESULT("Writing 0xabcdef12 at 0xc address with client1");

        // Issue a memory read simultaneously with the other client
        wait(10);
        outEnable = 1;
        outWrite = 0;
        outAddress = 0x8;
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        outEnable = 0;
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        wait(1);
        ASSERT_EQ(true, inpCompleted);
        ASSERT_EQ(0x12345678, inpData.read());
        wait(1);
        ASSERT_EQ(false, inpCompleted);
        ASSERT_EQ(0, inpData.read());
        SUMMARY_RESULT("Reading 0x12345678 at 0x8 address with client1");
    }
};

int sc_main(int argc, char *argv[]) {
    sc_set_time_resolution(100, SC_PS);
    sc_clock clock("my_clock", 1, SC_NS, 0.5, 0, SC_NS, true);

    Memory<32> mem("mem");
    MemoryController<2, MemoryDataType> memController("memController");

    // Bind mem with memController
    sc_signal<bool> memInpEnable;
    sc_signal<bool> memInpWrite;
    sc_signal<MemoryAddressType> memInpAddress;
    sc_signal<MemoryDataType> memInpData;
    sc_signal<MemoryDataType> memOutData;
    sc_signal<bool> memOutCompleted;
    mem.inpEnable(memInpEnable);
    mem.inpWrite(memInpWrite);
    mem.inpAddress(memInpAddress);
    mem.inpData(memInpData);
    mem.outData(memOutData);
    mem.outCompleted(memOutCompleted);
    memController.memory.outEnable(memInpEnable);
    memController.memory.outWrite(memInpWrite);
    memController.memory.outAddress(memInpAddress);
    memController.memory.outData(memInpData);
    memController.memory.inpData(memOutData);
    memController.memory.inpCompleted(memOutCompleted);

    // Bind client signals with memController
    struct ClientSignals {
        sc_signal<bool> inpEnable;
        sc_signal<bool> inpWrite;
        sc_signal<MemoryAddressType> inpAddress;
        sc_signal<MemoryDataType> inpData;
        sc_signal<bool> outCompleted;
    } clientSignals[2];
    sc_signal<MemoryDataType> clientOutData;
    for (int i = 0; i < 2; i++) {
        memController.clients[i].inpEnable(clientSignals[i].inpEnable);
        memController.clients[i].inpWrite(clientSignals[i].inpWrite);
        memController.clients[i].inpAddress(clientSignals[i].inpAddress);
        memController.clients[i].inpData(clientSignals[i].inpData);
        memController.clients[i].outCompleted(clientSignals[i].outCompleted);
    }
    memController.outData(clientOutData);

    // Bind client0 with memController
    Client0 client0{"client0"};
    client0.outEnable(clientSignals[0].inpEnable);
    client0.outWrite(clientSignals[0].inpWrite);
    client0.outAddress(clientSignals[0].inpAddress);
    client0.outData(clientSignals[0].inpData);
    client0.inpData(clientOutData);
    client0.inpCompleted(clientSignals[0].outCompleted);

    // Bind client1 with memController
    Client1 client1{"client1"};
    client1.outEnable(clientSignals[1].inpEnable);
    client1.outWrite(clientSignals[1].inpWrite);
    client1.outAddress(clientSignals[1].inpAddress);
    client1.outData(clientSignals[1].inpData);
    client1.inpData(clientOutData);
    client1.inpCompleted(clientSignals[1].outCompleted);

    // Setup trace
    VcdTrace trace{"memory"};
    ADD_TRACE(clock);
    ADD_TRACE(memInpEnable);
    ADD_TRACE(memInpWrite);
    ADD_TRACE(memInpAddress);
    ADD_TRACE(memInpData);
    ADD_TRACE(memOutData);
    ADD_TRACE(memOutCompleted);
    trace.trace(clientSignals[0].inpEnable, "client0_inpEnable");
    trace.trace(clientSignals[0].inpWrite, "client0_inpWrite");
    trace.trace(clientSignals[0].inpAddress, "client0_inpAddress");
    trace.trace(clientSignals[0].inpData, "client0_inpData");
    trace.trace(clientSignals[0].outCompleted, "client0_outCompleted");
    trace.trace(clientSignals[1].inpEnable, "client1_inpEnable");
    trace.trace(clientSignals[1].inpWrite, "client1_inpWrite");
    trace.trace(clientSignals[1].inpAddress, "client1_inpAddress");
    trace.trace(clientSignals[1].inpData, "client1_inpData");
    trace.trace(clientSignals[1].outCompleted, "client1_outCompleted");
    ADD_TRACE(clientOutData);

    // Bind clock for every component
    mem.inpClock(clock);
    memController.inpClock(clock);
    client0.inpClk(clock);
    client1.inpClk(clock);

    sc_start({65, SC_NS});

    return 0;
}
