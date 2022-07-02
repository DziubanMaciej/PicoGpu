#include "gpu/gpu.h"
#include "gpu/util/vcd_trace.h"

Gpu::Gpu(sc_module_name name)
    : userBlitter("UserBlitter"),
      memoryController("MemoryController"),
      memory("Memory"),
      shaderFrontend("ShaderFrontend"),
      shaderUnit0("ShaderUnit0"),
      shaderUnit1("ShaderUnit1"),
      primitiveAssembler("PrimitiveAssembler"),
      vertexShader("VertexShader"),
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
    shaderFrontend.inpClock(blocks.GLOBAL.inpClock);
    shaderUnit0.inpClock(blocks.GLOBAL.inpClock);
    shaderUnit1.inpClock(blocks.GLOBAL.inpClock);
    primitiveAssembler.inpClock(blocks.GLOBAL.inpClock);
    vertexShader.inpClock(blocks.GLOBAL.inpClock);
    rasterizer.inpClock(blocks.GLOBAL.inpClock);
    outputMerger.inpClock(blocks.GLOBAL.inpClock);
}

void Gpu::connectInternalPorts() {
    // MEM <-> MEMCTL
    ports.connectMemoryToClient(memoryController.memory, memory, "MEM_MEMCTL");

    // MEMCTL <-> clients
    sc_in<MemoryDataType> *portsForRead[] = {&userBlitter.inpData,
                                             &primitiveAssembler.memory.inpData,
                                             &outputMerger.memory.inpData,
                                             &shaderFrontend.memory.inpData};
    ports.connectPortsA(portsForRead, memoryController.outData, "MEMCTL_dataForRead");
    ports.connectMemoryToClient<MemoryClientType::ReadOnly, MemoryServerType::SeparateOutData>(primitiveAssembler.memory, memoryController.clients[0], "MEMCTL_BLT");
    ports.connectMemoryToClient<MemoryClientType::ReadWrite, MemoryServerType::SeparateOutData>(userBlitter, memoryController.clients[1], "MEMCTL_PA");
    ports.connectMemoryToClient<MemoryClientType::ReadWrite, MemoryServerType::SeparateOutData>(outputMerger.memory, memoryController.clients[2], "MEMCTL_OM");
    ports.connectMemoryToClient<MemoryClientType::ReadOnly, MemoryServerType::SeparateOutData>(shaderFrontend.memory, memoryController.clients[3], "MEMCTL_SF");

    // SF -> SU
    ports.connectHandshake(shaderFrontend.shaderUnitInterfaces[0].request, shaderUnit0.request, "SF_SU0_req");
    ports.connectHandshake(shaderFrontend.shaderUnitInterfaces[1].request, shaderUnit1.request, "SF_SU1_req");
    ports.connectHandshake(shaderUnit0.response, shaderFrontend.shaderUnitInterfaces[0].response, "SF_SU0_resp");
    ports.connectHandshake(shaderUnit1.response, shaderFrontend.shaderUnitInterfaces[1].response, "SF_SU1_resp");

    // SF -> clients
    ports.connectHandshake(vertexShader.shaderFrontend.request, shaderFrontend.clientInterfaces[0].request, "SF_VS_req");
    ports.connectHandshake(shaderFrontend.clientInterfaces[0].response, vertexShader.shaderFrontend.response, "SF_VS_resp");

    // PA <-> VS
    ports.connectHandshakeWithParallelPorts(primitiveAssembler.nextBlock, vertexShader.previousBlock, "PA_VS");

    // VS <-> RS
    ports.connectHandshakeWithParallelPorts(vertexShader.nextBlock, rasterizer.previousBlock, "VS_RS");

    // RS <-> OM
    ports.connectHandshake(rasterizer.nextBlock, outputMerger.previousBlock, "RS_OM");
}

void Gpu::connectPublicPorts() {
    primitiveAssembler.inpEnable(blocks.PA.inpEnable);
    primitiveAssembler.inpVerticesAddress(blocks.PA.inpVerticesAddress);
    primitiveAssembler.inpVerticesCount(blocks.PA.inpVerticesCount);

    vertexShader.inpShaderAddress(blocks.VS.inpShaderAddress);

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

        trace.trace(blocks.VS.inpShaderAddress);

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
