#pragma once

#include "gpu/isa/assembler/pico_gpu_binary.h"

namespace Isa {
    int assembly(const char *code, PicoGpuBinary *outBinary);
}
