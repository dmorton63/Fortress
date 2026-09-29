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
    void (*RunVfsMountsFn)();
    void (*RunVfsResolveFn)(const char *path);
    void (*RunVfsResolveByMountFn)(const char *mountToken, const char *leafToken);
    void (*RunVfsResolveBlockByMountFn)(const char *mountToken, uint32_t blockIndex);
    void (*RunVfsBlockDigestByMountFn)(const char *mountToken, uint32_t blockIndex);
    void (*RunVfsBlockDigestRangeByMountFn)(const char *mountToken, uint32_t startBlock, uint32_t blockCount);
    void (*RunVfsBlockDigestCompareByMountFn)(const char *mountToken, uint32_t leftBlock, uint32_t rightBlock);
    void (*RunVfsBlockDigestScanByMountFn)(const char *mountToken, uint32_t startBlock, uint32_t blockCount);
    void (*RunVfsBlockDigestNonZeroByMountFn)(const char *mountToken, uint32_t startBlock, uint32_t blockCount);
    void (*RunVfsBlockDigestFirstNonZeroByMountFn)(const char *mountToken,
                                                   uint32_t startBlock,
                                                   uint32_t blockCount);
    void (*RunVfsBlockDigestLastNonZeroByMountFn)(const char *mountToken,
                                                  uint32_t startBlock,
                                                  uint32_t blockCount);
    void (*RunVfsBlockDigestSpanByMountFn)(const char *mountToken,
                                           uint32_t startBlock,
                                           uint32_t blockCount);
    void (*RunVfsBlockDigestWindowByMountFn)(const char *mountToken,
                                             uint32_t startBlock,
                                             uint32_t blockCount);
    void (*RunVfsBlockDigestRunsByMountFn)(const char *mountToken,
                                           uint32_t startBlock,
                                           uint32_t blockCount);
    void (*RunVfsBlockDigestTransitionsByMountFn)(const char *mountToken,
                                                  uint32_t startBlock,
                                                  uint32_t blockCount);
    void (*RunVfsBlockDigestDensityByMountFn)(const char *mountToken,
                                              uint32_t startBlock,
                                              uint32_t blockCount);
    void (*RunVfsBlockDigestRatioByMountFn)(const char *mountToken,
                                            uint32_t startBlock,
                                            uint32_t blockCount);
    void (*RunVfsBlockDigestBalanceByMountFn)(const char *mountToken,
                                              uint32_t startBlock,
                                              uint32_t blockCount);
    void (*RunVfsBlockDigestSkewByMountFn)(const char *mountToken,
                                           uint32_t startBlock,
                                           uint32_t blockCount);
    void (*RunVfsBlockDigestTiltByMountFn)(const char *mountToken,
                                           uint32_t startBlock,
                                           uint32_t blockCount);
    void (*RunVfsBlockDigestBiasByMountFn)(const char *mountToken,
                                           uint32_t startBlock,
                                           uint32_t blockCount);
    void (*RunLogSaveByMountFn)(const char *mountToken, uint32_t startBlock, uint32_t blockCount);
    void (*RunServiceDbStatsFn)();
    void (*RunServiceDbFindFn)(uint32_t serviceId);
    void (*RunPortPolicyStatsFn)();
    void (*RunPortAuditLastFn)();
    void (*RunPortAuditDeniedFn)();
    void (*RunPortPolicyCheckFn)(uint32_t portId, uint32_t serviceId);
    void (*RunPortListFn)();
    void (*RunPortOpenFn)(uint32_t portId);
    void (*RunPortCloseFn)(uint32_t portId);
    void (*RunPortLeaseFn)(uint32_t portId);
    void (*RunDesktopZListFn)();
    void (*RunDesktopChildrenFn)(uint32_t parentSurfaceId);

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
