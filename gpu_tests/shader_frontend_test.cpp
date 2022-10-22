#include "gpu/blocks/shader_array/shader_frontend.h"
#include "gpu/blocks/shader_array/shader_unit.h"
#include "gpu/isa/assembler/assembler.h"
#include "gpu/util/handshake.h"
#include "gpu/util/port_connector.h"
#include "gpu/util/vcd_trace.h"
#include "gpu_tests/debug_memory.h"
#include "gpu_tests/test_utils.h"

#include <systemc.h>

SC_MODULE(Tester) {
    sc_in_clk inpClock;

    struct {
        sc_out<bool> outSending;
        sc_out<sc_uint<32>> outData;
        sc_in<bool> inpReceiving;
    } request;

    struct {
        sc_out<bool> outReceiving;
        sc_in<bool> inpSending;
        sc_in<sc_uint<32>> inpData;
    } response;

    TESTER("Tester", 6);

    uint32_t longShaderAddress;
    uint32_t shortShaderAddress;

    SC_HAS_PROCESS(Tester);
    Tester(::sc_core::sc_module_name, DebugMemory<1024> & memory) {
        generateShaders(memory);

        SC_THREAD(main);
        sensitive << inpClock.pos();
    }

    void main() {
        sendRequest(longShaderAddress, 123);

        for (int i = 0; i < 5; i++) {
            sendRequest(shortShaderAddress, 124 + i);
            expectResponse(124 + i);
        }

        expectResponse(123);
    }

private:
    void sendRequest(uint32_t isaAddress, uint32_t clientToken) {
        struct Request {
            ShaderFrontendRequest request;
            uint32_t shaderInputs[12];
        } shaderFrontendRequest = {};
        shaderFrontendRequest.request.dword0.isaAddress = isaAddress;
        shaderFrontendRequest.request.dword1.clientToken = clientToken;
        shaderFrontendRequest.request.dword1.threadCount = intToNonZeroCount(3);
        shaderFrontendRequest.request.dword2.inputsCount = NonZeroCount::One;
        shaderFrontendRequest.request.dword2.inputSize0 = NonZeroCount::Four;
        shaderFrontendRequest.request.dword2.outputsCount = NonZeroCount::One;
        shaderFrontendRequest.request.dword2.outputSize0 = NonZeroCount::Four;
        shaderFrontendRequest.shaderInputs[0] = 10;
        shaderFrontendRequest.shaderInputs[1] = 20;
        shaderFrontendRequest.shaderInputs[2] = 30;
        shaderFrontendRequest.shaderInputs[3] = 40;
        shaderFrontendRequest.shaderInputs[4] = 50;
        shaderFrontendRequest.shaderInputs[5] = 60;
        shaderFrontendRequest.shaderInputs[6] = 70;
        shaderFrontendRequest.shaderInputs[7] = 80;
        shaderFrontendRequest.shaderInputs[8] = 90;
        shaderFrontendRequest.shaderInputs[9] = 100;
        shaderFrontendRequest.shaderInputs[10] = 110;
        shaderFrontendRequest.shaderInputs[11] = 120;

        Handshake::sendArray(request.inpReceiving, request.outSending, request.outData,
                             reinterpret_cast<uint32_t *>(&shaderFrontendRequest), sizeof(Request) / sizeof(uint32_t));
    }

    void expectResponse(uint32_t clientToken) {
        struct Response {
            ShaderFrontendResponse response;
            uint32_t shaderOutputs[12];
        } shaderFrontendResponse;
        Handshake::receiveArray(response.inpSending, response.inpData, response.outReceiving,
                                reinterpret_cast<uint32_t *>(&shaderFrontendResponse), sizeof(Response) / sizeof(uint32_t));

        bool success = true;
        ASSERT_EQ(clientToken, shaderFrontendResponse.response.dword0.clientToken);
        ASSERT_EQ(110, shaderFrontendResponse.shaderOutputs[0]);
        ASSERT_EQ(1020, shaderFrontendResponse.shaderOutputs[1]);
        ASSERT_EQ(130, shaderFrontendResponse.shaderOutputs[2]);
        ASSERT_EQ(1040, shaderFrontendResponse.shaderOutputs[3]);
        ASSERT_EQ(150, shaderFrontendResponse.shaderOutputs[4]);
        ASSERT_EQ(1060, shaderFrontendResponse.shaderOutputs[5]);
        ASSERT_EQ(170, shaderFrontendResponse.shaderOutputs[6]);
        ASSERT_EQ(1080, shaderFrontendResponse.shaderOutputs[7]);
        ASSERT_EQ(190, shaderFrontendResponse.shaderOutputs[8]);
        ASSERT_EQ(1100, shaderFrontendResponse.shaderOutputs[9]);
        ASSERT_EQ(210, shaderFrontendResponse.shaderOutputs[10]);
        ASSERT_EQ(1120, shaderFrontendResponse.shaderOutputs[11]);
        SUMMARY_RESULT("ShaderFrontend test with client token " + std::to_string(clientToken));
    }

    void generateShaders(DebugMemory<1024> & memory) {
        Isa::PicoGpuBinary binary = {};
        auto &data = binary.getData();

        shortShaderAddress = 0;
        std::string code =
            "#input r0.xyzw\n"
            "#output r12.xyzw\n"
            "iadd r3.xz r0 100\n"
            "iadd r3.yw r0 1000\n"
            "mov r12 r3\n";
        int result = Isa::assembly(code.c_str(), &binary);
        FATAL_ERROR_IF(result != 0, "Failed to assemble code");
        memory.blitToMemory(shortShaderAddress, data.data(), data.size());

        longShaderAddress = data.size() * sizeof(uint32_t);
        code =
            "#input r0.xyzw\n"
            "#output r12.xyzw\n"
            "iadd r3.xz r0 100\n"
            "iadd r3.yw r0 1000\n";
        for (int i = 0; i < 500; i++) {
            code += "mov r2 r2\n"; // just some garbage instructions that take time
        }
        code += "mov r12 r3\n";
        binary.reset();
        result = Isa::assembly(code.c_str(), &binary);
        FATAL_ERROR_IF(result != 0, "Failed to assemble code");
        memory.blitToMemory(longShaderAddress, data.data(), data.size());
    }
};

