#include "gpu/isa/assembler/assembler.h"

#include <systemc.h>

int sc_main(int argc, char *argv[]) {
    Isa::PicoGpuBinary binary = {};
    int a = Isa::assembly(&binary);
  //  printf("Binary size %d\n", binary.vec.size());
    return a;
}

