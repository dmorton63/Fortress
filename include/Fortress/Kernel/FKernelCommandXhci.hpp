#pragma once

#include <cstddef>
#include <cstdint>

namespace Fortress::Kernel {

struct FKernelXhciCommandContext {
    const char *CommandBuffer;
    size_t *CommandLength;

    bool *IntrinLoopBackgroundEnabled;
    bool *IntrinBackgroundHaveLastReport;
    uint32_t *IntrinBackgroundLastReportLength;
    uint32_t *IntrinLoopBackgroundTickCounter;
    uint8_t (*GetHidLogModeValueFn)();
    void (*SetHidLogModeValueFn)(uint8_t value);

    void (*PushLog)(const char *line);
    bool (*StrEqFn)(const char *left, const char *right);
    bool (*StartsWithFn)(const char *value, const char *prefix);
    bool (*ReadTokenFn)(const char *&cursor, char *out, size_t outSize);
    bool (*ParseU64AutoFn)(const char *value, uint64_t &out);

    void (*RunXhciRegisterAutoSnapshotFn)();
    void (*RunXhciRegisterSnapshotFromAddressFn)(uint64_t capabilityBaseVirtualAddress);
    void (*RunXhciAutoProbeFn)();
    void (*RunXhciInitFn)();
    void (*RunXhciRingTestFn)();
    void (*RunXhciEnumFn)();
    void (*RunXhciAddressDeviceFn)();
    void (*RunXhciGetDescriptorFn)();
    void (*RunXhciGetConfigDescriptorFn)();
    void (*RunXhciSetConfigurationFn)();
    void (*RunXhciConfigureInterruptEndpointFn)();
    void (*RunXhciInterruptInFn)(bool loopMode);
    void (*RunXhciMscStatusFn)();

    void (*ClearCommandInputFn)();
};

bool TryProcessXhciCommand(FKernelXhciCommandContext &context);

} // namespace Fortress::Kernel
