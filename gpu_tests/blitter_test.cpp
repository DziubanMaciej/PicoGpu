#include "gpu/blocks/blitter.h"
#include "gpu/blocks/memory.h"
#include "gpu/blocks/memory_controller.h"
#include "gpu/util/port_connector.h"
#include "gpu/util/vcd_trace.h"
#include "gpu_tests/test_utils.h"

#include <systemc.h>

SC_MODULE(Tester) {
    sc_in_clk inpClock;
    sc_out<sc_uint<2>> outCommandType;
    sc_out<MemoryAddressType> outMemoryPtr;
    sc_out<PointerType> outUserPtr;
    sc_out<sc_uint<16>> outSizeInDwords;

    SC_HAS_PROCESS(Tester);

    TESTER("Test", 2);

    Blitter &blitter;

    Tester(::sc_core::sc_module_name, Blitter & blitter)
        : blitter(blitter) {
        SC_THREAD(main);
        sensitive << inpClock.pos();
    }

    void blit(Blitter::CommandType blitType, MemoryAddressType memoryPtr, uint32_t * userPtr, size_t sizeInDwords) {
        outCommandType = static_cast<size_t>(blitType);
        outMemoryPtr = memoryPtr;
        outUserPtr = reinterpret_cast<uintptr_t>(userPtr);
        outSizeInDwords = sizeInDwords;

        wait();

        outCommandType = 0;
        outMemoryPtr = 0;
        outUserPtr = 0;
        outSizeInDwords = 0;
    }

    void main() {
        bool success = true;
        wait(3);

        uint32_t data1[] = {
            0xdeadbeef,
            0x12345678,
            0x00000001,
        };
        uint32_t data2[] = {
            0x33333333,
            0x44444444,
            0x55555555,
            0x66666666,
        };

        blit(Blitter::CommandType::CopyToMem, 0x04, data1, 3);
        waitForBlitter("Written 3 dwords");

        blit(Blitter::CommandType::CopyToMem, 0x0C, data2, 4);
        waitForBlitter("Written 4 dwords");

        uint32_t readData[6] = {};
        blit(Blitter::CommandType::CopyFromMem, 0x04, readData, 6); // Read from locations 0x04 to 0x18
        waitForBlitter("Read 6 dwords");

        ASSERT_EQ(data1[0], readData[0]);
        ASSERT_EQ(data1[1], readData[1]);
        ASSERT_EQ(data2[0], readData[2]);
        ASSERT_EQ(data2[1], readData[3]);
        ASSERT_EQ(data2[2], readData[4]);
        ASSERT_EQ(data2[3], readData[5]);
        SUMMARY_RESULT("Data validation after memory blit");

        // Fill locations from 0x08 to 0x14
        uint32_t dataForFill = 0x112112;
        blit(Blitter::CommandType::FillMem, 0x08, &dataForFill, 4);
        waitForBlitter("Filled 4 dwords");

        // Read from locations 0x04 to 0x18
        {
            uint32_t readData[6] = {};
            blit(Blitter::CommandType::CopyFromMem, 0x04, readData, 6);
            waitForBlitter("Read 6 dwords");

            ASSERT_EQ(data1[0], readData[0]);
            ASSERT_EQ(dataForFill, readData[1]);
            ASSERT_EQ(dataForFill, readData[2]);
            ASSERT_EQ(dataForFill, readData[3]);
            ASSERT_EQ(dataForFill, readData[4]);
            ASSERT_EQ(data2[3], readData[5]);
            SUMMARY_RESULT("Data validation after memory fill");
        }
    }

    void waitForBlitter(const char *message) {
        int cycles = 0;
        do {
            wait(1);
            cycles++;
        } while (blitter.profiling.outBusy.read());
        Log() << message << " in " << cycles << " cycles";
    }
};

int sc_main(int argc, char *argv[]) {
    const bool useMemoryController = argc > 1 && static_cast<bool>(argv[1][0] - '0');

    PortConnector ports = {};

    sc_clock clock("clock", 1, SC_NS, 0.5, 0, SC_NS, true);
    Memory<64> mem{"mem"};
    std::unique_ptr<MemoryController<1>> memController = {};
    if (useMemoryController) {
        memController = std::make_unique<MemoryController<1>>("memController");
    }
    Blitter blitter("blitter");
    Tester tester("tester", blitter);

    // Bind mem with memController or blitter
    if (useMemoryController) {
        ports.connectMemoryToClient(memController->memory, mem, "MEMCTL_MEM");
        ports.connectMemoryToClient<MemoryClientType::ReadWrite, MemoryServerType::SeparateOutData>(blitter.memory, memController->clients[0], "MEMCTL_BLT");
        ports.connectPorts(blitter.memory.inpData, memController->outData, "MEMCTL_BLT_dataForRead");
    } else {
        ports.connectMemoryToClient(blitter.memory, mem, "MEM_BLT");
    }

    // Bind blitter with tester
    ports.connectPorts(blitter.command.inpCommandType, tester.outCommandType, "commandType");
    ports.connectPorts(blitter.command.inpMemoryPtr, tester.outMemoryPtr, "memoryPtr");
    ports.connectPorts(blitter.command.inpUserPtr, tester.outUserPtr, "userPtr");
    ports.connectPorts(blitter.command.inpSizeInDwords, tester.outSizeInDwords, "size");

    // Bind clock for every component
    mem.inpClock(clock);
    if (useMemoryController) {
        memController->inpClock(clock);
    }
    blitter.inpClock(clock);
    tester.inpClock(clock);

    // Bind profiling ports to dummy signals
    if (useMemoryController) {
        ports.connectPort(memController->profiling.outBusy, "MEMCTL_busy");
        ports.connectPort(memController->profiling.outReadsPerformed, "MEMCTL_reads");
        ports.connectPort(memController->profiling.outWritesPerformed, "MEMCTL_writes");
    }
    ports.connectPort(blitter.profiling.outBusy, "BLT_busy");

    // Add vcd trace
    std::string traceName{TEST_NAME};
    traceName.append(useMemoryController ? "WithMemoryController" : "WithoutMemoryController");
    VcdTrace trace{traceName.c_str()};
    ADD_TRACE(clock);
    ports.addSignalsToTrace(trace);

    sc_start({200, SC_NS});
    return tester.verify();
}