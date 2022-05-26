#include "gpu/gpu.h"

Gpu::Gpu(sc_module_name name, uint8_t *pixels)
    : primitiveAssembler("PrimitiveAssembler"),
      rasterizer("Rasterizer", pixels) {

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
