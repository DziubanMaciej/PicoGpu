#pragma once

#include "gpu/isa/isa.h"

class RegisterAllocator {
    static_assert(Isa::generalPurposeRegistersCount == 16, "RegisterAllocator was written with an assumption of 16 registers in GPU");
    using MaskType = uint16_t;

public:
    RegisterAllocator(MaskType mask);
    RegisterAllocator(Isa::Command::CommandStoreIsa isaMetadata);

    Isa::RegisterIndex allocate();

private:
    static MaskType constructUsedRegistersMaskFromIsaMetadata(Isa::Command::CommandStoreIsa isaMetadata);

    MaskType usedRegistersMask;
    Isa::RegisterIndex startRegisterToAllocate = 0; // cached to minimize number of loops through the bitmask
};
