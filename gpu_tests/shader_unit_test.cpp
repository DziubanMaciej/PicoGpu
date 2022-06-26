#include "gpu/blocks/shader_array/shader_unit.h"
#include "gpu/isa/assembler/assembler.h"
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

    TESTER("Tester", 2);

    SC_CTOR(Tester) {
        SC_THREAD(main);
        sensitive << inpClock.pos();
    }

    void setupProgramManually(uint32_t * outProgram, uint32_t & outSize) {
        struct DataStream {
            Isa::Command::CommandStoreIsa command = {};
            Isa::InstructionLayouts::BinaryMathImm isa0;
            Isa::InstructionLayouts::BinaryMathImm isa1;
            Isa::InstructionLayouts::UnaryMath isa2;
        };

        DataStream &dataStream = reinterpret_cast<DataStream &>(*outProgram);
        outSize = sizeof(dataStream);

        dataStream.command.commandType = Isa::Command::CommandType::StoreIsa;
        dataStream.command.hasNextCommand = 1;
        dataStream.command.programLength = 5;
        dataStream.command.inputsCount = Isa::Command::NonZeroCount::One;
        dataStream.command.outputsCount = Isa::Command::NonZeroCount::One;
        dataStream.command.inputSize0 = Isa::Command::NonZeroCount::Four;
        dataStream.command.outputSize0 = Isa::Command::NonZeroCount::Four;

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
    }

    void setupProgramFromAssembler(uint32_t * outProgram, uint32_t & outSize) {
        const char *code =
            "#input i0.xyzw\n"
            "#output o0.xyzw\n"
            "add r0.xz i0 100\n"
            "add r0.yw i0 1000\n"
            "mov o0 r0\n";
        Isa::PicoGpuBinary binary = {};
        int result = Isa::assembly(code, &binary);
        FATAL_ERROR_IF(result != 0, "Failed to assemble code");
        binary.setHasNextCommand();
        auto &data = binary.getData();

        std::copy(data.begin(), data.end(), outProgram);
        outSize = data.size() * sizeof(uint32_t);
    }

    void verify(uint32_t * dataStream, uint32_t dataStreamSize, const char *comment) {
        struct ExecuteCommand {
            Isa::Command::CommandExecuteIsa command;
            uint32_t inputInits[4];
        } executeCommand;
        executeCommand.command.commandType = Isa::Command::CommandType::ExecuteIsa;
        executeCommand.command.threadCount = 1;
        executeCommand.inputInits[0] = 10;
        executeCommand.inputInits[1] = 20;
        executeCommand.inputInits[2] = 30;
        executeCommand.inputInits[3] = 40;

        memcpy(dataStream + dataStreamSize / sizeof(uint32_t), &executeCommand, sizeof(ExecuteCommand));
        dataStreamSize += sizeof(ExecuteCommand);

        bool success = true;

        Handshake::sendArray(request.inpReceiving, request.outSending, request.outData, dataStream, dataStreamSize / sizeof(uint32_t));

        uint32_t output[4] = {};
        Handshake::receiveArray(response.inpSending, response.inpData, response.outReceiving, output, 4);

        ASSERT_EQ(110, output[0]);
        ASSERT_EQ(1020, output[1]);
        ASSERT_EQ(130, output[2]);
        ASSERT_EQ(1040, output[3]);
        SUMMARY_RESULT(comment);
    }

    void main() {

        {
            uint32_t dataStream[Isa::maxIsaSize] = {};
            uint32_t dataSteamSize = {};
            setupProgramManually(dataStream, dataSteamSize);
            verify(dataStream, dataSteamSize, "Program hardcoded manually");
        }

        {
            uint32_t dataStream[Isa::maxIsaSize] = {};
            uint32_t dataSteamSize = {};
            setupProgramFromAssembler(dataStream, dataSteamSize);
            verify(dataStream, dataSteamSize, "Program assembled from code");
        }
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

    sc_start({200000, SC_NS});

    return tester.verify();
}
