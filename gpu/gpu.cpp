#include "gpu/gpu.h"

Gpu::Gpu(sc_module_name name, uint8_t *pixels)
    : userBlitter("UserBlitter"),
      memoryController("MemoryController"),
      memory("Memory"),
      primitiveAssembler("PrimitiveAssembler"),
      rasterizer("Rasterizer", pixels) {

    // Initialize blitter
    userBlitter.inpClock(blocks.BLT.inpClock);
    userBlitter.outEnable(internalSignals.BLT_MEMCTL.enable);
    userBlitter.outWrite(internalSignals.BLT_MEMCTL.write);
    userBlitter.outAddress(internalSignals.BLT_MEMCTL.address);
    userBlitter.outData(internalSignals.BLT_MEMCTL.dataForWrite);
    userBlitter.inpData(internalSignals.MEMCTL.dataForRead);
    userBlitter.inpCompleted(internalSignals.BLT_MEMCTL.enable);

    // Initialize memory controller
    memoryController.inpClock(blocks.MEMCTL.inpClock);
    memoryController.clients[0].inpEnable(internalSignals.BLT_MEMCTL.enable);
    memoryController.clients[0].inpWrite(internalSignals.BLT_MEMCTL.write);
    memoryController.clients[0].inpAddress(internalSignals.BLT_MEMCTL.address);
    memoryController.clients[0].inpData(internalSignals.BLT_MEMCTL.dataForWrite);
    memoryController.clients[0].outCompleted(internalSignals.BLT_MEMCTL.completed);
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
    primitiveAssembler.inpIsNextBlockDone(internalSignals.PA_RS.isDone);
    primitiveAssembler.outEnableNextBlock(internalSignals.PA_RS.isEnabled);
    for (int i = 0; i < 6; i++) {
        primitiveAssembler.outTriangleVertices[i](internalSignals.PA_RS.vertices[i]);
    }

    // Initialize rasterizer
    rasterizer.inpClock(blocks.RS.inpClock);
    rasterizer.outIsDone(internalSignals.PA_RS.isDone);
    rasterizer.inpEnable(internalSignals.PA_RS.isEnabled);
    for (int i = 0; i < 6; i++) {
        rasterizer.inpTriangleVertices[i](internalSignals.PA_RS.vertices[i]);
    }
    rasterizer.inpFramebufferWidth(blocks.RS.framebufferWidth);
    rasterizer.inpFramebufferHeight(blocks.RS.framebufferHeight);
}
