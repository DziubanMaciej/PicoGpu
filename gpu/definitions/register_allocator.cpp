#include "gpu/definitions/register_allocator.h"
#include "gpu/util/error.h"
#include "gpu/util/math.h"

RegisterAllocator::RegisterAllocator(MaskType mask)
    : usedRegistersMask(mask) {}

RegisterAllocator::RegisterAllocator(Isa::Command::CommandStoreIsa isaMetadata)
    : usedRegistersMask(constructUsedRegistersMaskFromIsaMetadata(isaMetadata)) {}

Isa::RegisterSelection RegisterAllocator::allocate() {
    // TODO remember last retrieved bit and add startBit argument to findBit function
    int32_t index = findBit(usedRegistersMask, false);
    FATAL_ERROR_IF(index < 0, "Failed to find free register");
    setBit(usedRegistersMask, index);
    return Isa::RegisterSelection(index);
}

RegisterAllocator::MaskType RegisterAllocator::constructUsedRegistersMaskFromIsaMetadata(Isa::Command::CommandStoreIsa isaMetadata) {
    MaskType mask = 0;

    const auto inputsCount = nonZeroCountToInt(isaMetadata.inputsCount);
    const auto outputsCount = nonZeroCountToInt(isaMetadata.outputsCount);

#define ADD_TO_MASK(index)                              \
    if (inputsCount > index) {                          \
        setBit(mask, isaMetadata.inputRegister##index); \
    }
    ADD_TO_MASK(0);
    ADD_TO_MASK(1);
    ADD_TO_MASK(2);
    ADD_TO_MASK(3);
#undef ADD_TO_MASK

    return mask;
}
