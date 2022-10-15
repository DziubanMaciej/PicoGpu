#include "gpu/blocks/blitter.h"
#include "gpu/util/error.h"
#include "gpu/util/raii_boolean_setter.h"

void Blitter::main() {
    while (true) {
        wait();

        const CommandType commandType = static_cast<CommandType>(command.inpCommandType.read().to_int());
        if (commandType == CommandType::None) {
            continue;
        }

        const bool isWrite = commandType == CommandType::CopyToMem || commandType == CommandType::FillMem;
        const bool isFill = commandType == CommandType::FillMem;
        const MemoryAddressType memoryPtr = command.inpMemoryPtr.read();
        const auto userPtr = reinterpret_cast<uint32_t *>(command.inpUserPtr.read().to_uint64());
        const uint32_t sizeInDwords = command.inpSizeInDwords.read().to_uint();

        RaiiBooleanSetter busySetter{profiling.outBusy};

        for (size_t dwordIndex = 0; dwordIndex < sizeInDwords; dwordIndex++) {
            memory.outEnable = 1;
            memory.outWrite = isWrite;
            memory.outAddress = memoryPtr + 4 * dwordIndex;

            if (isWrite) {
                const size_t userPtrIndex = isFill ? 0 : dwordIndex;
                memory.outData = userPtr[userPtrIndex];
            }

            wait(1);
            memory.outEnable = 0;

            while (!memory.inpCompleted) {
                wait(1);
            }

            memory.outWrite = 0;
            memory.outAddress = 0;
            memory.outData = 0;

            if (!isWrite) {
                userPtr[dwordIndex] = memory.inpData.read();
            }
        }
    }
}
