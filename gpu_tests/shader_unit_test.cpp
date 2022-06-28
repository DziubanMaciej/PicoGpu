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

    TESTER("Tester", 6);

    SC_CTOR(Tester) {
        SC_THREAD(main);
        sensitive << inpClock.pos();
    }

    void setupProgramManually(uint32_t * outProgram, uint32_t & outSize) {
        struct DataStream {
            Isa::Command::CommandStoreIsa command = {};
            Isa::InstructionLayouts::BinaryMathImm isa0;
            Isa::InstructionLayouts::BinaryMathImm isa1;
            Isa::InstructionLayouts::BinaryMathImm isa2;
            Isa::InstructionLayouts::UnaryMath isa3;
        };

        DataStream &dataStream = reinterpret_cast<DataStream &>(*outProgram);
        outSize = sizeof(dataStream);

        dataStream.command.commandType = Isa::Command::CommandType::StoreIsa;
        dataStream.command.hasNextCommand = 1;
        dataStream.command.programLength = 7;
        dataStream.command.inputsCount = Isa::Command::NonZeroCount::One;
        dataStream.command.outputsCount = Isa::Command::NonZeroCount::One;
        dataStream.command.inputSize0 = Isa::Command::NonZeroCount::Three;
        dataStream.command.outputSize0 = Isa::Command::NonZeroCount::Four;

        dataStream.isa0.opcode = Isa::Opcode::add_imm;
        dataStream.isa0.src = Isa::RegisterSelection::i0;
        dataStream.isa0.dest = Isa::RegisterSelection::r0;
        dataStream.isa0.destMask = 0b1100;
        dataStream.isa0.immediateValue = 100;

        dataStream.isa1.opcode = Isa::Opcode::add_imm;
        dataStream.isa1.src = Isa::RegisterSelection::i0;
        dataStream.isa1.dest = Isa::RegisterSelection::r0;
        dataStream.isa1.destMask = 0b0010;
        dataStream.isa1.immediateValue = 1000;

        dataStream.isa2.opcode = Isa::Opcode::add_imm;
        dataStream.isa2.src = Isa::RegisterSelection::i0;
        dataStream.isa2.dest = Isa::RegisterSelection::r0;
        dataStream.isa2.destMask = 0b0001;
        dataStream.isa2.immediateValue = 10000;

        dataStream.isa3.opcode = Isa::Opcode::mov;
        dataStream.isa3.src = Isa::RegisterSelection::r0;
        dataStream.isa3.dest = Isa::RegisterSelection::o0;
        dataStream.isa3.destMask = 0b1111;
    }

    void setupProgramFromAssembler(uint32_t * outProgram, uint32_t & outSize) {
        const char *code =
            "#input i0.xyz\n"
            "#output o0.xyzw\n"
            "add r0.xy i0 100\n"
            "add r0.z  i0 1000\n"
            "add r0.w  i0 10000\n"
            "mov o0 r0\n";
        Isa::PicoGpuBinary binary = {};
        int result = Isa::assembly(code, &binary);
        FATAL_ERROR_IF(result != 0, "Failed to assemble code");
        binary.setHasNextCommand();
        auto &data = binary.getData();

        std::copy(data.begin(), data.end(), outProgram);
        outSize = data.size() * sizeof(uint32_t);
    }

    void verify(uint32_t * dataStream, uint32_t dataStreamSize, const std::string &comment) {
        struct ExecuteCommand {
            Isa::Command::CommandExecuteIsa command;
            uint32_t inputInits[9];
        } executeCommand;
        executeCommand.command.commandType = Isa::Command::CommandType::ExecuteIsa;
        executeCommand.command.threadCount = 3;
        executeCommand.inputInits[0] = 10;
        executeCommand.inputInits[1] = 20;
        executeCommand.inputInits[2] = 30;
        executeCommand.inputInits[3] = 40;
        executeCommand.inputInits[4] = 50;
        executeCommand.inputInits[5] = 60;
        executeCommand.inputInits[6] = 70;
        executeCommand.inputInits[7] = 80;
        executeCommand.inputInits[8] = 90;

        memcpy(dataStream + dataStreamSize / sizeof(uint32_t), &executeCommand, sizeof(ExecuteCommand));
        dataStreamSize += sizeof(ExecuteCommand);

        bool success = true;

        Handshake::sendArray(request.inpReceiving, request.outSending, request.outData, dataStream, dataStreamSize / sizeof(uint32_t));

        uint32_t output[12] = {};
        Handshake::receiveArray(response.inpSending, response.inpData, response.outReceiving, output, sizeof(output) / sizeof(uint32_t));

        ASSERT_EQ(110, output[0]);
        ASSERT_EQ(120, output[1]);
        ASSERT_EQ(1030, output[2]);
        ASSERT_EQ(10000, output[3]);
        SUMMARY_RESULT(comment + " (lane 0)");

        ASSERT_EQ(140, output[4]);
        ASSERT_EQ(150, output[5]);
        ASSERT_EQ(1060, output[6]);
        ASSERT_EQ(10000, output[7]);
        SUMMARY_RESULT(comment + " (lane 1)");

        ASSERT_EQ(170, output[8]);
        ASSERT_EQ(180, output[9]);
        ASSERT_EQ(1090, output[10]);
        ASSERT_EQ(10000, output[11]);
        SUMMARY_RESULT(comment + " (lane 2)");
    }

    void main() {

        {
            uint32_t dataStream[Isa::maxIsaSize] = {};
            uint32_t dataSteamSize = {};
            setupProgramManually(dataStream, dataSteamSize);
            verify(dataStream, dataSteamSize, "Program hardcoded manually ");
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
