#pragma once

#include <systemc.h>

constexpr inline size_t memoryDataTypeByteSize = 4;
using MemoryAddressType = sc_uint<32>;
using MemoryDataType = sc_uint<memoryDataTypeByteSize * 8>;
