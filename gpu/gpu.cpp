#include "gpu/gpu.h"
#include "gpu/util/vcd_trace.h"

#include <array>

std::array<sc_out<bool> *, 10> getBlocksBusySignals(Gpu *gpu) {
    return {
        &gpu->userBlitter.profiling.outBusy,
        &gpu->memoryController.profiling.outBusy,
        &gpu->shaderFrontend.profiling.outBusy,
        &gpu->shaderFrontend.profiling.outBusy,
        &gpu->shaderUnit0.profiling.outBusy,
        &gpu->shaderUnit1.profiling.outBusy,
        &gpu->primitiveAssembler.profiling.outBusy,
        &gpu->vertexShader.profiling.outBusy,
        &gpu->rasterizer.profiling.outBusy,
        &gpu->outputMerger.profiling.outBusy,
    };
}

Gpu::Gpu(sc_module_name name)
    : commandStreamer("CommandStreamer"),
      userBlitter("UserBlitter"),
      memoryController("MemoryController"),
      memory("Memory"),
      shaderFrontend("ShaderFrontend"),
      shaderUnit0("ShaderUnit0"),
      shaderUnit1("ShaderUnit1"),
      primitiveAssembler("PrimitiveAssembler"),
      vertexShader("VertexShader"),
      rasterizer("Rasterizer"),
      fragmentShader("FragmentShader"),
      outputMerger("OutputMerger") {

    connectClocks();
    connectInternalPorts();
    connectPublicPorts();
    connectProfilingPorts();

    SC_METHOD(setBusyValue);
    for (auto signal : getBlocksBusySignals(this)) {
        sensitive << *signal;
    }
}

void Gpu::connectClocks() {
    commandStreamer.inpClock(blocks.GLOBAL.inpClock);
    userBlitter.inpClock(blocks.GLOBAL.inpClock);
    memoryController.inpClock(blocks.GLOBAL.inpClock);
    memory.inpClock(blocks.GLOBAL.inpClock);
    shaderFrontend.inpClock(blocks.GLOBAL.inpClock);
    shaderUnit0.inpClock(blocks.GLOBAL.inpClock);
    shaderUnit1.inpClock(blocks.GLOBAL.inpClock);
    primitiveAssembler.inpClock(blocks.GLOBAL.inpClock);
    vertexShader.inpClock(blocks.GLOBAL.inpClock);
    rasterizer.inpClock(blocks.GLOBAL.inpClock);
    fragmentShader.inpClock(blocks.GLOBAL.inpClock);
    outputMerger.inpClock(blocks.GLOBAL.inpClock);
}

void Gpu::connectInternalPorts() {
    // CS
    ports.connectPorts(primitiveAssembler.inpEnable, commandStreamer.paBlock.outEnable, "CS_PA");
    commandStreamer.inpGpuBusy(out.busy);

    // MEM <-> MEMCTL
    ports.connectMemoryToClient(memoryController.memory, memory, "MEM_MEMCTL");

    // MEMCTL <-> clients
    sc_in<MemoryDataType> *portsForRead[] = {&userBlitter.inpData,
                                             &primitiveAssembler.memory.inpData,
                                             &outputMerger.memory.inpData,
                                             &shaderFrontend.memory.inpData};
    ports.connectPortsMultiple(portsForRead, memoryController.outData, "MEMCTL_dataForRead");
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
    ports.connectHandshake(fragmentShader.shaderFrontend.request, shaderFrontend.clientInterfaces[1].request, "SF_FS_req");
    ports.connectHandshake(shaderFrontend.clientInterfaces[1].response, fragmentShader.shaderFrontend.response, "SF_FS_resp");

    // PA <-> VS
    ports.connectHandshakeWithParallelPorts(primitiveAssembler.nextBlock, vertexShader.previousBlock, "PA_VS");

    // VS <-> RS
    ports.connectHandshakeWithParallelPorts(vertexShader.nextBlock, rasterizer.previousBlock, "VS_RS");

    // RS <-> FS
    ports.connectHandshake(rasterizer.nextBlock, fragmentShader.previousBlock, "RS_FS");

    // FS <-> OM
    ports.connectHandshake(fragmentShader.nextBlock, outputMerger.previousBlock, "FS_OM");
}

