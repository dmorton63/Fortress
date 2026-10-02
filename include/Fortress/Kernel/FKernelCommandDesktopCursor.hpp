#pragma once

#include <cstddef>
#include <cstdint>

#include "Fortress/Kernel/FDesktopCompositor.hpp"
#include "Fortress/Kernel/FDesktopInputRouter.hpp"
#include "Fortress/Kernel/FDesktopSurfaceContentHost.hpp"

namespace Fortress::Kernel {

struct FKernelDesktopCursorCommandContext {
    char *CommandBuffer;
    size_t *CommandLength;

    FDesktopCompositor *DesktopCompositor;
    FDesktopInputRouter *DesktopInputRouter;
    FDesktopSurfaceContentHost *DesktopSurfaceContentHost;

    bool *CursorOverlayEnabled;
    bool *DesktopSurfaceOverlayEnabled;
    bool *CursorInvertX;
    bool *CursorInvertY;
    uint32_t *CursorSensitivityPercent;

    uint8_t (*GetCursorLatencyModeValueFn)();
    void (*SetCursorLatencyModeValueFn)(uint8_t value);
    const char *(*GetCursorLatencyModeNameFn)();

    void (*PushLogFn)(const char *line);
    void (*AppendCharFn)(char *dst, size_t dstSize, size_t &offset, char c);
    void (*AppendStringFn)(char *dst, size_t dstSize, size_t &offset, const char *src);
    void (*AppendUIntFn)(char *dst, size_t dstSize, size_t &offset, uint64_t value);

    bool (*StrEqFn)(const char *left, const char *right);
    bool (*StartsWithFn)(const char *value, const char *prefix);
    bool (*ParseUIntFn)(const char *value, uint64_t &out);
    bool (*ReadTokenFn)(const char *&cursor, char *out, size_t outSize);

    bool (*MatchAnyExactFn)(const char *value, const char *first, const char *second, const char *third);
    bool (*MatchAnyPrefixFn)(const char *value, const char *firstPrefix, const char *secondPrefix, const char *thirdPrefix);
    const char *(*AliasArgAfterPrefixFn)(const char *value,
                                         const char *firstPrefix,
                                         size_t firstPrefixLength,
                                         const char *secondPrefix,
                                         size_t secondPrefixLength,
                                         const char *thirdPrefix,
                                         size_t thirdPrefixLength);

    void (*PublishCursorOverlaySetEventFn)(bool enabled);
    void (*PublishDesktopSurfaceOverlaySetEventFn)(bool enabled);
};

bool TryProcessDesktopSurfaceCommand(FKernelDesktopCursorCommandContext &context);
bool TryProcessCursorCommand(FKernelDesktopCursorCommandContext &context);

} // namespace Fortress::Kernel
