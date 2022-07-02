#include "gpu/blocks/memory.h"
#include "gpu/blocks/memory_controller.h"
#include "gpu/util/port_connector.h"
#include "gpu/util/vcd_trace.h"
#include "gpu_tests/test_utils.h"

#include <systemc.h>

SC_MODULE(Client0) {
    sc_in_clk inpClk;
    sc_out<bool> outEnable;
    sc_out<bool> outWrite;
    sc_out<MemoryAddressType> outAddress;
    sc_out<MemoryDataType> outData;
    sc_in<MemoryDataType> inpData;
    sc_in<bool> inpCompleted;

    TESTER("Client0", 3);

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

    TESTER("Client1", 3);

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

    PortConnector ports = {};

    Memory<32> mem("mem");
    MemoryController<2> memController("memController");
    Client0 client0{"client0"};
    Client1 client1{"client1"};

    // Bind mem with memController
    ports.connectMemoryToClient(memController.memory, mem, "MEMCTL_MEM");

    // Bind client0 with memController
    ports.connectMemoryToClient<MemoryClientType::ReadWrite, MemoryServerType::SeparateOutData>(client0, memController.clients[0], "MEMCTL_CLIENT0");
    ports.connectMemoryToClient<MemoryClientType::ReadWrite, MemoryServerType::SeparateOutData>(client1, memController.clients[1], "MEMCTL_CLIENT1");
    ports.connectPorts(client0.inpData, client1.inpData, memController.outData, "MEMCTL_dataForRead");

    // Setup trace
    VcdTrace trace{"memory"};
    ADD_TRACE(clock);
    ports.addSignalsToTrace(trace);

    // Bind clock for every component
    mem.inpClock(clock);
    memController.inpClock(clock);
    client0.inpClk(clock);
    client1.inpClk(clock);

    sc_start({65, SC_NS});

    return client0.verify() || client1.verify();
}
