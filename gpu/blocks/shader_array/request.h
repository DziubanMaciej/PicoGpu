#pragma once

// TODO: isa.h is only included for this utility type. It should be move to some utility header, we don't need the entire ISA here.
#include "gpu/isa/isa.h"

struct ShaderFrontendRequest {
    union {
        uint32_t isaAddress;
        uint32_t raw;
    } dword0;

    union {
        struct {
            uint32_t clientToken : 16;
            uint32_t threadCount : Isa::simdExponent;
        };
        uint32_t raw;
    } dword1;

    union {
        struct {
            Isa::Command::NonZeroCount inputsCount : 2;
            Isa::Command::NonZeroCount outputsCount : 2;
            Isa::Command::NonZeroCount inputSize0 : 2;
            Isa::Command::NonZeroCount inputSize1 : 2;
            Isa::Command::NonZeroCount inputSize2 : 2;
            Isa::Command::NonZeroCount inputSize3 : 2;
            Isa::Command::NonZeroCount outputSize0 : 2;
            Isa::Command::NonZeroCount outputSize1 : 2;
            Isa::Command::NonZeroCount outputSize2 : 2;
            Isa::Command::NonZeroCount outputSize3 : 2;
        };
        uint32_t raw;
    } dword2;
};

static_assert(sizeof(ShaderFrontendRequest) == 12);

struct ShaderFrontendResponse {
    union {
        struct {
            uint32_t clientToken : 16;
        };
        uint32_t raw;
    } dword0;
};
static_assert(sizeof(ShaderFrontendResponse) == 4);
