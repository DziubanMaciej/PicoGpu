#pragma once

#include "gpu/isa/isa.h"

#include <systemc.h>

constexpr inline size_t memoryDataTypeByteSize = 4;
using MemoryAddressType = sc_uint<32>;
using MemoryDataType = sc_uint<memoryDataTypeByteSize * 8>;

using VertexPositionFloatType = sc_uint<32>;   // 32-bit float saved as uint
using VertexPositionIntegerType = sc_uint<16>; // actual uint

constexpr inline size_t fragmentColorTypeByteSize = 4; // RGBA, 8 bits per channel
using FragmentColorType = sc_uint<fragmentColorTypeByteSize * 8>;

constexpr inline size_t depthTypeByteSize = 4;
using DepthType = sc_uint<depthTypeByteSize * 8>;

using PointerType = sc_uint<sizeof(void *) * 8>;

constexpr static size_t verticesInPrimitive = 3;
