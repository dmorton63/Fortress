#pragma once

#include <cstddef>
#include <cstdint>

namespace Fortress::Kernel {

struct FKernelUtilityCommandContext {
    const char *CommandBuffer;
    size_t *CommandLength;
    bool *BootLogFrozen;

    void (*PushLog)(const char *line);
    bool (*StrEqFn)(const char *left, const char *right);
    bool (*StartsWithFn)(const char *value, const char *prefix);
    bool (*ReadTokenFn)(const char *&cursor, char *out, size_t outSize);
    bool (*ParseUIntFn)(const char *value, uint64_t &out);

    void (*RunEventBurstFn)(uint32_t burstCount);
    void (*RunVfsStatFn)();
    void (*RunVfsResolveFn)(const char *path);

    void (*SetHudLogShowTailFn)();
    void (*SetHudLogShowFullFn)();
    void (*SetHudLogShowErrorsFn)();
    void (*SetHudLogShowWarnFn)();
    void (*SetHudLogShowAllIssuesFn)();
    void (*SetHudLogBootFn)();
    void (*SetHudLogHiddenFn)();
    void (*RunTerminalModeQueryFn)();
    void (*SetTerminalModeOnFn)();
    void (*SetTerminalModeOffFn)();
    void (*RunParallelHudQueryFn)();
    void (*SetParallelHudOnFn)();
    void (*SetParallelHudOffFn)();

    void (*RunKbdLayoutQueryFn)();
    void (*RunKbdLayoutSetUsFn)();
    void (*RunKbdLayoutSetDvorakFn)();
    void (*RunKbdModsQueryFn)();

    void (*RunTextShaperQueryFn)();
    void (*RunTextShaperSetBasicFn)();
    bool (*RunTextShaperSetWrapFn)(const char *args);

    void (*RunFontCacheQueryFn)();
    void (*RunFontCacheResetFn)();

    void (*RunHelpFn)();
    void (*RunEventHealthFn)();
    void (*RunStatsFn)();
    void (*RunRenderLayersFn)();

    void (*ClearCommandInputFn)();
};

bool TryProcessUtilityCommand(FKernelUtilityCommandContext &context);

} // namespace Fortress::Kernel
