#pragma once

#include "gpu/isa/isa.h"

struct ShaderFrontendRequest {
    union {
        uint32_t isaAddress;
        uint32_t raw;
    } dword0;

    union {
        struct {
            uint32_t clientToken : 16;
            NonZeroCount threadCount : Isa::simdExponent;
        };
        uint32_t raw;
    } dword1;

    union {
        struct {
            NonZeroCount inputsCount : 2;
            NonZeroCount outputsCount : 2;
            NonZeroCount inputSize0 : 2;
            NonZeroCount inputSize1 : 2;
            NonZeroCount inputSize2 : 2;
            NonZeroCount inputSize3 : 2;
            NonZeroCount outputSize0 : 2;
            NonZeroCount outputSize1 : 2;
            NonZeroCount outputSize2 : 2;
            NonZeroCount outputSize3 : 2;
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
