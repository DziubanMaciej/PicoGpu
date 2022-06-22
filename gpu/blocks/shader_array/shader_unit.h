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

    void initializeInputRegisters();
    void appendOutputRegistersValues(uint32_t * outputStream, uint32_t & outputStreamSize);
    VectorRegister &selectRegister(Isa::RegisterSelection selection);

    using UnaryFunction = uint32_t (*)(uint32_t);
    using BinaryFunction = uint32_t (*)(uint32_t, uint32_t);
    void executeInstruction(uint32_t * isa);
    void executeInstruction(Isa::InstructionLayouts::UnaryMath inst, UnaryFunction function);
    void executeInstruction(Isa::InstructionLayouts::BinaryMath inst, BinaryFunction function);
    void executeInstruction(Isa::InstructionLayouts::BinaryMathImm inst, BinaryFunction function);
    void executeInstruction(Isa::InstructionLayouts::Swizzle inst);

    Isa::Command::CommandStoreIsa isaMetadata = {};
    uint32_t isa[Isa::maxIsaSize] = {};

    struct Registers {
        VectorRegister i0;
        VectorRegister i1;
        VectorRegister i2;
        VectorRegister i3;
        VectorRegister o0;
        VectorRegister o1;
        VectorRegister o2;
        VectorRegister o3;

        uint32_t pc;

        VectorRegister r0;
        VectorRegister r1;
        VectorRegister r2;
        VectorRegister r3;
        VectorRegister r4;
        VectorRegister r5;
        VectorRegister r6;
        VectorRegister r7;
    } registers;
};
