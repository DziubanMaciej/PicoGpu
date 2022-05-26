#include "gpu/gpu.h"

Gpu::Gpu(sc_module_name name, uint8_t *pixels)
    : primitiveAssembler("PrimitiveAssembler"),
      rasterizer("Rasterizer", pixels) {
    primitiveAssembler.inpClock(inpPaClock);
    primitiveAssembler.inpIsNextBlockDone(rasterizerIsDone);
    primitiveAssembler.outEnableNextBlock(rasterizerIsEnabled);
    primitiveAssembler.outTriangleVertices[0](rasterizerVertices[0]);
    primitiveAssembler.outTriangleVertices[1](rasterizerVertices[1]);
    primitiveAssembler.outTriangleVertices[2](rasterizerVertices[2]);
    primitiveAssembler.outTriangleVertices[3](rasterizerVertices[3]);
    primitiveAssembler.outTriangleVertices[4](rasterizerVertices[4]);
    primitiveAssembler.outTriangleVertices[5](rasterizerVertices[5]);

    rasterizer.inpClock(inpRsClock);
    rasterizer.outIsDone(rasterizerIsDone);
    rasterizer.inpEnable(rasterizerIsEnabled);
    rasterizer.inpTriangleVertices[0](rasterizerVertices[0]);
    rasterizer.inpTriangleVertices[1](rasterizerVertices[1]);
    rasterizer.inpTriangleVertices[2](rasterizerVertices[2]);
    rasterizer.inpTriangleVertices[3](rasterizerVertices[3]);
    rasterizer.inpTriangleVertices[4](rasterizerVertices[4]);
    rasterizer.inpTriangleVertices[5](rasterizerVertices[5]);

    // TODO use this on the outside
    rasterizer.inpFramebufferWidth(rasterizeFramebufferWidth);
    rasterizer.inpFramebufferHeight(rasterizeFramebufferHeight);
    rasterizeFramebufferWidth.write(100);
    rasterizeFramebufferHeight.write(100);
}
