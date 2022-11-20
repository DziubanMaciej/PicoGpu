#pragma once

#include "gpu/blocks/blitter.h"
#include "gpu/definitions/types.h"

#include <queue>
#include <systemc.h>

SC_MODULE(CommandStreamer) {
    sc_in_clk inpClock;
    sc_in<bool> inpGpuBusy;

    struct PrimitiveAssemblerBlock {
        sc_out<bool> outEnable;
    } paBlock;

    struct {
        sc_out<sc_uint<2>> outCommandType;
        sc_out<MemoryAddressType> outMemoryPtr;
        sc_out<PointerType> outUserPtr;
        sc_out<sc_uint<16>> outSizeInDwords;
    } bltBlock;

    struct {
        sc_out<bool> outBusy;
    } profiling;

    SC_CTOR(CommandStreamer) {
        SC_CTHREAD(main, inpClock.pos());
    }

    void draw(sc_time * outTimeTaken);
    void blit(Blitter::CommandType blitType, MemoryAddressType memoryPtr, uint32_t * userPtr, size_t sizeInDwords, sc_time * outTimeTaken);
    void blitToMemory(MemoryAddressType memoryPtr, uint32_t * userPtr, size_t sizeInDwords, sc_time * outTimeTaken) { blit(Blitter::CommandType::CopyToMem, memoryPtr, userPtr, sizeInDwords, outTimeTaken); }
    void blitFromMemory(MemoryAddressType memoryPtr, uint32_t * userPtr, size_t sizeInDwords, sc_time * outTimeTaken) { blit(Blitter::CommandType::CopyFromMem, memoryPtr, userPtr, sizeInDwords, outTimeTaken); }
    void fillMemory(MemoryAddressType memoryPtr, uint32_t * userPtr, size_t sizeInDwords, sc_time * outTimeTaken) { blit(Blitter::CommandType::FillMem, memoryPtr, userPtr, sizeInDwords, outTimeTaken); }

private:
    void main();

    enum class CommandType {
        Draw,
        Blit,
    };
    struct BlitData {
        Blitter::CommandType blitType;
        MemoryAddressType memoryPtr;
        uint32_t *userPtr;
        size_t sizeInDwords;
    };
    struct ProfilingData {
        sc_time *outTimeTaken;
    };
    struct Command {
        CommandType type;
        BlitData blitData;
        ProfilingData profilingData;
    };

    std::queue<Command> commands;
};
