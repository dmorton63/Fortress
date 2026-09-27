#pragma once

#include <cstddef>
#include <cstdint>

namespace Fortress::Kernel {

struct FKernelMemoryMapCommandContext {
    char *CommandBuffer;
    size_t *CommandLength;
    void (*PushLogFn)(const char *line);
    bool (*ReserveVirtualRangeFn)(uint64_t pageCount, uint64_t &outVirtualAddress);
    bool (*ReleaseVirtualRangeFn)(uint64_t virtualAddress, uint64_t pageCount);
};

void ResetKernelCommandMemoryMapState();
bool TryProcessMemoryMapCommand(FKernelMemoryMapCommandContext &context);

} // namespace Fortress::Kernel
