#include "gpu/isa/assembler/assembler.h"

#include <systemc.h>

int sc_main(int argc, char *argv[]) {
    const char *code = ""
                       "#input i0.xw"
                       "#input i1.yz"
                       "#output o0.xyzw"
                       ""
                       "mov r0    i0"
                       "mov r0.xw i1"
                       "add r0 r0 r0"
                       "mov o0 r0";

    Isa::PicoGpuBinary binary = {};
    int a = Isa::assembly(code, &binary);
    return a;
}
