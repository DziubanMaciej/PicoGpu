#pragma once

#include "gpu/types.h"

#include <queue>
#include <systemc.h>

SC_MODULE(CommandStreamer) {
    sc_in_clk inpClock;
    sc_in<bool> inpGpuBusy;

    struct PrimitiveAssemblerBlock {
        sc_out<bool> outEnable;
    } paBlock;

    SC_CTOR(CommandStreamer) {
        SC_CTHREAD(main, inpClock.pos());
    }

    void draw();

private:
    void main();

    enum class CommandType {
        Draw,
        Blit, // Currently unused
    };
    struct Command {
        CommandType type;
    };

    std::queue<Command> commands;
};
