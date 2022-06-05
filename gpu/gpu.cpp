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
    memoryController.clients[1].inpEnable(internalSignals.MEMCTL_PA.enable);
    memoryController.clients[1].inpWrite(internalSignals.MEMCTL_PA.write);
    memoryController.clients[1].inpAddress(internalSignals.MEMCTL_PA.address);
    memoryController.clients[1].inpData(internalSignals.MEMCTL_PA.dataForWrite);
    memoryController.clients[1].outCompleted(internalSignals.MEMCTL_PA.completed);
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
    primitiveAssembler.memory.inpCompleted(internalSignals.MEMCTL_PA.enable);
    primitiveAssembler.nextBlock.inpIsDone(internalSignals.PA_RS.isDone);
    primitiveAssembler.nextBlock.outEnable(internalSignals.PA_RS.isEnabled);
    for (int i = 0; i < 6; i++) {
        primitiveAssembler.nextBlock.outTriangleVertices[i](internalSignals.PA_RS.vertices[i]);
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