int sc_main(int argc, char *argv[]) {
    sc_set_time_resolution(100, SC_PS);
    sc_clock clock("my_clock", 1, SC_NS, 0.5, 0, SC_NS, true);

    PortConnector ports = {};

    DebugMemory<1024> memory{"memory"};
    Tester tester{"tester", memory};
    ShaderFrontend<1, 2> shaderFrontend{"shaderFrontend"};
    ShaderUnit shaderUnit0{"shaderUnit0"};
    ShaderUnit shaderUnit1{"shaderUnit1"};
    ShaderUnit *shaderUnits[] = {&shaderUnit0, &shaderUnit1};

    // Connect clocks
    tester.inpClock(clock);
    shaderFrontend.inpClock(clock);
    memory.inpClock(clock);
    shaderUnit0.inpClock(clock);
    shaderUnit1.inpClock(clock);

    // Connect frontend with tester
    ports.connectHandshake(tester.request, shaderFrontend.clientInterfaces[0].request, "CLIENT_SF_REQ");
    ports.connectHandshake(shaderFrontend.clientInterfaces[0].response, tester.response, "CLIENT_SF_RESP");

    // Connect frontend to memory
    ports.connectMemoryToClient<MemoryClientType::ReadOnly>(shaderFrontend.memory, memory, "SF");

    // Connect frontend to shader units
    for (int i = 0; i < 2; i++) {
        ports.connectHandshake(shaderFrontend.shaderUnitInterfaces[i].request, shaderUnits[i]->request, "SF_SU" + std::to_string(i) + "_REQ");
        ports.connectHandshake(shaderUnits[i]->response, shaderFrontend.shaderUnitInterfaces[i].response, "SF_SU" + std::to_string(i) + "_RESP");
    }

    // Create a vcd trace
    VcdTrace trace{TEST_NAME};
    ADD_TRACE(clock);
    ports.addSignalsToTrace(trace);

    // Bind profiling ports to dummy signals
    ports.connectPort(shaderUnit0.profiling.outBusy, "SU0_busy");
    ports.connectPort(shaderUnit0.profiling.outThreadsStarted, "SU0_threadsStarted");
    ports.connectPort(shaderUnit0.profiling.outThreadsFinished, "SU0_threadsFinished");
    ports.connectPort(shaderUnit1.profiling.outBusy, "SU1_busy");
    ports.connectPort(shaderUnit1.profiling.outThreadsStarted, "SU1_threadsStarted");
    ports.connectPort(shaderUnit1.profiling.outThreadsFinished, "SU1_threadsFinished");
    ports.connectPort(shaderFrontend.profiling.outBusy, "SF_busy");
    ports.connectPort(shaderFrontend.profiling.outIsaFetches, "SF_isaFetches");

    // Run the simulation
    sc_start({200000, SC_NS});
    return tester.verify();
}
