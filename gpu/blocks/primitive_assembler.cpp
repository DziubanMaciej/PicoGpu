#include "gpu/blocks/primitive_assembler.h"

void PrimitiveAssembler::assemble() {
    while (1) {
        wait();

        nextBlock.outEnable = 0;

        if (!inpEnable) {
            continue;
        }

        const auto verticesAddress = inpVerticesAddress.read().to_int();
        const auto trianglesCount = inpVerticesCount.read().to_int() / 3;
        const auto componentsPerVertex = 2; // only x and y

        uint32_t readVertices[3][componentsPerVertex] = {};

        for (int triangleIndex = 0; triangleIndex < trianglesCount; triangleIndex++) {
            if (triangleIndex != 0) {
                wait();
            }
            nextBlock.outEnable = 0;

            // Read all vertices of the triangle to local memory
            for (int vertexIndex = 0; vertexIndex < 3; vertexIndex++) {
                for (int componentIndex = 0; componentIndex < componentsPerVertex; componentIndex++) {
                    memory.outEnable = 1;
                    memory.outAddress = verticesAddress + sizeof(uint32_t) * (triangleIndex * 3 * componentsPerVertex + componentsPerVertex * vertexIndex + componentIndex);
                    wait(1);
                    memory.outEnable = 0;

                    while (!memory.inpCompleted) {
                        wait(1);
                    }

                    memory.outAddress = 0;
                    readVertices[vertexIndex][componentIndex] = memory.inpData.read();
                }
            }

            // Output the triangle to the nexr block
            while (!nextBlock.inpIsDone) {
                wait(1);
            }
            for (int vertexIndex = 0; vertexIndex < 3; vertexIndex++) {
                for (int componentIndex = 0; componentIndex < componentsPerVertex; componentIndex++) {
                    nextBlock.outTriangleVertices[vertexIndex * componentsPerVertex + componentIndex] = readVertices[vertexIndex][componentIndex];
                }
            }
            nextBlock.outEnable = 1;
        }
    }
}