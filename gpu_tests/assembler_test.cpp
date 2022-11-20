#include "gpu/isa/assembler/assembler.h"
#include "gpu/util/log.h"

#include <systemc.h>

const char *passingPrograms[] = {
    // Program 0 - 1 arg passthrough
    "#vertexShader\n"
    "#input r0.xyzw\n"
    "#output r12.xyzw\n"
    "mov r12 r0\n",

    // Program 1 - 4 arg passthrough, different channels
    "#vertexShader\n"
    "#input r0.xyzw\n"
    "#input r1.xyz\n"
    "#input r2.xyz\n"
    "#input r3.xyzw\n"
    "#output r12.xyzw\n"
    "#output r13.xyz\n"
    "#output r14.xyz\n"
    "#output r15.xy\n"
    "mov r12 r0\n"
    "mov r13 r1\n"
    "mov r14 r2\n"
    "mov r15 r3\n",

    // Program 2 - a couple instructions
    "#fragmentShader\n"
    "#input r0.xyzw\n"
    "#input r1.xy\n"
    "#output r12.xyzw\n"
    ""
    "mov r2    r0\n"
    "mov r2.xw r1\n"
    "fadd r2 r2 r2\n"
    "mov r12 r2\n"
    "swizzle r12 r12.wzyx\n",
};

const char *failingPrograms[] = {
    // Program 0 - no instructions
    "#vertexShader\n"
    "#input r0.xyzw"
    "#output r12.xyzw",

    // Program 1 - no inputs
    "#vertexShader\n"
    "#output r12.xyzw"
    "mov r12 r0",

    // Program 2 - no outputs
    "#vertexShader\n"
    "#input r0.xyzw"
    "mov r12 r0",

    // Program 3 - swizzled source
    "#vertexShader\n"
    "#input r0.xyzw"
    "#output r12.xyzw"
    "mov r12 r0.xyz",

    // Program 4 - non-full swizzle
    "#vertexShader\n"
    "#input r0.xyzw"
    "#output r12.xyzw"
    "swizzle r12 r0.xyw",

    // Program 5 - non-contiguous inputs
    "#vertexShader\n"
    "#input r0.xyzw\n"
    "#input r2.xyzw\n"
    "#output r12.xyzw\n"
    "mov r12 r0\n",

    // Program 6 - non-contiguous outputs
    "#vertexShader\n"
    "#input r0.xyzw\n"
    "#output r12.xyzw\n"
    "#output r14.xyzw\n"
    "mov r12 r0\n",

    // Program 7 - input out of scope
    "#vertexShader\n"
    "#input r4.xyzw\n"
    "#output r12.xyzw\n"
    "mov r12 r4\n",

    // Program 8 - outputs out of scope
    "#vertexShader\n"
    "#input r0.xyzw\n"
    "#output r11.xyzw\n"
    "mov r11 r0\n",

    // Program 9 - no shader type directive
    "#input r0.xyzw\n"
    "#output r12.xyzw\n"
    "mov r12 r0\n",

    // Program 10 - multiple shader type directives
    "#vertexShader\n"
    "#vertexShader\n"
    "#input r0.xyzw\n"
    "#output r12.xyzw\n"
    "mov r12 r0\n",
};

int sc_main(int argc, char *argv[]) {
    bool success = true;
    int count = 0;

    for (const char *program : passingPrograms) {
        Isa::PicoGpuBinary binary = {};
        int result = Isa::assembly(program, &binary);

        const auto name = std::string("Program") + std::to_string(count++);
        if (result != 0) {
            Log() << name << "(expect pass): FAIL\n";
            success = false;
        } else {
            Log() << name << "(expect pass): SUCCESS\n";
        }
    }

    count = 0;
    for (const char *program : failingPrograms) {
        Isa::PicoGpuBinary binary = {};
        int result = Isa::assembly(program, &binary);

        const auto name = std::string("Program") + std::to_string(count++);
        if (result == 0) {
            Log() << name << "(expect fail): FAIL\n";
            success = false;
        } else {
            Log() << name << "(expect fail): SUCCESS\n";
        }
    }

    return success ? 0 : 1;
}
