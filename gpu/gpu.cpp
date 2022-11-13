#include "gpu/gpu.h"
#include "gpu/util/vcd_trace.h"

#include <array>

std::array<sc_out<bool> *, 10> getBlocksBusySignals(Gpu *gpu) {
    return {
        &gpu->blitter.profiling.outBusy,
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
      blitter("Blitter"),
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
    SC_CTHREAD(setBusyValue, blocks.GLOBAL.inpClock.pos());
}

void Gpu::connectClocks() {
    commandStreamer.inpClock(blocks.GLOBAL.inpClock);
    blitter.inpClock(blocks.GLOBAL.inpClock);
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
    ports.connectPorts(blitter.command.inpCommandType, commandStreamer.bltBlock.outCommandType, "CS_BLT_commandType");
    ports.connectPorts(blitter.command.inpMemoryPtr, commandStreamer.bltBlock.outMemoryPtr, "CS_BLT_memoryPtr");
    ports.connectPorts(blitter.command.inpUserPtr, commandStreamer.bltBlock.outUserPtr, "CS_BLT_userPtr");
    ports.connectPorts(blitter.command.inpSizeInDwords, commandStreamer.bltBlock.outSizeInDwords, "CS_BLT_size");
    commandStreamer.inpGpuBusy(out.busyNoCs);

    // MEM <-> MEMCTL
    ports.connectMemoryToClient(memoryController.memory, memory, "MEM_MEMCTL");

    // MEMCTL <-> clients
    sc_in<MemoryDataType> *portsForRead[] = {&blitter.memory.inpData,
                                             &primitiveAssembler.memory.inpData,
                                             &outputMerger.memory.inpData,
                                             &shaderFrontend.memory.inpData};
    ports.connectPortsMultiple(portsForRead, memoryController.outData, "MEMCTL_dataForRead");
    ports.connectMemoryToClient<MemoryClientType::ReadOnly, MemoryServerType::SeparateOutData>(primitiveAssembler.memory, memoryController.clients[0], "MEMCTL_PA");
    ports.connectMemoryToClient<MemoryClientType::ReadWrite, MemoryServerType::SeparateOutData>(blitter.memory, memoryController.clients[1], "MEMCTL_BLT");
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
    ports.connectHandshakeWithParallelPorts(rasterizer.nextBlock.perTriangle, fragmentShader.previousBlock.perTriangle, "RS_FS_tri");
    ports.connectHandshake(rasterizer.nextBlock.perFragment, fragmentShader.previousBlock.perFragment, "RS_FS_frag");

    // FS <-> OM
    ports.connectHandshake(fragmentShader.nextBlock, outputMerger.previousBlock, "FS_OM");
}

void Gpu::connectPublicPorts() {
    primitiveAssembler.inpVerticesAddress(blocks.PA.inpVerticesAddress);
    primitiveAssembler.inpVerticesCount(blocks.PA.inpVerticesCount);

    vertexShader.inpShaderAddress(blocks.VS.inpShaderAddress);
    vertexShader.inpCustomOutputComponents(blocks.GLOBAL.inpVsPsCustomComponents);

    rasterizer.inpCustomVsPsComponents(blocks.GLOBAL.inpVsPsCustomComponents);
    rasterizer.framebuffer.inpWidth(blocks.RS_OM.framebufferWidth);
    rasterizer.framebuffer.inpHeight(blocks.RS_OM.framebufferHeight);

    fragmentShader.inpCustomInputComponents(blocks.GLOBAL.inpVsPsCustomComponents);
    fragmentShader.inpShaderAddress(blocks.FS.inpShaderAddress);

    outputMerger.framebuffer.inpAddress(blocks.OM.inpFramebufferAddress);
    outputMerger.depth.inpEnable(blocks.OM.inpDepthEnable);
    outputMerger.depth.inpAddress(blocks.OM.inpDepthBufferAddress);
    outputMerger.framebuffer.inpWidth(blocks.RS_OM.framebufferWidth);
    outputMerger.framebuffer.inpHeight(blocks.RS_OM.framebufferHeight);
}

void Gpu::connectProfilingPorts() {
    profilingPorts.connectPort(commandStreamer.profiling.outBusy, "CS_busy");

    profilingPorts.connectPort(blitter.profiling.outBusy, "BLT_busy");

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
    trace.trace(out.busyNoCs);
    trace.trace(out.busy);
}

void Gpu::waitForIdle(const sc_clock &clock) const {
    sc_start(4 * clock.period());
    do {
        sc_start(clock.period());
    } while (out.busy.read());
}

void Gpu::setBusyValue() {
    // These fields hold history for the previous busy values. Least significant bit signifies
    // the current value, bit 1 signifies value for 1 cycle earlier, etc.
    uint32_t busy = 0;
    uint32_t busyNoCs = 0;

    while (true) {
        // Shift bits to the left indicating next timestep
        busy <<= 1;
        busyNoCs <<= 1;

        // Calculate new current values
        for (auto signal : getBlocksBusySignals(this)) {
            busy |= signal->read();
        }
        busyNoCs |= ((busy & 1) | commandStreamer.profiling.outBusy.read());

        // Store smoothed values into the output signals
        out.busy = (busyNoCs & 7);     // smoothed over 3 cycles
        out.busyNoCs = (busyNoCs & 1); // no smoothing

        wait();
    }
}
