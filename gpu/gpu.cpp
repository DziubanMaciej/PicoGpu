#include "gpu/gpu.h"
#include "gpu/util/vcd_trace.h"

Gpu::Gpu(sc_module_name name)
    : userBlitter("UserBlitter"),
      memoryController("MemoryController"),
      memory("Memory"),
      primitiveAssembler("PrimitiveAssembler"),
      rasterizer("Rasterizer"),
      outputMerger("OutputMerger") {

    // Initialize blitter
    userBlitter.inpClock(blocks.BLT.inpClock);
    userBlitter.outEnable(internalSignals.BLT_MEMCTL.enable);
    userBlitter.outWrite(internalSignals.BLT_MEMCTL.write);
    userBlitter.outAddress(internalSignals.BLT_MEMCTL.address);
    userBlitter.outData(internalSignals.BLT_MEMCTL.dataForWrite);
    userBlitter.inpData(internalSignals.MEMCTL.dataForRead);
    userBlitter.inpCompleted(internalSignals.BLT_MEMCTL.completed);

    // Initialize memory controller
    memoryController.inpClock(blocks.MEMCTL.inpClock);
    memoryController.clients[0].inpEnable(internalSignals.BLT_MEMCTL.enable);
    memoryController.clients[0].inpWrite(internalSignals.BLT_MEMCTL.write);
    memoryController.clients[0].inpAddress(internalSignals.BLT_MEMCTL.address);
    memoryController.clients[0].inpData(internalSignals.BLT_MEMCTL.dataForWrite);
    memoryController.clients[0].outCompleted(internalSignals.BLT_MEMCTL.completed);
    memoryController.clients[1].inpEnable(internalSignals.MEMCTL_PA.enable);
    memoryController.clients[1].inpWrite(internalSignals.MEMCTL_PA.write);
    memoryController.clients[1].inpAddress(internalSignals.MEMCTL_PA.address);
    memoryController.clients[1].inpData(internalSignals.MEMCTL_PA.dataForWrite);
    memoryController.clients[1].outCompleted(internalSignals.MEMCTL_PA.completed);
    memoryController.clients[2].inpEnable(internalSignals.MEMCTL_OM.enable);
    memoryController.clients[2].inpWrite(internalSignals.MEMCTL_OM.write);
    memoryController.clients[2].inpAddress(internalSignals.MEMCTL_OM.address);
    memoryController.clients[2].inpData(internalSignals.MEMCTL_OM.dataForWrite);
    memoryController.clients[2].outCompleted(internalSignals.MEMCTL_OM.completed);
    memoryController.outData(internalSignals.MEMCTL.dataForRead);
    memoryController.memory.outEnable(internalSignals.MEMCTL_MEM.enable);
    memoryController.memory.outWrite(internalSignals.MEMCTL_MEM.write);
    memoryController.memory.outAddress(internalSignals.MEMCTL_MEM.address);
    memoryController.memory.outData(internalSignals.MEMCTL_MEM.dataForWrite);
    memoryController.memory.inpData(internalSignals.MEMCTL_MEM.dataForRead);
    memoryController.memory.inpCompleted(internalSignals.MEMCTL_MEM.completed);

    // Initialize memory
    memory.inpClock(blocks.MEM.inpClock);
    memory.inpEnable(internalSignals.MEMCTL_MEM.enable);
    memory.inpWrite(internalSignals.MEMCTL_MEM.write);
    memory.inpAddress(internalSignals.MEMCTL_MEM.address);
    memory.inpData(internalSignals.MEMCTL_MEM.dataForWrite);
    memory.outData(internalSignals.MEMCTL_MEM.dataForRead);
    memory.outCompleted(internalSignals.MEMCTL_MEM.completed);

    // Initialize primitive assembler
    primitiveAssembler.inpClock(blocks.PA.inpClock);
    primitiveAssembler.inpEnable(blocks.PA.inpEnable);
    primitiveAssembler.inpVerticesAddress(blocks.PA.inpVerticesAddress);
    primitiveAssembler.inpVerticesCount(blocks.PA.inpVerticesCount);
    primitiveAssembler.memory.outEnable(internalSignals.MEMCTL_PA.enable);
    primitiveAssembler.memory.outAddress(internalSignals.MEMCTL_PA.address);
    primitiveAssembler.memory.inpData(internalSignals.MEMCTL.dataForRead);
    primitiveAssembler.memory.inpCompleted(internalSignals.MEMCTL_PA.completed);
    primitiveAssembler.nextBlock.inpIsDone(internalSignals.PA_RS.isDone);
    primitiveAssembler.nextBlock.outEnable(internalSignals.PA_RS.isEnabled);
    for (int i = 0; i < 6; i++) {
        primitiveAssembler.nextBlock.outTriangleVertices[i](internalSignals.PA_RS.vertices[i]);
    }

    // Initialize rasterizer
    rasterizer.inpClock(blocks.RS.inpClock);
    rasterizer.framebuffer.inpWidth(blocks.RS_OM.framebufferWidth);
    rasterizer.framebuffer.inpHeight(blocks.RS_OM.framebufferHeight);
    for (int i = 0; i < 6; i++) {
        rasterizer.previousBlock.inpTriangleVertices[i](internalSignals.PA_RS.vertices[i]);
    }
    rasterizer.previousBlock.inpEnable(internalSignals.PA_RS.isEnabled);
    rasterizer.previousBlock.outIsDone(internalSignals.PA_RS.isDone);
    rasterizer.nextBlock.outEnable(internalSignals.RS_OM.enable);
    rasterizer.nextBlock.outPixels(internalSignals.RS_OM.pixels);
    rasterizer.nextBlock.inpIsDone(internalSignals.RS_OM.isDone);

    // Initialize output merger
    outputMerger.inpClock(blocks.OM.inpClock);
    outputMerger.previousBlock.inpEnable(internalSignals.RS_OM.enable);
    outputMerger.previousBlock.inpPixels(internalSignals.RS_OM.pixels);
    outputMerger.previousBlock.outIsDone(internalSignals.RS_OM.isDone);
    outputMerger.framebuffer.inpAddress(blocks.OM.inpFramebufferAddress);
    outputMerger.framebuffer.inpWidth(blocks.RS_OM.framebufferWidth);
    outputMerger.framebuffer.inpHeight(blocks.RS_OM.framebufferHeight);
    outputMerger.memory.outEnable(internalSignals.MEMCTL_OM.enable);
    outputMerger.memory.outAddress(internalSignals.MEMCTL_OM.address);
    outputMerger.memory.outData(internalSignals.MEMCTL_OM.dataForWrite);
    outputMerger.memory.inpCompleted(internalSignals.MEMCTL_OM.completed);
}

