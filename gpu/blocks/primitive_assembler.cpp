#include "gpu/blocks/primitive_assembler.h"
#include "gpu/util/conversions.h"
#include "gpu/util/handshake.h"
#include "gpu/util/raii_boolean_setter.h"

void PrimitiveAssembler::assemble() {
    while (1) {
        wait();

        if (!inpEnable) {
            continue;
        }
        RaiiBooleanSetter busySetter{profiling.outBusy};

        const auto verticesAddress = inpVerticesAddress.read().to_int();
        const auto verticesInPrimitive = 3; // only triangles
        const auto primitiveCount = inpVerticesCount.read().to_int() / verticesInPrimitive;
        const auto componentsPerVertex = 3; // x, y, z

        uint32_t readVertices[verticesInPrimitive * componentsPerVertex] = {};

        for (int triangleIndex = 0; triangleIndex < primitiveCount; triangleIndex++) {
            if (triangleIndex != 0) {
                wait();
            }

            // Read all vertices of the triangle to local memory
            for (int vertexIndex = 0; vertexIndex < verticesInPrimitive; vertexIndex++) {
                for (int componentIndex = 0; componentIndex < componentsPerVertex; componentIndex++) {
                    memory.outEnable = 1;
                    memory.outAddress = verticesAddress + sizeof(uint32_t) * (triangleIndex * verticesInPrimitive * componentsPerVertex + componentsPerVertex * vertexIndex + componentIndex);
                    wait(1);
                    memory.outEnable = 0;

                    while (!memory.inpCompleted) {
                        wait(1);
                    }

                    memory.outAddress = 0;
                    readVertices[vertexIndex * componentsPerVertex + componentIndex] = memory.inpData.read();
                }
            }

            // Output the triangle to the next block
            Handshake::sendArrayWithParallelPorts(nextBlock.inpReceiving, nextBlock.outSending,
                                                  nextBlock.outData, nextBlock.portsCount,
                                                  readVertices, verticesInPrimitive * componentsPerVertex);
            profiling.outPrimitivesProduced = profiling.outPrimitivesProduced.read() + 1;
        }
    }
}