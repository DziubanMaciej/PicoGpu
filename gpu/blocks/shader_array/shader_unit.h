#pragma once

#include "gpu/isa/isa.h"
#include "gpu/isa/vector_register.h"
#include "gpu/types.h"

#include <systemc.h>

enum class ShaderUnitOperation {
    StoreIsa = 0,
    Execute = 1,
};

SC_MODULE(ShaderUnit) {
    sc_in_clk inpClock;

    struct {
        sc_in<bool> inpSending;
        sc_in<sc_uint<32>> inpData;
        sc_out<bool> outReceiving;
    } request;

    struct {
        sc_in<bool> inpReceiving;
        sc_out<bool> outSending;
        sc_out<sc_uint<32>> outData;
    } response;

    SC_CTOR(ShaderUnit) {
        SC_CTHREAD(main, inpClock.pos());
    }

    void main();

private:
    void processStoreIsaCommand(Isa::Command::CommandStoreIsa command);
    void processExecuteIsaCommand(Isa::Command::CommandExecuteIsa command);

    void initializeInputRegisters(uint32_t threadCount);
    void appendOutputRegistersValues(uint32_t threadCount, uint32_t * outputStream, uint32_t & outputStreamSize);
    VectorRegister &selectRegister(Isa::RegisterSelection selection, uint32_t lane);

    using UnaryFunction = int32_t (*)(int32_t);
    using BinaryFunction = int32_t (*)(int32_t, int32_t);
    using BinaryVectorScalarFunction = int32_t (*)(VectorRegister, VectorRegister);
    using BinaryVectorVectorFunction = VectorRegister (*)(VectorRegister, VectorRegister);
    void executeInstructions(uint32_t isaSize, uint32_t threadCount);
    int32_t executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::UnaryMath &inst, UnaryFunction function);
    int32_t executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::BinaryMath &inst, BinaryFunction function);
    int32_t executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::BinaryMath &inst, BinaryVectorScalarFunction function);
    int32_t executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::BinaryMath &inst, BinaryVectorVectorFunction function);
    int32_t executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::UnaryMathImm &inst, UnaryFunction function);
    int32_t executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::BinaryMathImm &inst, BinaryFunction function);
    int32_t executeInstructionLane(uint32_t lane, const Isa::InstructionLayouts::Swizzle &inst);

    template <typename... Args>
    int32_t executeInstructionForLanes(uint32_t threadCount, Args && ...args) {
        int32_t pcIncrement = 0;
        for (uint32_t lane = 0; lane < threadCount; lane++) {
            pcIncrement = executeInstructionLane(lane, std::forward<Args>(args)...);
        }
        return pcIncrement;
    }

    Isa::Command::CommandStoreIsa isaMetadata = {};
    uint32_t isa[Isa::maxIsaSize] = {};

    struct PerLaneRegisters {
        VectorRegister i0;
        VectorRegister i1;
        VectorRegister i2;
        VectorRegister i3;
        VectorRegister o0;
        VectorRegister o1;
        VectorRegister o2;
        VectorRegister o3;

        VectorRegister r0;
        VectorRegister r1;
        VectorRegister r2;
        VectorRegister r3;
        VectorRegister r4;
        VectorRegister r5;
        VectorRegister r6;
        VectorRegister r7;
    };
    struct Registers {
        PerLaneRegisters lanes[Isa::simdSize];
        uint32_t pc;
    } registers;
};
