#include "gpu/isa/assembler/assembler.h"
#include "gpu/util/log.h"

#include <systemc.h>

void expectPass(bool &outSuccess, const char *shaderName, const char *shaderSource) {
    Isa::PicoGpuBinary binary = {};
    int result = Isa::assembly(shaderSource, &binary);
    if (result != 0) {
        Log() << shaderName << " FAILED TO COMPILE\n";
        outSuccess = false;
    } else {
        Log() << shaderName << " OK\n";
    }
}

void expectFail(bool &outSuccess, const char *shaderName, const char *expectedErrorSubstring, const char *shaderSource) {
    Isa::PicoGpuBinary binary = {};
    int result = Isa::assembly(shaderSource, &binary);
    if (result == 0) {
        Log() << shaderName << " COMPILED BUT EXPECTED TO FAIL\n";
        outSuccess = false;
    } else if (binary.getError().find(expectedErrorSubstring) == std::string::npos) {
        Log() << shaderName << " DID NOT CONTAIN EXPECTED ERROR LOG";
        Log() << "Error log was: " << binary.getError() << "\n";
        outSuccess = false;
    } else {
        Log() << shaderName << " OK\n";
    }
}

int sc_main(int argc, char *argv[]) {
    bool success = true;
    int count = 0;

    expectPass(success, "Arg manual passthrough",
               "#vertexShader\n"
               "#input r0.xyzw\n"
               "#output r12.xyzw\n"
               "mov r12 r0\n");
    expectPass(success, "Arg manual passthrough multiple",
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
               "mov r15 r3\n");
    expectPass(success, "Arg auto passthrough",
               "#vertexShader\n"
               "#input r13.xyzw\n"
               "#output r13.xyzw\n"
               "mov r0 r0"); // TODO this mov shouldn't be necessary for passthrough shaders
    expectPass(success, "A couple instructions",
               "#fragmentShader\n"
               "#input r0.xyzw\n"
               "#input r1.xy\n"
               "#output r12.xyzw\n"
               ""
               "mov r2    r0\n"
               "mov r2.xw r1\n"
               "fadd r2 r2 r2\n"
               "mov r12 r2\n"
               "swizzle r12 r12.wzyx\n");
    expectPass(success, "Arg auto passthrough FS",
               "#vertexShader\n"
               "#input r0.xyzw\n"
               "#output r0.xyzw\n"
               "mov r0 r0");
    expectPass(success, "Undefined regs",
               "#vertexShader\n"
               "#input r0.xyzw\n"
               "#output r0.xyzw\n"
               "#undefinedRegs\n"
               "mov r0 r0");

    expectFail(success, "No instructions",
               "", // TODO error is printed to stdout, so we cannot check for it (same for all cases that have this empty)
               "#vertexShader\n"
               "#input r0.xyzw"
               "#output r12.xyzw");

    expectFail(success, "No inputs",
               "VertexShader must use at least one input register",
               "#vertexShader\n"
               "#output r12.xyzw"
               "mov r12 r0");

    expectFail(success, "No outputs VS",
               "VertexShader must use exactly one output register",
               "#vertexShader\n"
               "#input r0.xyzw"
               "mov r12 r0");

    expectFail(success, "No inputs FS",
               "FragmentShader must use exactly one input register",
               "#fragmentShader\n"
               "#output r0.xyzw"
               "mov r12 r0");

    expectFail(success, "Wrong output components VS",
               "VertexShader must use a 4-component position vector as its first output",
               "#vertexShader\n"
               "#input r0.xyzw"
               "#output r0.xy"
               "mov r12 r0");

    expectFail(success, "Wrong input components FS",
               "FragmentShader must use a 4-component position vector as its first input",
               "#fragmentShader\n"
               "#input r0.xy"
               "#output r0.xyzw"
               "mov r12 r0");

    expectFail(success, "Out of order input components",
               "Components for input directive must be used in order: x,y,z,w",
               "#vertexShader\n"
               "#input r0.xz\n"
               "#output r12.xyzw\n"
               "mov r12 r0\n");

    expectFail(success, "Out of order output components",
               "Components for output directive must be used in order: x,y,z,w",
               "#vertexShader\n"
               "#input r0.xyzw\n"
               "#output r12.xzw\n"
               "mov r12 r0\n");

    expectFail(success, "Too many inputs",
               "Too many input directives. Max is 4",
               "#vertexShader\n"
               "#input r0.xyzw\n"
               "#input r1.xyzw\n"
               "#input r2.xyzw\n"
               "#input r3.xyzw\n"
               "#input r4.xyzw\n"
               "#output r12.xyzw\n"
               "mov r12 r0\n");

    expectFail(success, "Too many outputs",
               "Too many output directives. Max is 4",
               "#vertexShader\n"
               "#input r0.xyzw\n"
               "#output r12.xyzw\n"
               "#output r13.xyzw\n"
               "#output r14.xyzw\n"
               "#output r15.xyzw\n"
               "#output r9.xyzw\n"
               "mov r12 r0\n");

    expectFail(success, "Duplicate input",
               "Multiple input directives for r0",
               "#vertexShader\n"
               "#input r0.xyzw\n"
               "#input r0.xyz\n"
               "#output r12.xyzw\n"
               "mov r12 r0\n");

    expectFail(success, "Swizzled source",
               "",
               "#vertexShader\n"
               "#input r0.xyzw"
               "#output r12.xyzw"
               "mov r12 r0.xyz");

    expectFail(success, "Non full swizzle",
               "",
               "#vertexShader\n"
               "#input r0.xyzw"
               "#output r12.xyzw"
               "swizzle r12 r0.xyw");

    expectFail(success, "No shader type directive",
               "No program type specification",
               "#input r0.xyzw\n"
               "#output r12.xyzw\n"
               "mov r12 r0\n");

    expectFail(success, "Multiple shader type directives",
               "Multiple program type specifications",
               "#vertexShader\n"
               "#vertexShader\n"
               "#input r0.xyzw\n"
               "#output r12.xyzw\n"
               "mov r12 r0\n");

    expectFail(success, "No output FS",
               "FragmentShader must use exactly one output register",
               "#fragmentShader\n"
               "#input r0.xyzw\n"
               "mov r12 r0");

    expectFail(success, "Wrong output components FS",
               "FragmentShader must use a 4-component color vector as its only output",
               "#fragmentShader\n"
               "#input r0.xyzw\n"
               "#output r12.xyz\n"
               "mov r12 r0");

    expectFail(success, "Too many outputs FS",
               "FragmentShader must use exactly one output register",
               "#fragmentShader\n"
               "#input r0.xyzw\n"
               "#output r12.xyzw\n"
               "#output r13.x\n"
               "mov r12 r0");

    expectFail(success, "Multiple undefined regs directives",
               "Multiple undefined regs directives",
               "#vertexShader\n"
               "#input r0.xyzw\n"
               "#output r0.xyzw\n"
               "#undefinedRegs\n"
               "#undefinedRegs\n"
               "mov r0 r0");

    return success ? 0 : 1;
}
