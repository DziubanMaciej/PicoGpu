#include "gpu/blocks/shader_array/shader_unit.h"
#include "gpu/util/handshake.h"
#include "gpu/util/vcd_trace.h"
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

    TESTER("Tester", 1);

    SC_CTOR(Tester) {
        SC_THREAD(main);
        sensitive << inpClock.pos();
    }

    void main() {
        bool success = true;

        struct DataStream {
            Isa::Header header = {};
            Isa::InstructionLayouts::BinaryMathImm isa0;
            Isa::InstructionLayouts::BinaryMathImm isa1;
            Isa::InstructionLayouts::UnaryMath isa2;
            uint32_t inputInit[4];
        } dataStream = {};

        dataStream.header.dword1.programLength = 5;
        dataStream.header.dword2.inputSize0 = 4;
        dataStream.header.dword2.outputSize0 = 4;

        dataStream.isa0.opcode = Isa::Opcode::add_imm;
        dataStream.isa0.src = Isa::RegisterSelection::i0;
        dataStream.isa0.dest = Isa::RegisterSelection::r0;
        dataStream.isa0.destMask = 0b1010;
        dataStream.isa0.immediateValue = 100;

        dataStream.isa1.opcode = Isa::Opcode::add_imm;
        dataStream.isa1.src = Isa::RegisterSelection::i0;
        dataStream.isa1.dest = Isa::RegisterSelection::r0;
        dataStream.isa1.destMask = 0b0101;
        dataStream.isa1.immediateValue = 1000;

        dataStream.isa2.opcode = Isa::Opcode::mov;
        dataStream.isa2.src = Isa::RegisterSelection::r0;
        dataStream.isa2.dest = Isa::RegisterSelection::o0;
        dataStream.isa2.destMask = 0b1111;

        dataStream.inputInit[0] = 10;
        dataStream.inputInit[1] = 20;
        dataStream.inputInit[2] = 30;
        dataStream.inputInit[3] = 40;

        uint32_t *dataStreamRaw = (uint32_t *)&dataStream;
        uint32_t dataStreamCount = sizeof(DataStream) / 4;
        sc_uint<32> token = dataStreamRaw[0];
        Handshake::send(request.inpReceiving, request.outSending, request.outData, token);
        for (int i = 1; i < dataStreamCount; i++) {
            request.outData = dataStreamRaw[i];
            wait();
        }

        sc_uint<32> output[4] = {};
        output[0] = Handshake::receive(response.inpSending, response.inpData, response.outReceiving);
        for (int i = 1; i < 4; i++) {
            wait();
            output[i] = response.inpData.read();
        }

        ASSERT_EQ(110, output[0]);
        ASSERT_EQ(1020, output[1]);
        ASSERT_EQ(130, output[2]);
        ASSERT_EQ(1040, output[3]);
        SUMMARY_RESULT("Program");
    }
};

int sc_main(int argc, char *argv[]) {
    sc_set_time_resolution(100, SC_PS);
    sc_clock clock("my_clock", 1, SC_NS, 0.5, 0, SC_NS, true);

    Tester tester{"tester"};
    ShaderUnit shaderUnit{"shaderUnit"};

    sc_signal<bool> requestSending;
    sc_signal<sc_uint<32>> requestData;
    sc_signal<bool> requestReceiving;
    sc_signal<bool> responseReceiving;
    sc_signal<bool> responeSending;
    sc_signal<sc_uint<32>> responseData;
    shaderUnit.inpClock(clock);
    shaderUnit.request.inpSending(requestSending);
    shaderUnit.request.outReceiving(requestReceiving);
    shaderUnit.request.inpData(requestData);
    shaderUnit.response.outSending(responeSending);
    shaderUnit.response.inpReceiving(responseReceiving);
    shaderUnit.response.outData(responseData);
    tester.inpClock(clock);
    tester.request.outSending(requestSending);
    tester.request.inpReceiving(requestReceiving);
    tester.request.outData(requestData);
    tester.response.inpSending(responeSending);
    tester.response.outReceiving(responseReceiving);
    tester.response.inpData(responseData);

    VcdTrace trace{TEST_NAME};
    ADD_TRACE(clock);
    ADD_TRACE(requestSending);
    ADD_TRACE(requestData);
    ADD_TRACE(requestReceiving);
    ADD_TRACE(responseReceiving);
    ADD_TRACE(responeSending);
    ADD_TRACE(responseData);

    sc_start({2000, SC_NS});

    return tester.verify();
}
