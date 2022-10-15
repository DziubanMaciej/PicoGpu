#include "gpu/blocks/command_streamer.h"
#include "gpu/util/error.h"

void CommandStreamer::main() {
    while (true) {
        wait();

        if (inpGpuBusy.read()) {
            continue;
        }
        if (commands.empty()) {
            continue;
        }

        const Command command = commands.front();
        switch (command.type) {
        case CommandType::Draw:
            paBlock.outEnable = 1;
            wait();
            paBlock.outEnable = 0;
            break;
        case CommandType::Blit:
            // TODO
        default:
            FATAL_ERROR("Invalid command type");
        }
        commands.pop();
    }
}

void CommandStreamer::draw() {
    Command command = {};
    command.type = CommandType::Draw;
    commands.push(command);
}
