#pragma once

#include <cstddef>

namespace Fortress::Kernel {

struct FKernelHeapCommandContext {
    const char *CommandBuffer;
    size_t *CommandLength;
    void (*PushLog)(const char *line);
    void (*ClearCommandInput)();
    void (*RunMemorySelfTest)();
    void (*RunXhciMemorySmokeTest)();
    void (*RunMmioSmokeTest)();
};

void ResetKernelCommandHeapState();
bool TryProcessHeapCommand(FKernelHeapCommandContext &context);

} // namespace Fortress::Kernel
