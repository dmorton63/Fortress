#include "Fortress/Kernel/FKernelCommandUtility.hpp"

namespace Fortress::Kernel {

bool TryProcessUtilityCommand(FKernelUtilityCommandContext &context) {
    if (context.CommandBuffer == nullptr || context.CommandLength == nullptr || context.PushLog == nullptr ||
        context.StrEqFn == nullptr || context.StartsWithFn == nullptr || context.ReadTokenFn == nullptr ||
        context.ParseUIntFn == nullptr || context.RunEventBurstFn == nullptr || context.RunVfsStatFn == nullptr ||
        context.RunVfsResolveFn == nullptr || context.SetHudLogShowTailFn == nullptr ||
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