void Gpu::addSignalsToVcdTrace(VcdTrace &trace, bool allClocksTheSame, bool publicPorts, bool internalPorts) {
    if (publicPorts) {
        trace.trace(blocks.BLT.inpClock);

        if (!allClocksTheSame) {
            trace.trace(blocks.MEMCTL.inpClock);
        }

        if (!allClocksTheSame) {
            trace.trace(blocks.MEM.inpClock);
        }

        if (!allClocksTheSame) {
            trace.trace(blocks.PA.inpClock);
        }
        trace.trace(blocks.PA.inpEnable);
        trace.trace(blocks.PA.inpVerticesAddress);
        trace.trace(blocks.PA.inpVerticesCount);

        if (!allClocksTheSame) {
            trace.trace(blocks.RS.inpClock);
        }

        trace.trace(blocks.RS_OM.framebufferWidth);
        trace.trace(blocks.RS_OM.framebufferHeight);

        if (!allClocksTheSame) {
            trace.trace(blocks.OM.inpClock);
        }
        trace.trace(blocks.OM.inpFramebufferAddress);
    }

    if (internalPorts) {
        trace.trace(internalSignals.BLT_MEMCTL.enable);
        trace.trace(internalSignals.BLT_MEMCTL.write);
        trace.trace(internalSignals.BLT_MEMCTL.address);
        trace.trace(internalSignals.BLT_MEMCTL.dataForWrite);
        trace.trace(internalSignals.BLT_MEMCTL.completed);

        trace.trace(internalSignals.MEMCTL.dataForRead);

        trace.trace(internalSignals.MEMCTL_MEM.enable);
        trace.trace(internalSignals.MEMCTL_MEM.write);
        trace.trace(internalSignals.MEMCTL_MEM.address);
        trace.trace(internalSignals.MEMCTL_MEM.dataForRead);
        trace.trace(internalSignals.MEMCTL_MEM.dataForWrite);
        trace.trace(internalSignals.MEMCTL_MEM.completed);

        trace.trace(internalSignals.MEMCTL_PA.enable);
        trace.trace(internalSignals.MEMCTL_PA.write);
        trace.trace(internalSignals.MEMCTL_PA.address);
        trace.trace(internalSignals.MEMCTL_PA.dataForWrite);
        trace.trace(internalSignals.MEMCTL_PA.completed);

        trace.trace(internalSignals.MEMCTL_OM.enable);
        trace.trace(internalSignals.MEMCTL_OM.write);
        trace.trace(internalSignals.MEMCTL_OM.address);
        trace.trace(internalSignals.MEMCTL_OM.dataForWrite);
        trace.trace(internalSignals.MEMCTL_OM.completed);

        trace.trace(internalSignals.PA_RS.isEnabled);
        trace.trace(internalSignals.PA_RS.isDone);
        for (int i = 0; i < 6; i++) {
            trace.trace(internalSignals.PA_RS.vertices[i]);
        }

        trace.trace(internalSignals.RS_OM.enable);
        // trace.trace(internalSignals.RS_OM.pixels); // TODO is it possible to work?
        trace.trace(internalSignals.RS_OM.isDone);
    }
}
