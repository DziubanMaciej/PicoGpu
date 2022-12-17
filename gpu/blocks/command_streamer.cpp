#include "gpu/blocks/command_streamer.h"
#include "gpu/util/error.h"

void CommandStreamer::main() {
    Command command{};
    sc_time beginTime{};

    while (true) {
        wait();

        if (commands.empty() || inpGpuBusyNoCs.read()) {
            profiling.outBusy = 0;
            if (command.profilingData.outTimeTaken) {
                *command.profilingData.outTimeTaken = sc_time_stamp() - beginTime;
            }
            continue;
        }

        profiling.outBusy = 1;
        command = commands.front();
        beginTime = sc_time_stamp();

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

void CommandStreamer::draw(sc_time *outTimeTaken) {
    Command command = {};
    command.type = CommandType::Draw;
    command.profilingData.outTimeTaken = outTimeTaken;
    commands.push(command);
}

void CommandStreamer::blit(Blitter::CommandType blitType, MemoryAddressType memoryPtr, uint32_t *userPtr, size_t sizeInDwords, sc_time *outTimeTaken) {
    Command command = {};
    command.type = CommandType::Blit;
    command.blitData.blitType = blitType;
    command.blitData.memoryPtr = memoryPtr;
    command.blitData.userPtr = userPtr;
    command.blitData.sizeInDwords = sizeInDwords;
    command.profilingData.outTimeTaken = outTimeTaken;
    commands.push(command);
}

void CommandStreamer::waitForIdle() const {
    sc_start(4 * clockPeriod);
    do {
        sc_start(clockPeriod);
    } while (inpGpuBusy.read());
}
