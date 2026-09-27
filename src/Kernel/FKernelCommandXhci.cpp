#include "Fortress/Kernel/FKernelCommandXhci.hpp"

namespace Fortress::Kernel {

static constexpr uint8_t KHidLogModeCompact = 0u;
static constexpr uint8_t KHidLogModeCore = 1u;
static constexpr uint8_t KHidLogModeVerbose = 2u;

bool TryProcessXhciCommand(FKernelXhciCommandContext &context) {
    if (context.CommandBuffer == nullptr || context.CommandLength == nullptr || context.IntrinLoopBackgroundEnabled == nullptr ||
        context.IntrinBackgroundHaveLastReport == nullptr || context.IntrinBackgroundLastReportLength == nullptr ||
        context.IntrinLoopBackgroundTickCounter == nullptr || context.GetHidLogModeValueFn == nullptr ||
        context.SetHidLogModeValueFn == nullptr ||
        context.PushLog == nullptr || context.StrEqFn == nullptr || context.StartsWithFn == nullptr ||
        context.ReadTokenFn == nullptr || context.ParseU64AutoFn == nullptr || context.RunXhciRegisterAutoSnapshotFn == nullptr ||
        context.RunXhciRegisterSnapshotFromAddressFn == nullptr || context.RunXhciAutoProbeFn == nullptr ||
        context.RunXhciInitFn == nullptr || context.RunXhciRingTestFn == nullptr || context.RunXhciEnumFn == nullptr ||
        context.RunXhciAddressDeviceFn == nullptr || context.RunXhciGetDescriptorFn == nullptr ||
        context.RunXhciGetConfigDescriptorFn == nullptr || context.RunXhciSetConfigurationFn == nullptr ||
        context.RunXhciConfigureInterruptEndpointFn == nullptr || context.RunXhciInterruptInFn == nullptr ||
        context.RunXhciMscStatusFn == nullptr ||
        context.ClearCommandInputFn == nullptr) {
        return false;
    }

    if (context.StrEqFn(context.CommandBuffer, "xhcihid")) {
        if (context.GetHidLogModeValueFn() == KHidLogModeVerbose) {
            context.PushLog("XHCI HID LOG VERBOSE");
        } else if (context.GetHidLogModeValueFn() == KHidLogModeCore) {
            context.PushLog("XHCI HID LOG CORE");
        } else {
            context.PushLog("XHCI HID LOG COMPACT");
        }
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "xhcihid compact")) {
        context.SetHidLogModeValueFn(KHidLogModeCompact);
        context.PushLog("XHCI HID LOG COMPACT");
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "xhcihid core")) {
        context.SetHidLogModeValueFn(KHidLogModeCore);
        context.PushLog("XHCI HID LOG CORE");
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "xhcihid verbose")) {
        context.SetHidLogModeValueFn(KHidLogModeVerbose);
        context.PushLog("XHCI HID LOG VERBOSE");
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "xhciregs")) {
        context.RunXhciRegisterAutoSnapshotFn();
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "xhciregs ")) {
        const char *cursor = context.CommandBuffer + 9;
        char token0[32] = {};
        if (!context.ReadTokenFn(cursor, token0, sizeof(token0))) {
            context.PushLog("USAGE: XHCIREGS [CAPVA|PABASE]");
            context.ClearCommandInputFn();
            return true;
        }

        const char *valueToken = token0;
        char tokenVA[32] = {};
        if (context.StrEqFn(token0, "capva") || context.StrEqFn(token0, "CAPVA") || context.StrEqFn(token0, "va") ||
            context.StrEqFn(token0, "VA") || context.StrEqFn(token0, "pabase") || context.StrEqFn(token0, "PABASE") ||
            context.StrEqFn(token0, "pa") || context.StrEqFn(token0, "PA")) {
            if (!context.ReadTokenFn(cursor, tokenVA, sizeof(tokenVA))) {
                context.PushLog("USAGE: XHCIREGS [CAPVA|PABASE]");
                context.ClearCommandInputFn();
                return true;
            }
            valueToken = tokenVA;
        }

        char extraToken[8] = {};
        if (context.ReadTokenFn(cursor, extraToken, sizeof(extraToken))) {
            context.PushLog("XHCIREGS TOO MANY ARGS");
            context.ClearCommandInputFn();
            return true;
        }

        uint64_t capabilityBaseVirtualAddress = 0;
        if (!context.ParseU64AutoFn(valueToken, capabilityBaseVirtualAddress) || capabilityBaseVirtualAddress == 0) {
            context.PushLog("XHCIREGS ARG INVALID");
            context.ClearCommandInputFn();
            return true;
        }

        context.RunXhciRegisterSnapshotFromAddressFn(capabilityBaseVirtualAddress);
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "xhciprobe")) {
        context.RunXhciAutoProbeFn();
        return true;
    }
    if (context.StrEqFn(context.CommandBuffer, "xhciinit")) {
        context.RunXhciInitFn();
        return true;
    }
    if (context.StrEqFn(context.CommandBuffer, "xhciringtest")) {
        context.RunXhciRingTestFn();
        return true;
    }
    if (context.StrEqFn(context.CommandBuffer, "xhcienum")) {
        context.RunXhciEnumFn();
        return true;
    }
    if (context.StrEqFn(context.CommandBuffer, "xhciaddrdev")) {
        context.RunXhciAddressDeviceFn();
        return true;
    }
    if (context.StrEqFn(context.CommandBuffer, "xhcigetdesc")) {
        context.RunXhciGetDescriptorFn();
        return true;
    }
    if (context.StrEqFn(context.CommandBuffer, "xhcigetcfg")) {
        context.RunXhciGetConfigDescriptorFn();
        return true;
    }
    if (context.StrEqFn(context.CommandBuffer, "xhcisetcfg")) {
        context.RunXhciSetConfigurationFn();
        return true;
    }
    if (context.StrEqFn(context.CommandBuffer, "xhciepconf")) {
        context.RunXhciConfigureInterruptEndpointFn();
        return true;
    }
    if (context.StrEqFn(context.CommandBuffer, "xhciintrin")) {
        context.RunXhciInterruptInFn(false);
        return true;
    }
    if (context.StrEqFn(context.CommandBuffer, "xhciintrinloop")) {
        *context.IntrinLoopBackgroundEnabled = false;
        *context.IntrinBackgroundHaveLastReport = false;
        *context.IntrinBackgroundLastReportLength = 0u;
        context.RunXhciInterruptInFn(true);
        return true;
    }
    if (context.StrEqFn(context.CommandBuffer, "xhciintrinbg")) {
        if (!*context.IntrinLoopBackgroundEnabled) {
            *context.IntrinLoopBackgroundEnabled = true;
            *context.IntrinLoopBackgroundTickCounter = 0u;
            *context.IntrinBackgroundHaveLastReport = false;
            *context.IntrinBackgroundLastReportLength = 0u;
            context.PushLog("XHCI INTRIN BG ON");
        } else {
            context.PushLog("XHCI INTRIN BG ALREADY ON");
        }
        return true;
    }
    if (context.StrEqFn(context.CommandBuffer, "xhciintrinstop")) {
        if (!*context.IntrinLoopBackgroundEnabled) {
            context.PushLog("XHCI INTRIN BG OFF");
        } else {
            *context.IntrinLoopBackgroundEnabled = false;
            *context.IntrinBackgroundHaveLastReport = false;
            *context.IntrinBackgroundLastReportLength = 0u;
            context.PushLog("XHCI INTRIN BG STOPPING");
        }
        return true;
    }
    if (context.StrEqFn(context.CommandBuffer, "xhciintrinstatus")) {
        context.PushLog(*context.IntrinLoopBackgroundEnabled ? "XHCI INTRIN BG ON" : "XHCI INTRIN BG OFF");
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "xhcimscstatus")) {
        context.RunXhciMscStatusFn();
        return true;
    }

    return false;
}

} // namespace Fortress::Kernel
