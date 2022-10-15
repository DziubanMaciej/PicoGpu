#include "gpu/blocks/command_streamer.h"
#include "gpu/util/error.h"

void CommandStreamer::main() {
    while (true) {
        wait();

        if (commands.empty() || inpGpuBusy.read()) {
            profiling.outBusy = 0;
            continue;
        }
        profiling.outBusy = 1;

        const Command command = commands.front();
        switch (command.type) {
        case CommandType::Draw:
            paBlock.outEnable = 1;
            wait();
            paBlock.outEnable = 0;
            break;
        case CommandType::Blit:
            bltBlock.outCommandType = static_cast<size_t>(command.blitData.blitType);
            bltBlock.outMemoryPtr = command.blitData.memoryPtr;
            bltBlock.outUserPtr = reinterpret_cast<uintptr_t>(command.blitData.userPtr);
            bltBlock.outSizeInDwords = command.blitData.sizeInDwords;
            wait();
            bltBlock.outCommandType = static_cast<size_t>(Blitter::CommandType::None);
            bltBlock.outMemoryPtr = 0;
            bltBlock.outUserPtr = 0;
            bltBlock.outSizeInDwords = 0;
            break;
        default:
            FATAL_ERROR("Invalid command type: ", static_cast<int>(command.type));
        }
        commands.pop();
    }
}

void CommandStreamer::draw() {
    Command command = {};
    command.type = CommandType::Draw;
    commands.push(command);
}

void CommandStreamer::blit(Blitter::CommandType blitType, MemoryAddressType memoryPtr, uint32_t *userPtr, size_t sizeInDwords) {
    Command command = {};
    command.type = CommandType::Blit;
    command.blitData.blitType = blitType;
    command.blitData.memoryPtr = memoryPtr;
    command.blitData.userPtr = userPtr;
    command.blitData.sizeInDwords = sizeInDwords;
    commands.push(command);
}
