#include "gpu/isa/assembler/assembler.h"
#include "gpu/util/log.h"

#include <systemc.h>

const char *passingPrograms[] = {
    // Program 0 - 1 arg passthrough
    "#input i0.xyzw\n"
    "#output o0.xyzw\n"
    "mov o0 i0\n",

    // Program 1 - 4 arg passthrough, different channels
    "#input i0.xyzw\n"
    "#input i1.xyz\n"
    "#input i2.xzw\n"
    "#input i3.xyzw\n"
    "#output o0.yzw\n"
    "#output o1.xyw\n"
    "#output o2.xzw\n"
    "#output o3.z\n"
    "mov o0 i0\n"
    "mov o1 i1\n"
    "mov o2 i2\n"
    "mov o3 i3\n",

    // Program 2 - a couple instructions
    "#input i0.xw\n"
    "#input i1.yz\n"
    "#output o0.xyzw\n"
    ""
    "mov r0    i0\n"
    "mov r0.xw i1\n"
    "fadd r0 r0 r0\n"
    "mov o0 r0\n"
    "swizzle o0 o0.wzyx\n",
};

const char *failingPrograms[] = {
    // Program 0 - no instructions
    "#input i0.xyzw"
    "#output o0.xyzw",

    // Program 1 - no inputs
    "#output o0.xyzw"
    "mov o0, i0",

    // Program 2 - no outputs
    "#input i0.xyzw"
    "mov o0, i0",

    // Program 3 - swizzled source
    "#input i0.xyzw"
    "#output o0.xyzw"
    "mov o0, i1.xyz",

    // Program 4 - non-full swizzle
    "#input i0.xyzw"
    "#output o0.xyzw"
    "swizzle o0, i1.xyw",
};

int sc_main(int argc, char *argv[]) {
    bool success = true;
    int count = 0;

    for (const char *program : passingPrograms) {
        Isa::PicoGpuBinary binary = {};
        int result = Isa::assembly(program, &binary);

        const auto name = std::string("Program") + std::to_string(count++);
        if (result != 0) {
            Log() << name << "(expect pass): FAIL";
            success = false;
        } else {
            Log() << name << "(expect pass): SUCCESS";
        }
    }

    for (const char *program : failingPrograms) {
        Isa::PicoGpuBinary binary = {};
        int result = Isa::assembly(program, &binary);

        const auto name = std::string("Program") + std::to_string(count++);
        if (result == 0) {
            Log() << name << "(expect fail): FAIL";
            success = false;
        } else {
            Log() << name << "(expect fail): SUCCESS";
        }
    }

    return success ? 0 : 1;
}
