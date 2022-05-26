#include "gpu/blocks/primitive_assembler.h"

void PrimitiveAssembler::assemble() {
    worked = false;
    while (1) {
        wait();
        if (worked) {
            continue;
        }

        outTriangleVertices[0].write(10);
        outTriangleVertices[1].write(10);
        outTriangleVertices[2].write(20);
        outTriangleVertices[3].write(10);
        outTriangleVertices[4].write(10);
        outTriangleVertices[5].write(20);
        outEnableNextBlock.write(1);
        do {
            wait();
        } while (!inpIsNextBlockDone.read());
        outEnableNextBlock.write(0);
    }
}