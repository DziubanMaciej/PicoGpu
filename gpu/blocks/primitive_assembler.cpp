#include "gpu/blocks/primitive_assembler.h"
#include "gpu/util/conversions.h"
#include "gpu/util/raii_boolean_setter.h"
#include "gpu/util/transfer.h"

void PrimitiveAssembler::assemble() {
    const size_t maxInputComponents = verticesInPrimitive * Isa::maxInputOutputRegisters * Isa::registerComponentsCount;
    uint32_t readVertices[maxInputComponents];

    while (1) {
        wait();
        if (!inpEnable) {
            continue;
        }

        RaiiBooleanSetter busySetter{profiling.outBusy};

        const uint32_t vertexBufferAddress = inpVerticesAddress.read().to_int();
        const auto primitiveCount = inpVerticesCount.read().to_int() / verticesInPrimitive;
        const CustomShaderComponents componentsInfo{this->inpCustomInputComponents.read().to_uint()};
        const size_t componentsToTransfer = verticesInPrimitive * componentsInfo.getTotalCustomComponents();

        for (int triangleIndex = 0; triangleIndex < primitiveCount; triangleIndex++) {
            if (triangleIndex != 0) {
                wait();
            }

            const uint32_t triangleAddress = vertexBufferAddress + triangleIndex * componentsToTransfer * sizeof(uint32_t);
            for (size_t componentIndex = 0; componentIndex < componentsToTransfer; componentIndex++) {
                readVertices[componentIndex] = fetchComponentFromMemory(triangleAddress + componentIndex * sizeof(uint32_t));
            }

            // Output the triangle to the next block
            Transfer::sendArrayWithParallelPorts(nextBlock.inpReceiving, nextBlock.outSending, nextBlock.outData, readVertices, componentsToTransfer);
            profiling.outPrimitivesProduced = profiling.outPrimitivesProduced.read() + 1;
        }
    }
}

uint32_t PrimitiveAssembler::fetchComponentFromMemory(uint32_t address) {
    memory.outEnable = 1;
    memory.outAddress = address;
    wait(1);
    memory.outEnable = 0;

    while (!memory.inpCompleted) {
        wait(1);
    }

    memory.outAddress = 0;
    return memory.inpData.read();
}