void Gpu::connectPublicPorts() {
    primitiveAssembler.inpVerticesAddress(blocks.PA.inpVerticesAddress);
    primitiveAssembler.inpVerticesCount(blocks.PA.inpVerticesCount);

    vertexShader.inpShaderAddress(blocks.VS.inpShaderAddress);

    rasterizer.framebuffer.inpWidth(blocks.RS_OM.framebufferWidth);
    rasterizer.framebuffer.inpHeight(blocks.RS_OM.framebufferHeight);

    fragmentShader.inpShaderAddress(blocks.FS.inpShaderAddress);

    outputMerger.framebuffer.inpAddress(blocks.OM.inpFramebufferAddress);
    outputMerger.depth.inpEnable(blocks.OM.inpDepthEnable);
    outputMerger.depth.inpAddress(blocks.OM.inpDepthBufferAddress);
    outputMerger.framebuffer.inpWidth(blocks.RS_OM.framebufferWidth);
    outputMerger.framebuffer.inpHeight(blocks.RS_OM.framebufferHeight);
}

void Gpu::connectProfilingPorts() {
    profilingPorts.connectPort(userBlitter.profiling.outBusy, "BLT_busy");

    profilingPorts.connectPort(memoryController.profiling.outBusy, "MEMCTL_busy");
    profilingPorts.connectPort(memoryController.profiling.outReadsPerformed, "MEMCTL_reads");
    profilingPorts.connectPort(memoryController.profiling.outWritesPerformed, "MEMCTL_writes");

    profilingPorts.connectPort(shaderFrontend.profiling.outBusy, "SF_busy");
    profilingPorts.connectPort(shaderFrontend.profiling.outIsaFetches, "SF_isaFetches");

    profilingPorts.connectPort(shaderUnit0.profiling.outBusy, "SU0_busy");
    profilingPorts.connectPort(shaderUnit0.profiling.outThreadsStarted, "SU0_threadsStarted");
    profilingPorts.connectPort(shaderUnit0.profiling.outThreadsFinished, "SU0_threadsFinished");
    profilingPorts.connectPort(shaderUnit1.profiling.outBusy, "SU1_busy");
    profilingPorts.connectPort(shaderUnit1.profiling.outThreadsStarted, "SU1_threadsStarted");
    profilingPorts.connectPort(shaderUnit1.profiling.outThreadsFinished, "SU1_threadsFinished");

    profilingPorts.connectPort(primitiveAssembler.profiling.outBusy, "PA_busy");
    profilingPorts.connectPort(primitiveAssembler.profiling.outPrimitivesProduced, "PA_primitivesProduced");

    profilingPorts.connectPort(vertexShader.profiling.outBusy, "VS_busy");

    profilingPorts.connectPort(rasterizer.profiling.outBusy, "RS_busy");
    profilingPorts.connectPort(rasterizer.profiling.outFragmentsProduced, "RS_fragmentsProduced");

    profilingPorts.connectPort(fragmentShader.profiling.outBusy, "FS_busy");

    profilingPorts.connectPort(outputMerger.profiling.outBusy, "OM_busy");
}

void Gpu::addSignalsToVcdTrace(VcdTrace &trace, bool publicPorts, bool internalPorts) {
    if (publicPorts) {
        trace.trace(blocks.GLOBAL.inpClock);

        trace.trace(blocks.PA.inpVerticesAddress);
        trace.trace(blocks.PA.inpVerticesCount);

        trace.trace(blocks.VS.inpShaderAddress);

        trace.trace(blocks.RS_OM.framebufferWidth);
        trace.trace(blocks.RS_OM.framebufferHeight);

        trace.trace(blocks.FS.inpShaderAddress);

        trace.trace(blocks.OM.inpFramebufferAddress);
        trace.trace(blocks.OM.inpDepthEnable);
        trace.trace(blocks.OM.inpDepthBufferAddress);
    }

    if (internalPorts) {
        ports.addSignalsToTrace(trace);
    }
}

void Gpu::addProfilingSignalsToVcdTrace(VcdTrace &trace) {
    profilingPorts.addSignalsToTrace(trace);
    trace.trace(blocks.GLOBAL.inpClock);
    trace.trace(out.busy);
}

void Gpu::waitForIdle(const sc_clock &clock) const {
    do {
        sc_start(2 * clock.period());
    } while (out.busy.read());
}

void Gpu::setBusyValue() {
    bool value = false;
    for (auto signal : getBlocksBusySignals(this)) {
        value = value || signal->read();
    }
    out.busy = value;
}
