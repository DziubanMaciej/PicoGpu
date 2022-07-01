#include "gpu/blocks/shader_array/shader_unit.h"
#include "gpu/isa/assembler/assembler.h"
#include "gpu/util/conversions.h"
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

    TESTER("Tester", 3);

    SC_CTOR(Tester) {
        SC_THREAD(main);
        sensitive << inpClock.pos();
    }

    struct TestCase {
        TestCase(const char *name) : name(name) {}
        void appendStoreCommand(Isa::PicoGpuBinary &isa) {
            const auto oldSize = inputDataStream.size();
            inputDataStream.resize(inputDataStream.size() + isa.getData().size());

            auto &storeCommand = isa.getData();
            isa.setHasNextCommand();
            std::copy(storeCommand.begin(),
                      storeCommand.end(),
                      &inputDataStream[oldSize]);
        }

        void appendExecuteCommand(uint32_t threadCount) {
            const auto oldSize = inputDataStream.size();
            inputDataStream.resize(inputDataStream.size() + sizeof(Isa::Command::CommandExecuteIsa) / sizeof(uint32_t));

            Isa::Command::CommandExecuteIsa command = {};
            command.commandType = Isa::Command::CommandType::ExecuteIsa;
            command.threadCount = 3;

            std::copy(reinterpret_cast<uint32_t *>(&command),
                      reinterpret_cast<uint32_t *>(&command) + sizeof(Isa::Command::CommandExecuteIsa) / sizeof(uint32_t),
                      &inputDataStream[oldSize]);
        }

        void appendShaderInputs(std::initializer_list<int32_t> inputs) {
            inputDataStream.reserve(inputDataStream.size() + inputs.size());

            for (auto it = inputs.begin(); it != inputs.end(); it++) {
                inputDataStream.push_back(*it);
            }
        }

        void appendExpectedOutputs(std::initializer_list<int32_t> outputs) {
            expectedOutputs.reserve(expectedOutputs.size() + outputs.size());

            for (auto it = outputs.begin(); it != outputs.end(); it++) {
                expectedOutputs.push_back(*it);
            }
        }

        const char *name;
        std::vector<uint32_t> inputDataStream;
        std::vector<int32_t> expectedOutputs;
        bool floatOutputs = false;
    };

    void executeTestCase(const TestCase &testCase) {
        Handshake::sendArray(request.inpReceiving, request.outSending, request.outData,
                             testCase.inputDataStream.data(), testCase.inputDataStream.size());

        std::vector<uint32_t> actualOutputs(testCase.expectedOutputs.size());
        Handshake::receiveArray(response.inpSending, response.inpData, response.outReceiving,
                                actualOutputs.data(), actualOutputs.size());

        bool success = true;
        for (size_t i = 0; i < actualOutputs.size(); i++) {
            if (testCase.floatOutputs) {
                float exp = Conversions::intBytesToFloat(testCase.expectedOutputs[i]);
                float act = Conversions::intBytesToFloat(actualOutputs[i]);
                ASSERT_EQ(exp, act);
            } else {
                ASSERT_EQ(testCase.expectedOutputs[i], actualOutputs[i]);
            }
        }
        SUMMARY_RESULT(testCase.name);
    }

    TestCase createManualTestCase() {
        struct DataStream {
            Isa::Command::CommandStoreIsa storeCommand = {};
            Isa::InstructionLayouts::BinaryMathImm isa0;
            Isa::InstructionLayouts::BinaryMathImm isa1;
            Isa::InstructionLayouts::BinaryMathImm isa2;
            Isa::InstructionLayouts::UnaryMath isa3;
            Isa::Command::CommandExecuteIsa executeCommand;
            uint32_t inputs[9];
        };

        TestCase testCase = {"manual"};
        testCase.inputDataStream.resize(sizeof(DataStream) / sizeof(uint32_t));
        DataStream &dataStream = *reinterpret_cast<DataStream *>(testCase.inputDataStream.data());

        dataStream.storeCommand.commandType = Isa::Command::CommandType::StoreIsa;
        dataStream.storeCommand.hasNextCommand = 1;
        dataStream.storeCommand.programLength = 7;
        dataStream.storeCommand.inputsCount = NonZeroCount::One;
        dataStream.storeCommand.outputsCount = NonZeroCount::One;
        dataStream.storeCommand.inputSize0 = NonZeroCount::Three;
        dataStream.storeCommand.outputSize0 = NonZeroCount::Four;

        dataStream.isa0.opcode = Isa::Opcode::iadd_imm;
        dataStream.isa0.src = Isa::RegisterSelection::i0;
        dataStream.isa0.dest = Isa::RegisterSelection::r0;
        dataStream.isa0.destMask = 0b1100;
        dataStream.isa0.immediateValuesCount = NonZeroCount::One;
        dataStream.isa0.immediateValues[0] = 100;

        dataStream.isa1.opcode = Isa::Opcode::iadd_imm;
        dataStream.isa1.src = Isa::RegisterSelection::i0;
        dataStream.isa1.dest = Isa::RegisterSelection::r0;
        dataStream.isa1.destMask = 0b0010;
        dataStream.isa1.immediateValuesCount = NonZeroCount::One;
        dataStream.isa1.immediateValues[0] = 1000;

        dataStream.isa2.opcode = Isa::Opcode::iadd_imm;
        dataStream.isa2.src = Isa::RegisterSelection::i0;
        dataStream.isa2.dest = Isa::RegisterSelection::r0;
        dataStream.isa2.destMask = 0b0001;
        dataStream.isa2.immediateValuesCount = NonZeroCount::One;
        dataStream.isa2.immediateValues[0] = 10000;

        dataStream.isa3.opcode = Isa::Opcode::mov;
        dataStream.isa3.src = Isa::RegisterSelection::r0;
        dataStream.isa3.dest = Isa::RegisterSelection::o0;
        dataStream.isa3.destMask = 0b1111;

        dataStream.executeCommand.commandType = Isa::Command::CommandType::ExecuteIsa;
        dataStream.executeCommand.threadCount = 3;

        dataStream.inputs[0] = 10;
        dataStream.inputs[1] = 20;
        dataStream.inputs[2] = 30;
        dataStream.inputs[3] = 40;
        dataStream.inputs[4] = 50;
        dataStream.inputs[5] = 60;
        dataStream.inputs[6] = 70;
        dataStream.inputs[7] = 80;
        dataStream.inputs[8] = 90;

        testCase.expectedOutputs = {
            110,
            120,
            1030,
            10000,
            140,
            150,
            1060,
            10000,
            170,
            180,
            1090,
            10000,
        };

        return testCase;
    }

    TestCase createSimpleTestCase() {
        const char *code =
            "#input i0.xyz\n"
            "#output o0.xyzw\n"
            "iadd r0.xy i0 100\n"
            "iadd r0.z  i0 1000\n"
            "iadd r0.w  i0 10000\n"
            "mov o0 r0\n";
        Isa::PicoGpuBinary binary = {};
        int result = Isa::assembly(code, &binary);
        FATAL_ERROR_IF(result != 0, "Failed to assemble code");

        auto &data = binary.getData();

        TestCase testCase{"simple"};
        testCase.appendStoreCommand(binary);
        testCase.appendExecuteCommand(3);
        testCase.appendShaderInputs({
            10,
            20,
            30,
            40,
            50,
            60,
            70,
            80,
            90,
        });
        testCase.appendExpectedOutputs({
            110,
            120,
            1030,
            10000,
            140,
            150,
            1060,
            10000,
            170,
            180,
            1090,
            10000,
        });
        return testCase;
    }

    TestCase createFloatTestCase() {
        const char *code = R"code(
            #input i0.x
            #output o0.xyz

            swizzle r0 i0.xxxx

            finit r4 2.f 4.f 8.f
            fmul  r0 r0 r4

            fadd  r0 r0 r4

            finit r4 3.f
            fsub r0 r0 r4

            finit r4 2.f
            fdiv r0 r0 r4

            finit r4 0.25
            fmul r0 r0 r4

            mov o0 r0
        )code";

        Isa::PicoGpuBinary binary = {};
        int result = Isa::assembly(code, &binary);
        FATAL_ERROR_IF(result != 0, "Failed to assemble code");

        auto &data = binary.getData();

        TestCase testCase{"float"};
        testCase.appendStoreCommand(binary);
        testCase.appendExecuteCommand(3);
        testCase.appendShaderInputs({
            Conversions::floatBytesToInt(5.f),
            Conversions::floatBytesToInt(10.f),
            Conversions::floatBytesToInt(15.f),
        });
        testCase.appendExpectedOutputs({
            Conversions::floatBytesToInt((5.f * 2.f + 2.f - 3.f) / 8.f),
            Conversions::floatBytesToInt((5.f * 4.f + 4.f - 3.f) / 8.f),
            Conversions::floatBytesToInt((5.f * 8.f + 8.f - 3.f) / 8.f),

            Conversions::floatBytesToInt((10.f * 2.f + 2.f - 3.f) / 8.f),
            Conversions::floatBytesToInt((10.f * 4.f + 4.f - 3.f) / 8.f),
            Conversions::floatBytesToInt((10.f * 8.f + 8.f - 3.f) / 8.f),

            Conversions::floatBytesToInt((15.f * 2.f + 2.f - 3.f) / 8.f),
            Conversions::floatBytesToInt((15.f * 4.f + 4.f - 3.f) / 8.f),
            Conversions::floatBytesToInt((15.f * 8.f + 8.f - 3.f) / 8.f),
        });

        testCase.floatOutputs = true;
        return testCase;
    }

    void main() {
        executeTestCase(createSimpleTestCase());
        executeTestCase(createManualTestCase());
        executeTestCase(createFloatTestCase());
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
