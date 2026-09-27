#include "Fortress/Kernel/FKernelCommandRuntimeControl.hpp"

namespace Fortress::Kernel {

static bool StrEq(const char *left, const char *right) {
    if (left == nullptr || right == nullptr) {
        return false;
    }

    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return false;
        }
        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

bool TryProcessRuntimeControlCommand(FKernelRuntimeControlCommandContext &context) {
    if (context.CommandBuffer == nullptr || context.WireframeEnabled == nullptr || context.ScenePaused == nullptr ||
        context.PublishWireframeSetEvent == nullptr || context.PublishScenePauseSetEvent == nullptr ||
        context.PushLog == nullptr || context.IsParallelWorkersSupported == nullptr ||
        context.IsParallelWorkersEnabled == nullptr || context.SetParallelWorkersEnabled == nullptr ||
        context.IsParallelDrainEnabled == nullptr || context.AreParallelDispatchCallbacksEnabled == nullptr ||
        context.SetParallelDrainEnabled == nullptr ||
        context.StartsWithFn == nullptr || context.ParseUIntFn == nullptr || context.RunParallelTestFn == nullptr ||
        context.RunParallelCanaryFn == nullptr || context.RunParallelProbeFn == nullptr ||
        context.RequestPlatformShutdown == nullptr ||
        context.HaltCpuForever == nullptr) {
        return false;
    }

    if (StrEq(context.CommandBuffer, "parallel")) {
        if (!context.IsParallelWorkersSupported()) {
            context.PushLog("PARALLEL UNSUPPORTED");
        } else {
            if (!context.IsParallelWorkersEnabled()) {
                context.PushLog("PARALLEL OFF");
            } else {
                if (!context.IsParallelDrainEnabled()) {
                    context.PushLog("PARALLEL ON AP_PARKED");
                } else if (context.AreParallelDispatchCallbacksEnabled()) {
                    context.PushLog("PARALLEL ON AP_DRAIN");
                } else {
                    context.PushLog("PARALLEL ON AP_PROBE");
                }
            }
        }
        return true;
    }

    if (StrEq(context.CommandBuffer, "parallel drain")) {
        context.PushLog(context.IsParallelDrainEnabled() ? "PARALLEL DRAIN ON" : "PARALLEL DRAIN OFF");
        return true;
    }

    if (StrEq(context.CommandBuffer, "parallel drain on")) {
        if (!context.SetParallelDrainEnabled(true)) {
            context.PushLog("PARALLEL DRAIN BLOCKED");
            return true;
        }

        context.PushLog("PARALLEL DRAIN ON");
        return true;
    }

    if (StrEq(context.CommandBuffer, "parallel drain off")) {
        if (!context.SetParallelDrainEnabled(false)) {
            context.PushLog("PARALLEL DRAIN DISABLE FAIL");
            return true;
        }

        context.PushLog("PARALLEL DRAIN OFF");
        return true;
    }

    if (StrEq(context.CommandBuffer, "parallel on")) {
        if (!context.IsParallelWorkersSupported()) {
            context.PushLog("PARALLEL UNSUPPORTED");
            return true;
        }

        if (!context.SetParallelWorkersEnabled(true)) {
            context.PushLog("PARALLEL ENABLE FAIL");
            return true;
        }

        if (!context.IsParallelDrainEnabled()) {
            context.PushLog("PARALLEL ON AP_PARKED");
        } else if (context.AreParallelDispatchCallbacksEnabled()) {
            context.PushLog("PARALLEL ON AP_DRAIN");
        } else {
            context.PushLog("PARALLEL ON AP_PROBE");
        }
        return true;
    }

    if (StrEq(context.CommandBuffer, "parallel off")) {
        if (!context.SetParallelWorkersEnabled(false)) {
            context.PushLog("PARALLEL DISABLE FAIL");
            return true;
        }

        context.PushLog("PARALLEL OFF");
        return true;
    }

    if (StrEq(context.CommandBuffer, "paralleltest") || context.StartsWithFn(context.CommandBuffer, "paralleltest ")) {
        if (!context.IsParallelWorkersSupported()) {
            context.PushLog("PARALLELTEST UNSUPPORTED");
            return true;
        }

        Fortress::Core::uint64 requestedItems = 2048u;
        if (context.StartsWithFn(context.CommandBuffer, "paralleltest ")) {
            if (!context.ParseUIntFn(context.CommandBuffer + 13, requestedItems)) {
                context.PushLog("PARALLELTEST ARG INVALID");
                return true;
            }
        }

        if (requestedItems == 0u || requestedItems > 100000u) {
            context.PushLog("PARALLELTEST RANGE 1..100000");
            return true;
        }

        context.RunParallelTestFn(static_cast<Fortress::Core::uint32>(requestedItems));
        return true;
    }

    if (StrEq(context.CommandBuffer, "parallelcanary") || context.StartsWithFn(context.CommandBuffer, "parallelcanary ")) {
        if (!context.IsParallelWorkersSupported()) {
            context.PushLog("PARALLELCANARY UNSUPPORTED");
            return true;
        }

        Fortress::Core::uint64 requestedItems = 64u;
        if (context.StartsWithFn(context.CommandBuffer, "parallelcanary ")) {
            if (!context.ParseUIntFn(context.CommandBuffer + 15, requestedItems)) {
                context.PushLog("PARALLELCANARY ARG INVALID");
                return true;
            }
        }

        if (requestedItems == 0u || requestedItems > 100000u) {
            context.PushLog("PARALLELCANARY RANGE 1..100000");
            return true;
        }

        context.RunParallelCanaryFn(static_cast<Fortress::Core::uint32>(requestedItems));
        return true;
    }

    if (StrEq(context.CommandBuffer, "parallelprobe")) {
        context.RunParallelProbeFn();
        return true;
    }

    if (StrEq(context.CommandBuffer, "wire")) {
        *context.WireframeEnabled = !*context.WireframeEnabled;
        context.PublishWireframeSetEvent(*context.WireframeEnabled);
        context.PushLog(*context.WireframeEnabled ? "WIRE ON" : "WIRE OFF");
        return true;
    }

    if (StrEq(context.CommandBuffer, "wire on")) {
        *context.WireframeEnabled = true;
        context.PublishWireframeSetEvent(true);
        context.PushLog("WIRE ON");
        return true;
    }

    if (StrEq(context.CommandBuffer, "wire off")) {
        *context.WireframeEnabled = false;
        context.PublishWireframeSetEvent(false);
        context.PushLog("WIRE OFF");
        return true;
    }

    if (StrEq(context.CommandBuffer, "pause")) {
        *context.ScenePaused = true;
        context.PublishScenePauseSetEvent(true);
        context.PushLog("ANIMATION PAUSED");
        return true;
    }

    if (StrEq(context.CommandBuffer, "resume")) {
        *context.ScenePaused = false;
        context.PublishScenePauseSetEvent(false);
        context.PushLog("ANIMATION RUNNING");
        return true;
    }

    if (StrEq(context.CommandBuffer, "shutdown") || StrEq(context.CommandBuffer, "poweroff") ||
        StrEq(context.CommandBuffer, "halt") || StrEq(context.CommandBuffer, "ok")) {
        context.PushLog("SHUTDOWN REQUESTED");
        context.RequestPlatformShutdown();
        context.PushLog("SHUTDOWN FALLBACK HALT");
        context.HaltCpuForever();
        return true;
    }

    return false;
}

} // namespace Fortress::Kernel
