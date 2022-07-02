#include "gpu/gpu.h"
#include "gpu/util/vcd_trace.h"

Gpu::Gpu(sc_module_name name)
    : userBlitter("UserBlitter"),
      memoryController("MemoryController"),
      memory("Memory"),
      primitiveAssembler("PrimitiveAssembler"),
      rasterizer("Rasterizer"),
      outputMerger("OutputMerger") {

    connectClocks();
    connectInternalPorts();
    connectPublicPorts();
}

void Gpu::connectClocks() {
    userBlitter.inpClock(blocks.GLOBAL.inpClock);
    memoryController.inpClock(blocks.GLOBAL.inpClock);
    memory.inpClock(blocks.GLOBAL.inpClock);
    primitiveAssembler.inpClock(blocks.GLOBAL.inpClock);
    rasterizer.inpClock(blocks.GLOBAL.inpClock);
    outputMerger.inpClock(blocks.GLOBAL.inpClock);
}

void Gpu::connectInternalPorts() {
    // MEM <-> MEMCTL
    ports.connectMemoryToClient(memoryController.memory, memory, "MEM_MEMCTL");

    // MEMCTL <-> clients
    sc_in<MemoryDataType> *portsForRead[] = {&userBlitter.inpData,
                                             &primitiveAssembler.memory.inpData,
                                             &outputMerger.memory.inpData};
    ports.connectPortsA(portsForRead, memoryController.outData, "MEMCTL_dataForRead");
    ports.connectMemoryToClient<MemoryClientType::ReadOnly, MemoryServerType::SeparateOutData>(primitiveAssembler.memory, memoryController.clients[0], "MEMCTL_BLT");
    ports.connectMemoryToClient<MemoryClientType::ReadWrite, MemoryServerType::SeparateOutData>(userBlitter, memoryController.clients[1], "MEMCTL_PA");
    ports.connectMemoryToClient<MemoryClientType::ReadWrite, MemoryServerType::SeparateOutData>(outputMerger.memory, memoryController.clients[2], "MEMCTL_OM");

    // PA <-> RS
    ports.connectHandshakeWithParallelPorts(primitiveAssembler.nextBlock, rasterizer.previousBlock, "PA_RS");

    // RS <-> OM
    ports.connectHandshake(rasterizer.nextBlock, outputMerger.previousBlock, "RS_OM");
}

void Gpu::connectPublicPorts() {
    primitiveAssembler.inpEnable(blocks.PA.inpEnable);
    primitiveAssembler.inpVerticesAddress(blocks.PA.inpVerticesAddress);
    primitiveAssembler.inpVerticesCount(blocks.PA.inpVerticesCount);

    rasterizer.framebuffer.inpWidth(blocks.RS_OM.framebufferWidth);
    rasterizer.framebuffer.inpHeight(blocks.RS_OM.framebufferHeight);

    outputMerger.framebuffer.inpAddress(blocks.OM.inpFramebufferAddress);
    outputMerger.depth.inpEnable(blocks.OM.inpDepthEnable);
    outputMerger.depth.inpAddress(blocks.OM.inpDepthBufferAddress);
    outputMerger.framebuffer.inpWidth(blocks.RS_OM.framebufferWidth);
    outputMerger.framebuffer.inpHeight(blocks.RS_OM.framebufferHeight);
}

void Gpu::addSignalsToVcdTrace(VcdTrace &trace, bool publicPorts, bool internalPorts) {
    if (publicPorts) {
        trace.trace(blocks.GLOBAL.inpClock);

        trace.trace(blocks.PA.inpEnable);
        trace.trace(blocks.PA.inpVerticesAddress);
        trace.trace(blocks.PA.inpVerticesCount);

        trace.trace(blocks.RS_OM.framebufferWidth);
        trace.trace(blocks.RS_OM.framebufferHeight);

        trace.trace(blocks.OM.inpFramebufferAddress);
        trace.trace(blocks.OM.inpDepthEnable);
        trace.trace(blocks.OM.inpDepthBufferAddress);
    }

    if (internalPorts) {
        ports.addSignalsToTrace(trace);
    }
}
