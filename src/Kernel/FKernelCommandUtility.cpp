#include "Fortress/Kernel/FKernelCommandUtility.hpp"

namespace Fortress::Kernel {

static bool SplitTokenAt(const char *token,
                         char delimiter,
                         char *left,
                         size_t leftSize,
                         char *right,
                         size_t rightSize) {
    if (token == nullptr || left == nullptr || right == nullptr || leftSize == 0u || rightSize == 0u) {
        return false;
    }

    left[0] = '\0';
    right[0] = '\0';

    size_t split = 0u;
    while (token[split] != '\0' && token[split] != delimiter) {
        split++;
    }

    if (token[split] != delimiter || split == 0u) {
        return false;
    }

    const char *rhs = token + split + 1u;
    if (*rhs == '\0') {
        return false;
    }

    size_t leftLen = split;
    size_t rightLen = 0u;
    while (rhs[rightLen] != '\0') {
        rightLen++;
    }

    if (leftLen + 1u > leftSize || rightLen + 1u > rightSize) {
        return false;
    }

    for (size_t i = 0u; i < leftLen; i++) {
        left[i] = token[i];
    }
    left[leftLen] = '\0';

    for (size_t i = 0u; i < rightLen; i++) {
        right[i] = rhs[i];
    }
    right[rightLen] = '\0';
    return true;
}

static bool ParsePackedRangeToken(const char *token,
                                  uint64_t &outStart,
                                  uint64_t &outCount,
                                  bool (*parseUIntFn)(const char *value, uint64_t &out)) {
    if (token == nullptr || token[0] == '\0' || parseUIntFn == nullptr) {
        return false;
    }

    char left[24] = {};
    char right[24] = {};
    const char delimiters[3] = {'.', ':', '/'};
    for (size_t i = 0u; i < sizeof(delimiters); i++) {
        if (!SplitTokenAt(token, delimiters[i], left, sizeof(left), right, sizeof(right))) {
            continue;
        }

        if (!parseUIntFn(left, outStart) || !parseUIntFn(right, outCount)) {
            return false;
        }

        return true;
    }

    return false;
}

static bool ParsePackedPairToken(const char *token,
                                 uint64_t &outLeft,
                                 uint64_t &outRight,
                                 bool (*parseUIntFn)(const char *value, uint64_t &out)) {
    if (token == nullptr || token[0] == '\0' || parseUIntFn == nullptr) {
        return false;
    }

    char left[24] = {};
    char right[24] = {};
    const char delimiters[3] = {'.', ':', '/'};
    for (size_t i = 0u; i < sizeof(delimiters); i++) {
        if (!SplitTokenAt(token, delimiters[i], left, sizeof(left), right, sizeof(right))) {
            continue;
        }

        if (!parseUIntFn(left, outLeft) || !parseUIntFn(right, outRight)) {
            return false;
        }

        return true;
    }

    return false;
}

bool TryProcessUtilityCommand(FKernelUtilityCommandContext &context) {
    if (context.CommandBuffer == nullptr || context.CommandLength == nullptr || context.PushLog == nullptr ||
        context.StrEqFn == nullptr || context.StartsWithFn == nullptr || context.ReadTokenFn == nullptr ||
        context.ParseUIntFn == nullptr || context.RunEventBurstFn == nullptr || context.RunVfsStatFn == nullptr ||
        context.RunVfsMountsFn == nullptr || context.RunVfsResolveFn == nullptr ||
        context.RunVfsResolveByMountFn == nullptr || context.RunVfsResolveBlockByMountFn == nullptr ||
        context.RunVfsBlockDigestByMountFn == nullptr || context.RunVfsBlockDigestRangeByMountFn == nullptr ||
        context.RunVfsBlockDigestCompareByMountFn == nullptr ||
        context.RunVfsBlockDigestScanByMountFn == nullptr ||
        context.RunVfsBlockDigestNonZeroByMountFn == nullptr ||
        context.RunVfsBlockDigestFirstNonZeroByMountFn == nullptr ||
        context.RunLogSaveByMountFn == nullptr ||
        context.RunServiceDbStatsFn == nullptr ||
        context.RunServiceDbFindFn == nullptr || context.RunPortPolicyStatsFn == nullptr ||
        context.RunPortAuditLastFn == nullptr || context.RunPortAuditDeniedFn == nullptr ||
        context.RunPortPolicyCheckFn == nullptr || context.RunPortListFn == nullptr ||
        context.RunPortOpenFn == nullptr || context.RunPortCloseFn == nullptr ||
        context.RunPortLeaseFn == nullptr || context.RunDesktopZListFn == nullptr ||
        context.RunDesktopChildrenFn == nullptr || context.SetHudLogShowTailFn == nullptr ||
        context.SetHudLogShowFullFn == nullptr || context.SetHudLogShowErrorsFn == nullptr ||
        context.SetHudLogShowWarnFn == nullptr || context.SetHudLogShowAllIssuesFn == nullptr ||
        context.SetHudLogBootFn == nullptr || context.SetHudLogHiddenFn == nullptr ||
        context.RunTerminalModeQueryFn == nullptr || context.SetTerminalModeOnFn == nullptr ||
        context.SetTerminalModeOffFn == nullptr ||
        context.RunParallelHudQueryFn == nullptr || context.SetParallelHudOnFn == nullptr ||
        context.SetParallelHudOffFn == nullptr ||
        context.RunKbdLayoutQueryFn == nullptr || context.RunKbdLayoutSetUsFn == nullptr ||
        context.RunKbdLayoutSetDvorakFn == nullptr || context.RunKbdModsQueryFn == nullptr ||
        context.RunTextShaperQueryFn == nullptr || context.RunTextShaperSetBasicFn == nullptr ||
        context.RunTextShaperSetWrapFn == nullptr || context.RunFontCacheQueryFn == nullptr ||
        context.RunFontCacheResetFn == nullptr || context.RunHelpFn == nullptr ||
        context.RunEventHealthFn == nullptr || context.RunStatsFn == nullptr ||
        context.RunRenderLayersFn == nullptr || context.ClearCommandInputFn == nullptr) {
        return false;
    }

    if (context.StrEqFn(context.CommandBuffer, "showlog") || context.StartsWithFn(context.CommandBuffer, "showlog ")) {
        if (context.StrEqFn(context.CommandBuffer, "showlog")) {
            context.SetHudLogShowTailFn();
            return true;
        }

        const char *cursor = context.CommandBuffer + 8;
        char modeToken[16] = {};
        if (!context.ReadTokenFn(cursor, modeToken, sizeof(modeToken))) {
            context.PushLog("SHOWLOG USAGE [TAIL|FULL|ERRORS|WARN|ALLISSUES]");
            context.ClearCommandInputFn();
            return true;
        }

        if (context.StrEqFn(modeToken, "tail")) {
            context.SetHudLogShowTailFn();
        } else if (context.StrEqFn(modeToken, "full")) {
            context.SetHudLogShowFullFn();
        } else if (context.StrEqFn(modeToken, "errors")) {
            context.SetHudLogShowErrorsFn();
        } else if (context.StrEqFn(modeToken, "warn")) {
            context.SetHudLogShowWarnFn();
        } else if (context.StrEqFn(modeToken, "allissues")) {
            context.SetHudLogShowAllIssuesFn();
        } else {
            context.PushLog("SHOWLOG MODE INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("SHOWLOG TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "bootlog")) {
        context.SetHudLogBootFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "hidelog")) {
        context.SetHudLogHiddenFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "terminal")) {
        context.SetTerminalModeOnFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "terminal status")) {
        context.RunTerminalModeQueryFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "terminal on")) {
        context.SetTerminalModeOnFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "terminal off")) {
        context.SetTerminalModeOffFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "parallelhud")) {
        context.RunParallelHudQueryFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "parallelhud on")) {
        context.SetParallelHudOnFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "parallelhud off")) {
        context.SetParallelHudOffFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "kbdlayout")) {
        context.RunKbdLayoutQueryFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "kbdlayout us")) {
        context.RunKbdLayoutSetUsFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "kbdlayout dvorak")) {
        context.RunKbdLayoutSetDvorakFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "kbdmods")) {
        context.RunKbdModsQueryFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "textshaper")) {
        context.RunTextShaperQueryFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "textshaper basic")) {
        context.RunTextShaperSetBasicFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "textshaper wrap")) {
        (void)context.RunTextShaperSetWrapFn(nullptr);
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "textshaper wrap ")) {
        (void)context.RunTextShaperSetWrapFn(context.CommandBuffer + 16);
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "fontcache")) {
        context.RunFontCacheQueryFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "fontcache reset")) {
        context.RunFontCacheResetFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "help")) {
        context.RunHelpFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "eventhealth")) {
        context.RunEventHealthFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "eventburst") || context.StartsWithFn(context.CommandBuffer, "eventburst ")) {
        uint64_t burstCount = 2048u;
        if (context.StartsWithFn(context.CommandBuffer, "eventburst ")) {
            if (!context.ParseUIntFn(context.CommandBuffer + 11, burstCount)) {
                context.PushLog("EVENTBURST ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        }

        if (burstCount == 0u || burstCount > 200000u) {
            context.PushLog("EVENTBURST RANGE 1..200000");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunEventBurstFn(static_cast<uint32_t>(burstCount));
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "vfsstat")) {
        context.RunVfsStatFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "vfs stat")) {
        context.RunVfsStatFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "vfsmounts")) {
        context.RunVfsMountsFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "vfs mounts")) {
        context.RunVfsMountsFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "srdbstat")) {
        context.RunServiceDbStatsFn();
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "srdbfind ")) {
        uint64_t serviceId = 0u;
        if (!context.ParseUIntFn(context.CommandBuffer + 9, serviceId) || serviceId > 0xFFFFFFFFu) {
            context.PushLog("SRDBFIND ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunServiceDbFindFn(static_cast<uint32_t>(serviceId));
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "portstat")) {
        context.RunPortPolicyStatsFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "portlist")) {
        context.RunPortListFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "portaudit last")) {
        context.RunPortAuditLastFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "portaudit denied")) {
        context.RunPortAuditDeniedFn();
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "portcheck ")) {
        const char *cursor = context.CommandBuffer + 10;
        char portToken[16] = {};
        char serviceToken[16] = {};
        if (!context.ReadTokenFn(cursor, portToken, sizeof(portToken)) ||
            !context.ReadTokenFn(cursor, serviceToken, sizeof(serviceToken))) {
            context.PushLog("PORTCHECK USAGE PORTID SERVICEID");
            context.ClearCommandInputFn();
            return true;
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("PORTCHECK TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t portId = 0u;
        uint64_t serviceId = 0u;
        if (!context.ParseUIntFn(portToken, portId) || !context.ParseUIntFn(serviceToken, serviceId) ||
            portId > 0xFFFFu || serviceId > 0xFFFFFFFFu) {
            context.PushLog("PORTCHECK ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunPortPolicyCheckFn(static_cast<uint32_t>(portId), static_cast<uint32_t>(serviceId));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "portopen ")) {
        uint64_t portId = 0u;
        if (!context.ParseUIntFn(context.CommandBuffer + 9, portId) || portId > 0xFFFFu) {
            context.PushLog("PORTOPEN ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunPortOpenFn(static_cast<uint32_t>(portId));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "portclose ")) {
        uint64_t portId = 0u;
        if (!context.ParseUIntFn(context.CommandBuffer + 10, portId) || portId > 0xFFFFu) {
            context.PushLog("PORTCLOSE ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunPortCloseFn(static_cast<uint32_t>(portId));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "portlease ")) {
        uint64_t portId = 0u;
        if (!context.ParseUIntFn(context.CommandBuffer + 10, portId) || portId > 0xFFFFu) {
            context.PushLog("PORTLEASE ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunPortLeaseFn(static_cast<uint32_t>(portId));
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "dskzlist")) {
        context.RunDesktopZListFn();
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "dskchildren ")) {
        uint64_t parentSurfaceId = 0u;
        if (!context.ParseUIntFn(context.CommandBuffer + 12, parentSurfaceId) || parentSurfaceId > 0xFFFFFFFFu) {
            context.PushLog("DSKCHILDREN ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunDesktopChildrenFn(static_cast<uint32_t>(parentSurfaceId));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsresolve ")) {
        const char *cursor = context.CommandBuffer + 11;
        char pathToken[96] = {};
        if (!context.ReadTokenFn(cursor, pathToken, sizeof(pathToken))) {
            context.PushLog("USAGE: VFSRESOLVE /ABS/PATH");
            context.ClearCommandInputFn();
            return true;
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSRESOLVE TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsResolveFn(pathToken);
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs resolve ")) {
        const char *cursor = context.CommandBuffer + 12;
        char firstToken[96] = {};
        if (!context.ReadTokenFn(cursor, firstToken, sizeof(firstToken))) {
            context.PushLog("VFS RESOLVE USAGE /ABS/PATH");
            context.ClearCommandInputFn();
            return true;
        }

        if (firstToken[0] == '/') {
            char extraToken[8] = {};
            if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
                context.PushLog("VFS RESOLVE TOO MANY ARGS");
                context.ClearCommandInputFn();
                return true;
            }

            context.RunVfsResolveFn(firstToken);
            return true;
        }

        if (context.StrEqFn(firstToken, "bootblk")) {
            char blockToken[24] = {};
            if (!context.ReadTokenFn(cursor, blockToken, sizeof(blockToken))) {
                context.RunVfsResolveFn("/boot/blk");
                return true;
            }

            char extraToken[8] = {};
            if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
                context.PushLog("VFS RESOLVE BOOTBLK TOO MANY ARGS");
                context.ClearCommandInputFn();
                return true;
            }

            uint64_t blockIndex = 0u;
            if (!context.ParseUIntFn(blockToken, blockIndex) || blockIndex > 0xFFFFFFFFu) {
                context.PushLog("VFS RESOLVE BOOTBLK ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }

            context.RunVfsResolveBlockByMountFn("boot", static_cast<uint32_t>(blockIndex));
            return true;
        }

        if (context.StartsWithFn(firstToken, "bootblk/") || context.StartsWithFn(firstToken, "bootblk:")) {
            const char *suffix = firstToken + 8;
            uint64_t blockIndex = 0u;
            if (!context.ParseUIntFn(suffix, blockIndex) || blockIndex > 0xFFFFFFFFu) {
                context.PushLog("VFS RESOLVE BOOTBLK ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }

            char extraToken[8] = {};
            if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
                context.PushLog("VFS RESOLVE BOOTBLK TOO MANY ARGS");
                context.ClearCommandInputFn();
                return true;
            }

            context.RunVfsResolveBlockByMountFn("boot", static_cast<uint32_t>(blockIndex));
            return true;
        }

        if (context.StrEqFn(firstToken, "boot0")) {
            char extraToken[8] = {};
            if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
                context.PushLog("VFS RESOLVE BOOT0 TOO MANY ARGS");
                context.ClearCommandInputFn();
                return true;
            }

            context.RunVfsResolveFn("/boot/blk/0");
            return true;
        }

        if (context.StrEqFn(firstToken, "boot")) {
            char leafToken[72] = {};
            if (!context.ReadTokenFn(cursor, leafToken, sizeof(leafToken))) {
                context.PushLog("VFS RESOLVE BOOT USAGE NAME");
                context.ClearCommandInputFn();
                return true;
            }

            char extraToken[8] = {};
            if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
                context.PushLog("VFS RESOLVE BOOT TOO MANY ARGS");
                context.ClearCommandInputFn();
                return true;
            }

            context.RunVfsResolveByMountFn("boot", leafToken);
            return true;
        }

        if (context.StartsWithFn(firstToken, "boot/")) {
            const char *leafToken = firstToken + 5;
            if (*leafToken == '\0') {
                context.PushLog("VFS RESOLVE BOOT USAGE NAME");
                context.ClearCommandInputFn();
                return true;
            }

            char extraToken[8] = {};
            if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
                context.PushLog("VFS RESOLVE BOOT TOO MANY ARGS");
                context.ClearCommandInputFn();
                return true;
            }

            context.RunVfsResolveByMountFn("boot", leafToken);
            return true;
        }

        if (context.StrEqFn(firstToken, "mblk")) {
            char mountToken[32] = {};
            char blockToken[24] = {};
            if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken))) {
                context.PushLog("VFS RESOLVE MBLK USAGE MOUNT INDEX");
                context.ClearCommandInputFn();
                return true;
            }

            if (!context.ReadTokenFn(cursor, blockToken, sizeof(blockToken))) {
                char splitMount[32] = {};
                char splitBlock[24] = {};
                if (!SplitTokenAt(mountToken,
                                  ':',
                                  splitMount,
                                  sizeof(splitMount),
                                  splitBlock,
                                  sizeof(splitBlock))) {
                    context.PushLog("VFS RESOLVE MBLK USAGE MOUNT INDEX");
                    context.ClearCommandInputFn();
                    return true;
                }

                for (size_t i = 0u; splitMount[i] != '\0'; i++) {
                    mountToken[i] = splitMount[i];
                    mountToken[i + 1u] = '\0';
                }
                for (size_t i = 0u; splitBlock[i] != '\0'; i++) {
                    blockToken[i] = splitBlock[i];
                    blockToken[i + 1u] = '\0';
                }
            }

            char extraToken[8] = {};
            if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
                context.PushLog("VFS RESOLVE MBLK TOO MANY ARGS");
                context.ClearCommandInputFn();
                return true;
            }

            uint64_t blockIndex = 0u;
            if (!context.ParseUIntFn(blockToken, blockIndex) || blockIndex > 0xFFFFFFFFu) {
                context.PushLog("VFS RESOLVE MBLK ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }

            context.RunVfsResolveBlockByMountFn(mountToken, static_cast<uint32_t>(blockIndex));
            return true;
        }

        if (context.StrEqFn(firstToken, "mresolve")) {
            char mountToken[32] = {};
            char leafToken[72] = {};
            if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken))) {
                context.PushLog("VFS RESOLVE MRESOLVE USAGE MOUNT NAME");
                context.ClearCommandInputFn();
                return true;
            }

            if (!context.ReadTokenFn(cursor, leafToken, sizeof(leafToken))) {
                char splitMount[32] = {};
                char splitLeaf[72] = {};
                if (!SplitTokenAt(mountToken,
                                  ':',
                                  splitMount,
                                  sizeof(splitMount),
                                  splitLeaf,
                                  sizeof(splitLeaf))) {
                    context.PushLog("VFS RESOLVE MRESOLVE USAGE MOUNT NAME");
                    context.ClearCommandInputFn();
                    return true;
                }

                for (size_t i = 0u; splitMount[i] != '\0'; i++) {
                    mountToken[i] = splitMount[i];
                    mountToken[i + 1u] = '\0';
                }
                for (size_t i = 0u; splitLeaf[i] != '\0'; i++) {
                    leafToken[i] = splitLeaf[i];
                    leafToken[i + 1u] = '\0';
                }
            }

            char extraToken[8] = {};
            if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
                context.PushLog("VFS RESOLVE MRESOLVE TOO MANY ARGS");
                context.ClearCommandInputFn();
                return true;
            }

            context.RunVfsResolveByMountFn(mountToken, leafToken);
            return true;
        }

        context.PushLog("VFS RESOLVE ARG INVALID");
        context.ClearCommandInputFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "vfsboot0")) {
        context.RunVfsResolveFn("/boot/blk/0");
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "vfs boot0")) {
        context.RunVfsResolveFn("/boot/blk/0");
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsmresolve ")) {
        const char *cursor = context.CommandBuffer + 11;
        char mountToken[32] = {};
        char leafToken[72] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken))) {
            context.PushLog("VFSMRESOLVE USAGE MOUNT NAME");
            context.ClearCommandInputFn();
            return true;
        }

        if (!context.ReadTokenFn(cursor, leafToken, sizeof(leafToken))) {
            char splitMount[32] = {};
            char splitLeaf[72] = {};
            if (!SplitTokenAt(mountToken,
                              ':',
                              splitMount,
                              sizeof(splitMount),
                              splitLeaf,
                              sizeof(splitLeaf))) {
                context.PushLog("VFSMRESOLVE USAGE MOUNT NAME");
                context.ClearCommandInputFn();
                return true;
            }

            for (size_t i = 0u; splitMount[i] != '\0'; i++) {
                mountToken[i] = splitMount[i];
                mountToken[i + 1u] = '\0';
            }
            for (size_t i = 0u; splitLeaf[i] != '\0'; i++) {
                leafToken[i] = splitLeaf[i];
                leafToken[i + 1u] = '\0';
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSMRESOLVE TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsResolveByMountFn(mountToken, leafToken);
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs mresolve ")) {
        const char *cursor = context.CommandBuffer + 13;
        char mountToken[32] = {};
        char leafToken[72] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken))) {
            context.PushLog("VFS MRESOLVE USAGE MOUNT NAME");
            context.ClearCommandInputFn();
            return true;
        }

        if (!context.ReadTokenFn(cursor, leafToken, sizeof(leafToken))) {
            char splitMount[32] = {};
            char splitLeaf[72] = {};
            if (!SplitTokenAt(mountToken,
                              ':',
                              splitMount,
                              sizeof(splitMount),
                              splitLeaf,
                              sizeof(splitLeaf))) {
                context.PushLog("VFS MRESOLVE USAGE MOUNT NAME");
                context.ClearCommandInputFn();
                return true;
            }

            for (size_t i = 0u; splitMount[i] != '\0'; i++) {
                mountToken[i] = splitMount[i];
                mountToken[i + 1u] = '\0';
            }
            for (size_t i = 0u; splitLeaf[i] != '\0'; i++) {
                leafToken[i] = splitLeaf[i];
                leafToken[i + 1u] = '\0';
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS MRESOLVE TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsResolveByMountFn(mountToken, leafToken);
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsmblk ")) {
        const char *cursor = context.CommandBuffer + 8;
        char mountToken[32] = {};
        char blockToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken))) {
            context.PushLog("VFSMBLK USAGE MOUNT INDEX");
            context.ClearCommandInputFn();
            return true;
        }

        if (!context.ReadTokenFn(cursor, blockToken, sizeof(blockToken))) {
            char splitMount[32] = {};
            char splitBlock[24] = {};
            if (!SplitTokenAt(mountToken,
                              ':',
                              splitMount,
                              sizeof(splitMount),
                              splitBlock,
                              sizeof(splitBlock))) {
                context.PushLog("VFSMBLK USAGE MOUNT INDEX");
                context.ClearCommandInputFn();
                return true;
            }

            for (size_t i = 0u; splitMount[i] != '\0'; i++) {
                mountToken[i] = splitMount[i];
                mountToken[i + 1u] = '\0';
            }
            for (size_t i = 0u; splitBlock[i] != '\0'; i++) {
                blockToken[i] = splitBlock[i];
                blockToken[i + 1u] = '\0';
            }
        }

        uint64_t blockIndex = 0u;
        if (!context.ParseUIntFn(blockToken, blockIndex) || blockIndex > 0xFFFFFFFFu) {
            context.PushLog("VFSMBLK ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsResolveBlockByMountFn(mountToken, static_cast<uint32_t>(blockIndex));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs mblk ")) {
        const char *cursor = context.CommandBuffer + 9;
        char mountToken[32] = {};
        char blockToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken))) {
            context.PushLog("VFS MBLK USAGE MOUNT INDEX");
            context.ClearCommandInputFn();
            return true;
        }

        if (!context.ReadTokenFn(cursor, blockToken, sizeof(blockToken))) {
            char splitMount[32] = {};
            char splitBlock[24] = {};
            if (!SplitTokenAt(mountToken,
                              ':',
                              splitMount,
                              sizeof(splitMount),
                              splitBlock,
                              sizeof(splitBlock))) {
                context.PushLog("VFS MBLK USAGE MOUNT INDEX");
                context.ClearCommandInputFn();
                return true;
            }

            for (size_t i = 0u; splitMount[i] != '\0'; i++) {
                mountToken[i] = splitMount[i];
                mountToken[i + 1u] = '\0';
            }
            for (size_t i = 0u; splitBlock[i] != '\0'; i++) {
                blockToken[i] = splitBlock[i];
                blockToken[i + 1u] = '\0';
            }
        }

        uint64_t blockIndex = 0u;
        if (!context.ParseUIntFn(blockToken, blockIndex) || blockIndex > 0xFFFFFFFFu) {
            context.PushLog("VFS MBLK ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsResolveBlockByMountFn(mountToken, static_cast<uint32_t>(blockIndex));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsblkdigest ")) {
        const char *cursor = context.CommandBuffer + 13;
        char mountToken[32] = {};
        char blockToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken))) {
            context.PushLog("VFSBLKDIGEST USAGE MOUNT INDEX");
            context.ClearCommandInputFn();
            return true;
        }

        if (!context.ReadTokenFn(cursor, blockToken, sizeof(blockToken))) {
            char splitMount[32] = {};
            char splitBlock[24] = {};
            if (!SplitTokenAt(mountToken,
                              ':',
                              splitMount,
                              sizeof(splitMount),
                              splitBlock,
                              sizeof(splitBlock))) {
                context.PushLog("VFSBLKDIGEST USAGE MOUNT INDEX");
                context.ClearCommandInputFn();
                return true;
            }

            for (size_t i = 0u; splitMount[i] != '\0'; i++) {
                mountToken[i] = splitMount[i];
                mountToken[i + 1u] = '\0';
            }
            for (size_t i = 0u; splitBlock[i] != '\0'; i++) {
                blockToken[i] = splitBlock[i];
                blockToken[i + 1u] = '\0';
            }
        }

        uint64_t blockIndex = 0u;
        if (!context.ParseUIntFn(blockToken, blockIndex) || blockIndex > 0xFFFFFFFFu) {
            context.PushLog("VFSBLKDIGEST ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestByMountFn(mountToken, static_cast<uint32_t>(blockIndex));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs blkdigest ")) {
        const char *cursor = context.CommandBuffer + 14;
        char mountToken[32] = {};
        char blockToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken))) {
            context.PushLog("VFS BLKDIGEST USAGE MOUNT INDEX");
            context.ClearCommandInputFn();
            return true;
        }

        if (!context.ReadTokenFn(cursor, blockToken, sizeof(blockToken))) {
            char splitMount[32] = {};
            char splitBlock[24] = {};
            if (!SplitTokenAt(mountToken,
                              ':',
                              splitMount,
                              sizeof(splitMount),
                              splitBlock,
                              sizeof(splitBlock))) {
                context.PushLog("VFS BLKDIGEST USAGE MOUNT INDEX");
                context.ClearCommandInputFn();
                return true;
            }

            for (size_t i = 0u; splitMount[i] != '\0'; i++) {
                mountToken[i] = splitMount[i];
                mountToken[i + 1u] = '\0';
            }
            for (size_t i = 0u; splitBlock[i] != '\0'; i++) {
                blockToken[i] = splitBlock[i];
                blockToken[i + 1u] = '\0';
            }
        }

        uint64_t blockIndex = 0u;
        if (!context.ParseUIntFn(blockToken, blockIndex) || blockIndex > 0xFFFFFFFFu) {
            context.PushLog("VFS BLKDIGEST ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestByMountFn(mountToken, static_cast<uint32_t>(blockIndex));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigestrange ")) {
        const char *cursor = context.CommandBuffer + 15;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTRANGE USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTRANGE ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTRANGE USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTRANGE TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 16u) {
            context.PushLog("VFSDIGESTRANGE COUNT RANGE 1..16");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestRangeByMountFn(mountToken,
                                                static_cast<uint32_t>(startBlock),
                                                static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digestrange ")) {
        const char *cursor = context.CommandBuffer + 16;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTRANGE USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTRANGE ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTRANGE USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTRANGE TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 16u) {
            context.PushLog("VFS DIGESTRANGE COUNT RANGE 1..16");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestRangeByMountFn(mountToken,
                                                static_cast<uint32_t>(startBlock),
                                                static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigestcmp ")) {
        const char *cursor = context.CommandBuffer + 13;
        char mountToken[32] = {};
        char leftToken[24] = {};
        char rightToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, leftToken, sizeof(leftToken))) {
            context.PushLog("VFSDIGESTCMP USAGE MOUNT LEFT RIGHT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t leftBlock = 0u;
        uint64_t rightBlock = 0u;
        const bool hasRightToken = context.ReadTokenFn(cursor, rightToken, sizeof(rightToken));
        if (hasRightToken) {
            if (!context.ParseUIntFn(leftToken, leftBlock) || !context.ParseUIntFn(rightToken, rightBlock) ||
                leftBlock > 0xFFFFFFFFu || rightBlock > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTCMP ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedPairToken(leftToken, leftBlock, rightBlock, context.ParseUIntFn) ||
                leftBlock > 0xFFFFFFFFu || rightBlock > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTCMP USAGE MOUNT LEFT RIGHT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTCMP TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestCompareByMountFn(mountToken,
                                                  static_cast<uint32_t>(leftBlock),
                                                  static_cast<uint32_t>(rightBlock));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digestcmp ")) {
        const char *cursor = context.CommandBuffer + 14;
        char mountToken[32] = {};
        char leftToken[24] = {};
        char rightToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, leftToken, sizeof(leftToken))) {
            context.PushLog("VFS DIGESTCMP USAGE MOUNT LEFT RIGHT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t leftBlock = 0u;
        uint64_t rightBlock = 0u;
        const bool hasRightToken = context.ReadTokenFn(cursor, rightToken, sizeof(rightToken));
        if (hasRightToken) {
            if (!context.ParseUIntFn(leftToken, leftBlock) || !context.ParseUIntFn(rightToken, rightBlock) ||
                leftBlock > 0xFFFFFFFFu || rightBlock > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTCMP ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedPairToken(leftToken, leftBlock, rightBlock, context.ParseUIntFn) ||
                leftBlock > 0xFFFFFFFFu || rightBlock > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTCMP USAGE MOUNT LEFT RIGHT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTCMP TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestCompareByMountFn(mountToken,
                                                  static_cast<uint32_t>(leftBlock),
                                                  static_cast<uint32_t>(rightBlock));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigestscan ")) {
        const char *cursor = context.CommandBuffer + 14;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTSCAN USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTSCAN ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTSCAN USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTSCAN TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 64u) {
            context.PushLog("VFSDIGESTSCAN COUNT RANGE 1..64");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestScanByMountFn(mountToken,
                                               static_cast<uint32_t>(startBlock),
                                               static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digestscan ")) {
        const char *cursor = context.CommandBuffer + 15;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTSCAN USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTSCAN ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTSCAN USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTSCAN TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 64u) {
            context.PushLog("VFS DIGESTSCAN COUNT RANGE 1..64");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestScanByMountFn(mountToken,
                                               static_cast<uint32_t>(startBlock),
                                               static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigestnz ")) {
        const char *cursor = context.CommandBuffer + 12;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTNZ USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTNZ ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTNZ USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTNZ TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 128u) {
            context.PushLog("VFSDIGESTNZ COUNT RANGE 1..128");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestNonZeroByMountFn(mountToken,
                                                  static_cast<uint32_t>(startBlock),
                                                  static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digestnz ")) {
        const char *cursor = context.CommandBuffer + 13;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTNZ USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTNZ ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTNZ USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTNZ TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 128u) {
            context.PushLog("VFS DIGESTNZ COUNT RANGE 1..128");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestNonZeroByMountFn(mountToken,
                                                  static_cast<uint32_t>(startBlock),
                                                  static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigestfirst ")) {
        const char *cursor = context.CommandBuffer + 15;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTFIRST USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTFIRST ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTFIRST USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTFIRST TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 256u) {
            context.PushLog("VFSDIGESTFIRST COUNT RANGE 1..256");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestFirstNonZeroByMountFn(mountToken,
                                                       static_cast<uint32_t>(startBlock),
                                                       static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digestfirst ")) {
        const char *cursor = context.CommandBuffer + 16;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTFIRST USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTFIRST ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTFIRST USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTFIRST TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 256u) {
            context.PushLog("VFS DIGESTFIRST COUNT RANGE 1..256");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestFirstNonZeroByMountFn(mountToken,
                                                       static_cast<uint32_t>(startBlock),
                                                       static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigestlast ")) {
        const char *cursor = context.CommandBuffer + 14;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTLAST USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTLAST ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTLAST USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTLAST TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 512u) {
            context.PushLog("VFSDIGESTLAST COUNT RANGE 1..512");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestLastNonZeroByMountFn(mountToken,
                                                      static_cast<uint32_t>(startBlock),
                                                      static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digestlast ")) {
        const char *cursor = context.CommandBuffer + 15;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTLAST USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTLAST ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTLAST USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTLAST TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 512u) {
            context.PushLog("VFS DIGESTLAST COUNT RANGE 1..512");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestLastNonZeroByMountFn(mountToken,
                                                      static_cast<uint32_t>(startBlock),
                                                      static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigestspan ")) {
        const char *cursor = context.CommandBuffer + 14;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTSPAN USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTSPAN ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTSPAN USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTSPAN TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 1024u) {
            context.PushLog("VFSDIGESTSPAN COUNT RANGE 1..1024");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestSpanByMountFn(mountToken,
                                               static_cast<uint32_t>(startBlock),
                                               static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digestspan ")) {
        const char *cursor = context.CommandBuffer + 15;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTSPAN USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTSPAN ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTSPAN USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTSPAN TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 1024u) {
            context.PushLog("VFS DIGESTSPAN COUNT RANGE 1..1024");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestSpanByMountFn(mountToken,
                                               static_cast<uint32_t>(startBlock),
                                               static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigestwindow ")) {
        const char *cursor = context.CommandBuffer + 16;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTWINDOW USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTWINDOW ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTWINDOW USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTWINDOW TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 2048u) {
            context.PushLog("VFSDIGESTWINDOW COUNT RANGE 1..2048");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestWindowByMountFn(mountToken,
                                                 static_cast<uint32_t>(startBlock),
                                                 static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digestwindow ")) {
        const char *cursor = context.CommandBuffer + 17;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTWINDOW USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTWINDOW ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTWINDOW USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTWINDOW TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 2048u) {
            context.PushLog("VFS DIGESTWINDOW COUNT RANGE 1..2048");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestWindowByMountFn(mountToken,
                                                 static_cast<uint32_t>(startBlock),
                                                 static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigestruns ")) {
        const char *cursor = context.CommandBuffer + 14;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTRUNS USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTRUNS ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTRUNS USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTRUNS TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 4096u) {
            context.PushLog("VFSDIGESTRUNS COUNT RANGE 1..4096");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestRunsByMountFn(mountToken,
                                               static_cast<uint32_t>(startBlock),
                                               static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digestruns ")) {
        const char *cursor = context.CommandBuffer + 15;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTRUNS USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTRUNS ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTRUNS USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTRUNS TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 4096u) {
            context.PushLog("VFS DIGESTRUNS COUNT RANGE 1..4096");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestRunsByMountFn(mountToken,
                                               static_cast<uint32_t>(startBlock),
                                               static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigesttrans ")) {
        const char *cursor = context.CommandBuffer + 15;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTTRANS USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTTRANS ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTTRANS USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTTRANS TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 8192u) {
            context.PushLog("VFSDIGESTTRANS COUNT RANGE 1..8192");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestTransitionsByMountFn(mountToken,
                                                      static_cast<uint32_t>(startBlock),
                                                      static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digesttrans ")) {
        const char *cursor = context.CommandBuffer + 16;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTTRANS USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTTRANS ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTTRANS USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTTRANS TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 8192u) {
            context.PushLog("VFS DIGESTTRANS COUNT RANGE 1..8192");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestTransitionsByMountFn(mountToken,
                                                      static_cast<uint32_t>(startBlock),
                                                      static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigestdensity ")) {
        const char *cursor = context.CommandBuffer + 17;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTDENSITY USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTDENSITY ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTDENSITY USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTDENSITY TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 16384u) {
            context.PushLog("VFSDIGESTDENSITY COUNT RANGE 1..16384");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestDensityByMountFn(mountToken,
                                                  static_cast<uint32_t>(startBlock),
                                                  static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digestdensity ")) {
        const char *cursor = context.CommandBuffer + 18;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTDENSITY USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTDENSITY ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTDENSITY USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTDENSITY TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 16384u) {
            context.PushLog("VFS DIGESTDENSITY COUNT RANGE 1..16384");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestDensityByMountFn(mountToken,
                                                  static_cast<uint32_t>(startBlock),
                                                  static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigestratio ")) {
        const char *cursor = context.CommandBuffer + 15;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTRATIO USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTRATIO ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTRATIO USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTRATIO TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 32768u) {
            context.PushLog("VFSDIGESTRATIO COUNT RANGE 1..32768");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestRatioByMountFn(mountToken,
                                                static_cast<uint32_t>(startBlock),
                                                static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digestratio ")) {
        const char *cursor = context.CommandBuffer + 16;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTRATIO USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTRATIO ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTRATIO USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTRATIO TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 32768u) {
            context.PushLog("VFS DIGESTRATIO COUNT RANGE 1..32768");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestRatioByMountFn(mountToken,
                                                static_cast<uint32_t>(startBlock),
                                                static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigestbalance ")) {
        const char *cursor = context.CommandBuffer + 17;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTBALANCE USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTBALANCE ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTBALANCE USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTBALANCE TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 65536u) {
            context.PushLog("VFSDIGESTBALANCE COUNT RANGE 1..65536");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestBalanceByMountFn(mountToken,
                                                  static_cast<uint32_t>(startBlock),
                                                  static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digestbalance ")) {
        const char *cursor = context.CommandBuffer + 18;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTBALANCE USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTBALANCE ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTBALANCE USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTBALANCE TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 65536u) {
            context.PushLog("VFS DIGESTBALANCE COUNT RANGE 1..65536");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestBalanceByMountFn(mountToken,
                                                  static_cast<uint32_t>(startBlock),
                                                  static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigestskew ")) {
        const char *cursor = context.CommandBuffer + 14;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTSKEW USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTSKEW ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTSKEW USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTSKEW TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 131072u) {
            context.PushLog("VFSDIGESTSKEW COUNT RANGE 1..131072");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestSkewByMountFn(mountToken,
                                               static_cast<uint32_t>(startBlock),
                                               static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digestskew ")) {
        const char *cursor = context.CommandBuffer + 15;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTSKEW USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTSKEW ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTSKEW USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTSKEW TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 131072u) {
            context.PushLog("VFS DIGESTSKEW COUNT RANGE 1..131072");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestSkewByMountFn(mountToken,
                                               static_cast<uint32_t>(startBlock),
                                               static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigesttilt ")) {
        const char *cursor = context.CommandBuffer + 14;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTTILT USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTTILT ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTTILT USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTTILT TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 262144u) {
            context.PushLog("VFSDIGESTTILT COUNT RANGE 1..262144");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestTiltByMountFn(mountToken,
                                               static_cast<uint32_t>(startBlock),
                                               static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digesttilt ")) {
        const char *cursor = context.CommandBuffer + 15;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTTILT USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTTILT ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTTILT USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTTILT TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 262144u) {
            context.PushLog("VFS DIGESTTILT COUNT RANGE 1..262144");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestTiltByMountFn(mountToken,
                                               static_cast<uint32_t>(startBlock),
                                               static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsdigestbias ")) {
        const char *cursor = context.CommandBuffer + 14;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFSDIGESTBIAS USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTBIAS ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFSDIGESTBIAS USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSDIGESTBIAS TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 524288u) {
            context.PushLog("VFSDIGESTBIAS COUNT RANGE 1..524288");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestBiasByMountFn(mountToken,
                                               static_cast<uint32_t>(startBlock),
                                               static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs digestbias ")) {
        const char *cursor = context.CommandBuffer + 15;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("VFS DIGESTBIAS USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTBIAS ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("VFS DIGESTBIAS USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS DIGESTBIAS TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u || blockCount > 524288u) {
            context.PushLog("VFS DIGESTBIAS COUNT RANGE 1..524288");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunVfsBlockDigestBiasByMountFn(mountToken,
                                               static_cast<uint32_t>(startBlock),
                                               static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "logsavemount ")) {
        const char *cursor = context.CommandBuffer + 12;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("LOGSAVEMOUNT USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("LOGSAVEMOUNT ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("LOGSAVEMOUNT USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("LOGSAVEMOUNT TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u) {
            context.PushLog("LOGSAVEMOUNT COUNT RANGE 1..4096");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunLogSaveByMountFn(mountToken,
                                    static_cast<uint32_t>(startBlock),
                                    static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "log savemount ")) {
        const char *cursor = context.CommandBuffer + 13;
        char mountToken[32] = {};
        char startToken[24] = {};
        char countToken[24] = {};
        if (!context.ReadTokenFn(cursor, mountToken, sizeof(mountToken)) ||
            !context.ReadTokenFn(cursor, startToken, sizeof(startToken))) {
            context.PushLog("LOG SAVEMOUNT USAGE MOUNT START COUNT");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t startBlock = 0u;
        uint64_t blockCount = 0u;
        const bool hasCountToken = context.ReadTokenFn(cursor, countToken, sizeof(countToken));
        if (hasCountToken) {
            if (!context.ParseUIntFn(startToken, startBlock) || !context.ParseUIntFn(countToken, blockCount) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("LOG SAVEMOUNT ARG INVALID");
                context.ClearCommandInputFn();
                return true;
            }
        } else {
            if (!ParsePackedRangeToken(startToken, startBlock, blockCount, context.ParseUIntFn) ||
                startBlock > 0xFFFFFFFFu || blockCount > 0xFFFFFFFFu) {
                context.PushLog("LOG SAVEMOUNT USAGE MOUNT START COUNT");
                context.ClearCommandInputFn();
                return true;
            }
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("LOG SAVEMOUNT TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        if (blockCount == 0u) {
            context.PushLog("LOG SAVEMOUNT COUNT RANGE 1..4096");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunLogSaveByMountFn(mountToken,
                                    static_cast<uint32_t>(startBlock),
                                    static_cast<uint32_t>(blockCount));
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsbootblk/") ||
        context.StartsWithFn(context.CommandBuffer, "vfsbootblk:")) {
        uint64_t blockIndex = 0u;
        if (!context.ParseUIntFn(context.CommandBuffer + 11, blockIndex)) {
            context.PushLog("VFSBOOTBLK ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        char path[96] = "/boot/blk/";
        size_t pos = 10u;
        char digits[24] = {};
        size_t digitsCount = 0u;
        do {
            digits[digitsCount++] = static_cast<char>('0' + (blockIndex % 10u));
            blockIndex /= 10u;
        } while (blockIndex != 0u && digitsCount < (sizeof(digits) - 1u));

        for (size_t i = 0u; i < digitsCount; i++) {
            if (pos + 1u >= sizeof(path)) {
                break;
            }
            path[pos++] = digits[digitsCount - 1u - i];
        }
        path[pos] = '\0';

        context.RunVfsResolveFn(path);
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsbootblk ")) {

        uint64_t blockIndex = 0u;
        if (!context.ParseUIntFn(context.CommandBuffer + 11, blockIndex)) {
            context.PushLog("VFSBOOTBLK ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        char path[96] = "/boot/blk/";
        size_t pos = 10u;
        char digits[24] = {};
        size_t digitsCount = 0u;
        do {
            digits[digitsCount++] = static_cast<char>('0' + (blockIndex % 10u));
            blockIndex /= 10u;
        } while (blockIndex != 0u && digitsCount < (sizeof(digits) - 1u));

        for (size_t i = 0u; i < digitsCount; i++) {
            if (pos + 1u >= sizeof(path)) {
                break;
            }
            path[pos++] = digits[digitsCount - 1u - i];
        }
        path[pos] = '\0';

        context.RunVfsResolveFn(path);
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs bootblk/") ||
        context.StartsWithFn(context.CommandBuffer, "vfs bootblk:")) {
        uint64_t blockIndex = 0u;
        if (!context.ParseUIntFn(context.CommandBuffer + 12, blockIndex)) {
            context.PushLog("VFS BOOTBLK ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        char path[96] = "/boot/blk/";
        size_t pos = 10u;
        char digits[24] = {};
        size_t digitsCount = 0u;
        do {
            digits[digitsCount++] = static_cast<char>('0' + (blockIndex % 10u));
            blockIndex /= 10u;
        } while (blockIndex != 0u && digitsCount < (sizeof(digits) - 1u));

        for (size_t i = 0u; i < digitsCount; i++) {
            if (pos + 1u >= sizeof(path)) {
                break;
            }
            path[pos++] = digits[digitsCount - 1u - i];
        }
        path[pos] = '\0';

        context.RunVfsResolveFn(path);
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs bootblk ")) {

        uint64_t blockIndex = 0u;
        if (!context.ParseUIntFn(context.CommandBuffer + 12, blockIndex)) {
            context.PushLog("VFS BOOTBLK ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        char path[96] = "/boot/blk/";
        size_t pos = 10u;
        char digits[24] = {};
        size_t digitsCount = 0u;
        do {
            digits[digitsCount++] = static_cast<char>('0' + (blockIndex % 10u));
            blockIndex /= 10u;
        } while (blockIndex != 0u && digitsCount < (sizeof(digits) - 1u));

        for (size_t i = 0u; i < digitsCount; i++) {
            if (pos + 1u >= sizeof(path)) {
                break;
            }
            path[pos++] = digits[digitsCount - 1u - i];
        }
        path[pos] = '\0';

        context.RunVfsResolveFn(path);
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfsboot ")) {
        const char *cursor = context.CommandBuffer + 8;
        char leafToken[72] = {};
        if (!context.ReadTokenFn(cursor, leafToken, sizeof(leafToken))) {
            context.PushLog("USAGE: VFSBOOT NAME");
            context.ClearCommandInputFn();
            return true;
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFSBOOT TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        char path[96] = "/boot/";
        size_t pos = 6u;
        for (size_t i = 0u; leafToken[i] != '\0'; i++) {
            if (pos + 1u >= sizeof(path)) {
                break;
            }
            path[pos++] = leafToken[i];
        }
        path[pos] = '\0';

        context.RunVfsResolveFn(path);
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "vfs boot ")) {
        const char *cursor = context.CommandBuffer + 9;
        char leafToken[72] = {};
        if (!context.ReadTokenFn(cursor, leafToken, sizeof(leafToken))) {
            context.PushLog("VFS BOOT USAGE NAME");
            context.ClearCommandInputFn();
            return true;
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("VFS BOOT TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        char path[96] = "/boot/";
        size_t pos = 6u;
        for (size_t i = 0u; leafToken[i] != '\0'; i++) {
            if (pos + 1u >= sizeof(path)) {
                break;
            }
            path[pos++] = leafToken[i];
        }
        path[pos] = '\0';

        context.RunVfsResolveFn(path);
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "stats")) {
        context.RunStatsFn();
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "layers")) {
        context.RunRenderLayersFn();
        return true;
    }

    return false;
}

} // namespace Fortress::Kernel
