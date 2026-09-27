#include "Fortress/Kernel/FKernelFramePipeline.hpp"

#include "Fortress/Kernel/FKernelCommandConsole.hpp"
#include "Fortress/Kernel/FKernelCommandControlPlane.hpp"
#include "Fortress/Kernel/FKernelCubeScene.hpp"
#include "Fortress/Memory/FKernelHeap.hpp"
#include "Fortress/Memory/FMemoryArena.hpp"
#include "Fortress/Memory/FPhysicalMemoryManager.hpp"
#include "Fortress/Memory/FVirtualMemoryManager.hpp"
#include "Fortress/Video/FFontManager.hpp"
#include "Fortress/Video/FDisplayManager.hpp"
#include "Fortress/Video/FTextRenderer.hpp"
#include "Fortress/Video/FVideoSurface.hpp"

namespace Fortress::Kernel {

namespace {

struct FFramePassContext {
    FKernelRuntimeContext &Runtime;
    Fortress::Video::FDisplayFrameContext &FrameContext;
    const FFrameRenderOptions &Options;
    const Fortress::Memory::FPhysicalMemoryStats &PmmStats;
    const Fortress::Memory::FVirtualMemoryStats &VmmStats;
    const Fortress::Memory::FKernelHeapStats &HeapStats;
    Fortress::Core::uint64 ArenaUsedBytes;
    FDesktopRuntime &DesktopRuntime;
};

enum class EFramePassId : Fortress::Core::uint8 {
    Clear = 0,
    Scene,
    DesktopSurfaces,
    Hud,
    InputPulse,
    CursorOverlay,
};

using FFramePassPredicate = bool (*)(const FFramePassContext &context);
using FFramePassExecute = void (*)(const FFramePassContext &context);

struct FFramePassDesc {
    EFramePassId Id;
    FFramePassPredicate ShouldRun;
    FFramePassExecute Execute;
};

static Fortress::Core::int32 ClampInt32(Fortress::Core::int32 value,
                                        Fortress::Core::int32 minValue,
                                        Fortress::Core::int32 maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

static bool NormalizeToScreenCoordinates(const Fortress::Video::FDisplayMode &mode,
                                         Fortress::Core::int32 normX,
                                         Fortress::Core::int32 normY,
                                         Fortress::Core::int32 &outX,
                                         Fortress::Core::int32 &outY) {
    if (mode.Width == 0 || mode.Height == 0) {
        return false;
    }

    normX = ClampInt32(normX, 0, 1000);
    normY = ClampInt32(normY, 0, 1000);

    const Fortress::Core::int32 maxX = static_cast<Fortress::Core::int32>(mode.Width - 1);
    const Fortress::Core::int32 maxY = static_cast<Fortress::Core::int32>(mode.Height - 1);
    outX = (normX * maxX) / 1000;
    outY = (normY * maxY) / 1000;
    return true;
}

static void DrawFilledRect(const Fortress::Video::FVideoSurfaceView &surface,
                           Fortress::Core::int32 x,
                           Fortress::Core::int32 y,
                           Fortress::Core::int32 width,
                           Fortress::Core::int32 height,
                           Fortress::Video::FColor color) {
    if (!surface.IsValid() || width <= 0 || height <= 0 ||
        surface.Desc.PixelFormat != Fortress::Video::EPixelFormat::Masked32) {
        return;
    }

    const Fortress::Core::uint32 packed = Fortress::Video::FVideoSurfaceOps::PackMasked32(color, surface.Desc.PixelMask);
    for (Fortress::Core::int32 py = 0; py < height; py++) {
        for (Fortress::Core::int32 px = 0; px < width; px++) {
            Fortress::Video::FVideoSurfaceOps::DrawPixel32(surface, x + px, y + py, packed);
        }
    }
}

static void DrawVerticalGradientRect(const Fortress::Video::FVideoSurfaceView &surface,
                                                                         Fortress::Core::int32 x,
                                                                         Fortress::Core::int32 y,
                                                                         Fortress::Core::int32 width,
                                                                         Fortress::Core::int32 height,
                                                                         Fortress::Video::FColor topColor,
                                                                         Fortress::Video::FColor bottomColor) {
        if (!surface.IsValid() || width <= 0 || height <= 0 ||
                surface.Desc.PixelFormat != Fortress::Video::EPixelFormat::Masked32) {
                return;
        }

        const Fortress::Core::int32 denom = (height > 1) ? (height - 1) : 1;
        for (Fortress::Core::int32 py = 0; py < height; py++) {
                const Fortress::Core::int32 r = static_cast<Fortress::Core::int32>(topColor.R) +
                                                                                ((static_cast<Fortress::Core::int32>(bottomColor.R) -
                                                                                    static_cast<Fortress::Core::int32>(topColor.R)) * py) /
                                                                                        denom;
                const Fortress::Core::int32 g = static_cast<Fortress::Core::int32>(topColor.G) +
                                                                                ((static_cast<Fortress::Core::int32>(bottomColor.G) -
                                                                                    static_cast<Fortress::Core::int32>(topColor.G)) * py) /
                                                                                        denom;
                const Fortress::Core::int32 b = static_cast<Fortress::Core::int32>(topColor.B) +
                                                                                ((static_cast<Fortress::Core::int32>(bottomColor.B) -
                                                                                    static_cast<Fortress::Core::int32>(topColor.B)) * py) /
                                                                                        denom;
                const Fortress::Core::uint32 packed = Fortress::Video::FVideoSurfaceOps::PackMasked32(
                        Fortress::Video::FColor::RGB(static_cast<Fortress::Core::uint8>(r),
                                                                                 static_cast<Fortress::Core::uint8>(g),
                                                                                 static_cast<Fortress::Core::uint8>(b)),
                        surface.Desc.PixelMask);

                for (Fortress::Core::int32 px = 0; px < width; px++) {
                        Fortress::Video::FVideoSurfaceOps::DrawPixel32(surface, x + px, y + py, packed);
                }
        }
}

static Fortress::Video::FColor SampleVerticalGradientColor(Fortress::Video::FColor topColor,
                                                                                                                     Fortress::Video::FColor bottomColor,
                                                                                                                     Fortress::Core::int32 height,
                                                                                                                     Fortress::Core::int32 yOffset) {
        if (height <= 1) {
                return topColor;
        }

        const Fortress::Core::int32 clampedOffset = ClampInt32(yOffset, 0, height - 1);
        const Fortress::Core::int32 denom = height - 1;
        const Fortress::Core::int32 r = static_cast<Fortress::Core::int32>(topColor.R) +
                                                                        ((static_cast<Fortress::Core::int32>(bottomColor.R) -
                                                                            static_cast<Fortress::Core::int32>(topColor.R)) *
                                                                         clampedOffset) /
                                                                                denom;
        const Fortress::Core::int32 g = static_cast<Fortress::Core::int32>(topColor.G) +
                                                                        ((static_cast<Fortress::Core::int32>(bottomColor.G) -
                                                                            static_cast<Fortress::Core::int32>(topColor.G)) *
                                                                         clampedOffset) /
                                                                                denom;
        const Fortress::Core::int32 b = static_cast<Fortress::Core::int32>(topColor.B) +
                                                                        ((static_cast<Fortress::Core::int32>(bottomColor.B) -
                                                                            static_cast<Fortress::Core::int32>(topColor.B)) *
                                                                         clampedOffset) /
                                                                                denom;

        return Fortress::Video::FColor::RGB(static_cast<Fortress::Core::uint8>(r),
                                                                                 static_cast<Fortress::Core::uint8>(g),
                                                                                 static_cast<Fortress::Core::uint8>(b));
}

static bool StartsWith(const char *value, const char *prefix) {
    if (value == nullptr || prefix == nullptr) {
        return false;
    }

    Fortress::Core::usize i = 0;
    while (prefix[i] != '\0') {
        if (value[i] != prefix[i]) {
            return false;
        }
        i++;
    }

    return true;
}

static bool ContainsSubstring(const char *value, const char *needle) {
    if (value == nullptr || needle == nullptr || needle[0] == '\0') {
        return false;
    }

    for (Fortress::Core::usize i = 0u; value[i] != '\0'; i++) {
        Fortress::Core::usize j = 0u;
        while (needle[j] != '\0' && value[i + j] != '\0' && value[i + j] == needle[j]) {
            j++;
        }
        if (needle[j] == '\0') {
            return true;
        }
    }

    return false;
}

static bool IsIssueLogLine(const char *line) {
    return ContainsSubstring(line, "FAIL") || ContainsSubstring(line, "ERR") ||
           ContainsSubstring(line, "INVALID") || ContainsSubstring(line, "PANIC") ||
           ContainsSubstring(line, "TIMEOUT");
}

static bool IsWarnLogLine(const char *line) {
    return ContainsSubstring(line, "WARN") || ContainsSubstring(line, "DEGRADED") ||
           ContainsSubstring(line, "RETRY") || ContainsSubstring(line, "FALLBACK");
}

static bool IsTerminalNoiseLine(const char *line) {
    if (line == nullptr || line[0] == '\0') {
        return false;
    }

    return StartsWith(line, "EVENT HEARTBEAT") ||
           StartsWith(line, "EVENT CMD RX") ||
           StartsWith(line, "EVENT STATS PUB") ||
           StartsWith(line, "TEXT CACHE") ||
           StartsWith(line, "TIMER PIT INIT") ||
           StartsWith(line, "IRQ REG ") ||
           StartsWith(line, "CDISP CORE ") ||
           StartsWith(line, "CDISP EXE CORE") ||
           StartsWith(line, "APW START ");
}

static void CopyString(char *dst, Fortress::Core::usize dstSize, const char *src) {
    if (dst == nullptr || dstSize == 0u) {
        return;
    }

    Fortress::Core::usize i = 0;
    for (; i + 1u < dstSize && src != nullptr && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static Fortress::Core::usize StringLength(const char *value) {
    if (value == nullptr) {
        return 0u;
    }

    Fortress::Core::usize length = 0u;
    while (value[length] != '\0') {
        length++;
    }

    return length;
}

static void BuildInputDisplayLine(const char *commandBuffer,
                                  Fortress::Core::usize maxVisibleChars,
                                  bool caretVisible,
                                  char *outBuffer,
                                  Fortress::Core::usize outBufferSize) {
    if (outBuffer == nullptr || outBufferSize == 0u || maxVisibleChars == 0u) {
        return;
    }

    Fortress::Core::usize writePos = 0u;
    const Fortress::Core::usize textLimit = (maxVisibleChars > 0u) ? (maxVisibleChars - 1u) : 0u;
    const Fortress::Core::usize commandLength = StringLength(commandBuffer);

    if (commandLength == 0u) {
        if (textLimit > 0u) {
            outBuffer[writePos++] = '_';
        }
    } else if (textLimit > 0u) {
        Fortress::Core::usize sourceStart = 0u;
        Fortress::Core::usize copyCount = commandLength;
        bool addEllipsis = false;
        if (copyCount > textLimit) {
            copyCount = textLimit;
            sourceStart = commandLength - copyCount;
            addEllipsis = (textLimit >= 4u);
        }

        if (addEllipsis) {
            outBuffer[writePos++] = '.';
            outBuffer[writePos++] = '.';
            outBuffer[writePos++] = '.';
            sourceStart = commandLength - (textLimit - 3u);
            copyCount = textLimit - 3u;
        }

        for (Fortress::Core::usize i = 0u; i < copyCount && (writePos + 1u) < outBufferSize; i++) {
            outBuffer[writePos++] = commandBuffer[sourceStart + i];
        }
    }

    if ((writePos + 1u) < outBufferSize) {
        outBuffer[writePos++] = caretVisible ? '|' : ' ';
    }
    outBuffer[writePos] = '\0';
}

static bool FindLatestLogLineByPrefix(const char *prefix, char *outLine, Fortress::Core::usize outSize) {
    if (prefix == nullptr || outLine == nullptr || outSize == 0u) {
        return false;
    }

    const Fortress::Core::usize logCount = FKernelCommandConsole::GetLogCount();
    for (Fortress::Core::usize i = logCount; i > 0u; i--) {
        const char *line = FKernelCommandConsole::GetLogLine(i - 1u);
        if (StartsWith(line, prefix)) {
            CopyString(outLine, outSize, line);
            return true;
        }
    }

    return false;
}

static void CapturePersistentSummaryLine(const char *prefix,
                                         char *cachedLine,
                                         Fortress::Core::usize cachedLineSize) {
    if (prefix == nullptr || cachedLine == nullptr || cachedLineSize == 0u) {
        return;
    }

    char latestLine[96] = {};
    if (FindLatestLogLineByPrefix(prefix, latestLine, sizeof(latestLine))) {
        CopyString(cachedLine, cachedLineSize, latestLine);
    }
}

static void DrawTextLine(const Fortress::Video::FVideoSurfaceView &surface,
                         Fortress::Video::FFontManager &font,
                         const char *line,
                         Fortress::Core::int32 scale,
                         Fortress::Video::FColor fg,
                         Fortress::Video::FColor bg,
                         Fortress::Core::int32 x,
                         Fortress::Core::int32 y) {
    Fortress::Core::int32 cursorX = x;
    Fortress::Core::int32 cursorY = y;
    (void)Fortress::Video::FTextRenderer::DrawText(surface, font, line, scale, fg, bg, cursorX, cursorY);
}

static void DrawVistaButton(const Fortress::Video::FVideoSurfaceView &surface,
                            Fortress::Video::FFontManager &font,
                            const char *label,
                            Fortress::Core::int32 x,
                            Fortress::Core::int32 y,
                            Fortress::Core::int32 width,
                            Fortress::Core::int32 height,
                            Fortress::Core::int32 scale) {
    if (width < 24 || height < 16) {
        return;
    }

    // Drop shadow and dark frame.
    DrawFilledRect(surface, x + 2, y + 2, width, height, Fortress::Video::FColor::RGB(10, 20, 36));
    DrawFilledRect(surface, x, y, width, height, Fortress::Video::FColor::RGB(34, 68, 116));
    DrawFilledRect(surface, x + 1, y + 1, width - 2, height - 2, Fortress::Video::FColor::RGB(102, 154, 220));

    const Fortress::Core::int32 innerX = x + 2;
    const Fortress::Core::int32 innerY = y + 2;
    const Fortress::Core::int32 innerW = width - 4;
    const Fortress::Core::int32 innerH = height - 4;
    const Fortress::Core::int32 topH = innerH / 2;
    const Fortress::Core::int32 bottomH = innerH - topH;

    // Vista-like stacked gradients: bright upper glass + deeper lower body.
    DrawVerticalGradientRect(surface,
                             innerX,
                             innerY,
                             innerW,
                             topH,
                             Fortress::Video::FColor::RGB(246, 252, 255),
                             Fortress::Video::FColor::RGB(176, 216, 252));
    DrawVerticalGradientRect(surface,
                             innerX,
                             innerY + topH,
                             innerW,
                             bottomH,
                             Fortress::Video::FColor::RGB(106, 166, 236),
                             Fortress::Video::FColor::RGB(56, 118, 194));

    // High-gloss top stripe plus a subtle secondary shine.
    const Fortress::Core::int32 glossH = topH / 2;
    if (glossH > 1) {
        DrawVerticalGradientRect(surface,
                                 innerX + 1,
                                 innerY + 1,
                                 innerW - 2,
                                 glossH,
                                 Fortress::Video::FColor::RGB(255, 255, 255),
                                 Fortress::Video::FColor::RGB(210, 232, 255));

        DrawVerticalGradientRect(surface,
                                 innerX + 2,
                                 innerY + glossH,
                                 innerW - 4,
                                 2,
                                 Fortress::Video::FColor::RGB(214, 236, 255),
                                 Fortress::Video::FColor::RGB(176, 212, 252));
    }

    // Top rim highlight and lower edge shade to increase glass depth cues.
    DrawFilledRect(surface, innerX + 1, innerY + 1, innerW - 2, 1, Fortress::Video::FColor::RGB(255, 255, 255));
    DrawFilledRect(surface,
                   innerX + 1,
                   innerY + innerH - 2,
                   innerW - 2,
                   1,
                   Fortress::Video::FColor::RGB(44, 96, 166));

    const Fortress::Core::usize labelLen = StringLength(label);
    const Fortress::Core::int32 glyphAdvance = Fortress::Video::FTextRenderer::GetGlyphAdvance(scale);
    const Fortress::Core::int32 lineAdvance = Fortress::Video::FTextRenderer::GetLineAdvance(scale);
    const Fortress::Core::int32 textWidth = static_cast<Fortress::Core::int32>(labelLen) * glyphAdvance;
    const Fortress::Core::int32 textX = x + (width - textWidth) / 2;
    const Fortress::Core::int32 textY = y + (height - lineAdvance) / 2;

    const Fortress::Video::FColor textBg = SampleVerticalGradientColor(Fortress::Video::FColor::RGB(176, 216, 252),
                                                                       Fortress::Video::FColor::RGB(92, 146, 220),
                                                                       innerH,
                                                                       (textY - innerY) + (lineAdvance / 2));
    DrawTextLine(surface,
                 font,
                 label,
                 scale,
                 Fortress::Video::FColor::RGB(246, 252, 255),
                 textBg,
                 textX + 1,
                 textY + 1);

    DrawTextLine(surface,
                 font,
                 label,
                 scale,
                 Fortress::Video::FColor::RGB(24, 54, 92),
                 textBg,
                 textX,
                 textY);
}

static void DrawCursorOverlayAndTarget(const Fortress::Video::FDisplayFrameContext &frameContext,
                                       const bool consumeButtonEdges) {
    if (!frameContext.IsValid()) {
        return;
    }

    Fortress::Core::int32 hidNormX = 500;
    Fortress::Core::int32 hidNormY = 500;
    if (!FKernelCommandConsole::GetHidCursorNormalized(hidNormX, hidNormY)) {
        return;
    }

    Fortress::Core::int32 cursorX = 0;
    Fortress::Core::int32 cursorY = 0;
    if (!NormalizeToScreenCoordinates(frameContext.Mode, hidNormX, hidNormY, cursorX, cursorY)) {
        return;
    }

    const auto &surface = frameContext.BackSurface;
    const Fortress::Core::int32 markerHalf = 4;
    DrawFilledRect(surface,
                   cursorX - markerHalf,
                   cursorY - markerHalf,
                   markerHalf * 2 + 1,
                   markerHalf * 2 + 1,
                   Fortress::Video::FColor::RGB(255, 255, 255));
    DrawFilledRect(surface,
                   cursorX - 1,
                   cursorY - 1,
                   3,
                   3,
                   Fortress::Video::FColor::RGB(20, 30, 40));

    bool leftPress = false;
    bool rightPress = false;
    bool middlePress = false;
    if (consumeButtonEdges) {
        (void)FKernelCommandConsole::ConsumeHidButtonPressEdges(leftPress, rightPress, middlePress);
    }

    if (leftPress || rightPress || middlePress) {
        const Fortress::Core::int32 pulseSize = 14;
        DrawFilledRect(surface,
                       cursorX - pulseSize / 2,
                       cursorY - pulseSize / 2,
                       pulseSize,
                       pulseSize,
                       Fortress::Video::FColor::RGB(255, 200, 90));
    }
}

static bool ShouldRunClearPass(const FFramePassContext &context) {
    return context.FrameContext.IsValid();
}

static void ExecuteClearPass(const FFramePassContext &context) {
    const Fortress::Video::FVideoSurfaceView &surface = context.FrameContext.BackSurface;
    const Fortress::Core::uint32 packed = Fortress::Video::FVideoSurfaceOps::PackMasked32(
        Fortress::Video::FColor::RGB(6, 8, 12), surface.Desc.PixelMask);
    Fortress::Video::FVideoSurfaceOps::Clear32(surface, packed);
}

static bool ShouldRunScenePass(const FFramePassContext &context) {
    return context.FrameContext.IsValid() && context.Options.RenderScene && context.Runtime.CubeScene != nullptr;
}

static void ExecuteScenePass(const FFramePassContext &context) {
    context.Runtime.CubeScene->Render(context.FrameContext.BackSurface, context.Options.WireframeEnabled);
}

static bool ShouldRunDesktopSurfacesPass(const FFramePassContext &context) {
    return context.FrameContext.IsValid() && context.Options.RenderDesktopSurfaces;
}

static void ExecuteDesktopSurfacesPass(const FFramePassContext &context) {
    if (!context.DesktopRuntime.IsReady() || !context.DesktopRuntime.GetOverlay().IsReady()) {
        return;
    }

    context.DesktopRuntime.GetOverlay().Render(context.FrameContext.BackSurface);
}

static void ExecuteHudPass(const FFramePassContext &context) {
    const Fortress::Core::int32 panelX = 8;
    const Fortress::Core::int32 panelY = 8;
    const Fortress::Core::int32 panelWidth =
        static_cast<Fortress::Core::int32>(context.FrameContext.BackSurface.Desc.Width) - (panelX * 2);
    const Fortress::Core::int32 panelHeight =
        static_cast<Fortress::Core::int32>(context.FrameContext.BackSurface.Desc.Height) - (panelY * 2);

    if (panelWidth <= 0 || panelHeight <= 0) {
        return;
    }

    const Fortress::Core::int32 textX = panelX + 12;
    const Fortress::Core::int32 separatorX = panelX + 8;
    const Fortress::Core::int32 separatorWidth = panelWidth - 16;
    const Fortress::Core::int32 panelBottomY = panelY + panelHeight - 10;
    const Fortress::Video::FColor panelTopColor = Fortress::Video::FColor::RGB(10, 50, 255);
    const Fortress::Video::FColor panelBottomColor = Fortress::Video::FColor::RGB(255, 30, 20);

    DrawVerticalGradientRect(context.FrameContext.BackSurface,
                             panelX,
                             panelY,
                             panelWidth,
                             panelHeight,
                             panelTopColor,
                             panelBottomColor);

    static Fortress::Video::FFontManager GFallbackHudFont = {};
    static bool GFallbackHudFontReady = false;
    if (!GFallbackHudFontReady) {
        GFallbackHudFontReady = GFallbackHudFont.Initialize();
    }

    const auto DrawHudTextLine = [&](const char *line, Fortress::Video::FColor fg, Fortress::Core::int32 y) {
        const Fortress::Video::FColor bg =
            SampleVerticalGradientColor(panelTopColor, panelBottomColor, panelHeight, y - panelY);
        DrawTextLine(context.FrameContext.BackSurface, GFallbackHudFont, line, 2, fg, bg, textX, y);
    };

    if (context.Runtime.Console == nullptr) {
        if (GFallbackHudFontReady) {
            DrawHudTextLine("FORTRESS HUD", Fortress::Video::FColor::RGB(255, 255, 210), panelY + 12);
            DrawHudTextLine("CONSOLE UNAVAILABLE",
                            Fortress::Video::FColor::RGB(255, 150, 120),
                            panelY + 12 + Fortress::Video::FTextRenderer::GetLineAdvance(2));
        }
        return;
    }

    if (!GFallbackHudFontReady) {
        return;
    }

    // Render a direct command-log fallback last so runtime messages persist on screen.
    Fortress::Core::int32 cursorY = panelY + 12;
    const Fortress::Core::int32 lineAdvance = Fortress::Video::FTextRenderer::GetLineAdvance(2);
    const Fortress::Core::int32 glyphAdvance = Fortress::Video::FTextRenderer::GetGlyphAdvance(2);
    const Fortress::Core::int32 inputLineY = panelBottomY - lineAdvance;
    const Fortress::Core::int32 pinnedLogHintY = inputLineY - lineAdvance - 6;
    const Fortress::Core::int32 logBottomY = pinnedLogHintY - 8;

    if (FKernelCommandConsole::IsTerminalModeEnabled()) {
        DrawHudTextLine("FORTRESS TERMINAL", Fortress::Video::FColor::RGB(255, 255, 210), cursorY);
        cursorY += lineAdvance;

        DrawHudTextLine("TYPE HELP FOR COMMANDS", Fortress::Video::FColor::RGB(210, 255, 220), cursorY);
        cursorY += lineAdvance;

        DrawFilledRect(context.FrameContext.BackSurface,
                       separatorX,
                       cursorY,
                       separatorWidth,
                       2,
                       Fortress::Video::FColor::RGB(40, 80, 120));

        cursorY += lineAdvance;
        const FKernelCommandConsole::EHudLogViewMode logViewMode = FKernelCommandConsole::GetHudLogViewMode();
        const Fortress::Core::usize logCount =
            (logViewMode == FKernelCommandConsole::EHudLogViewMode::BootLog) ? FKernelCommandConsole::GetBootLogCount()
                                                                              : FKernelCommandConsole::GetLogCount();
        const Fortress::Core::usize maxVisibleLines =
            (cursorY <= logBottomY) ? static_cast<Fortress::Core::usize>((logBottomY - cursorY) / lineAdvance + 1) : 0u;

        Fortress::Core::usize visibleIndexes[256] = {};
        Fortress::Core::usize visibleCount = 0u;
        for (Fortress::Core::usize i = 0u; i < logCount && visibleCount < 256u; i++) {
            const char *line = (logViewMode == FKernelCommandConsole::EHudLogViewMode::BootLog)
                                   ? FKernelCommandConsole::GetBootLogLine(i)
                                   : FKernelCommandConsole::GetLogLine(i);
            if (!IsTerminalNoiseLine(line)) {
                visibleIndexes[visibleCount++] = i;
            }
        }

        Fortress::Core::usize startVisibleIndex = 0u;
        if (visibleCount > maxVisibleLines) {
            startVisibleIndex = visibleCount - maxVisibleLines;
        }

        Fortress::Core::int32 drawY = cursorY;
        for (Fortress::Core::usize vi = startVisibleIndex; vi < visibleCount; vi++) {
            const Fortress::Core::usize i = visibleIndexes[vi];
            const char *line = (logViewMode == FKernelCommandConsole::EHudLogViewMode::BootLog)
                                   ? FKernelCommandConsole::GetBootLogLine(i)
                                   : FKernelCommandConsole::GetLogLine(i);
            DrawHudTextLine(line, Fortress::Video::FColor::RGB(215, 230, 255), drawY);
            drawY += lineAdvance;
            if (drawY > logBottomY) {
                break;
            }
        }

        DrawHudTextLine("TERMINAL MODE ON  (TERMINAL OFF TO EXIT)", Fortress::Video::FColor::RGB(235, 230, 170), pinnedLogHintY);

        DrawFilledRect(context.FrameContext.BackSurface,
                       separatorX,
                       inputLineY - 4,
                       separatorWidth,
                       2,
                       Fortress::Video::FColor::RGB(100, 64, 40));

        const Fortress::Video::FColor inputBg =
            SampleVerticalGradientColor(panelTopColor, panelBottomColor, panelHeight, inputLineY - panelY);
        DrawTextLine(context.FrameContext.BackSurface,
                     GFallbackHudFont,
                     "INPUT> ",
                     2,
                     Fortress::Video::FColor::RGB(255, 240, 160),
                     inputBg,
                     textX,
                     inputLineY);

        const Fortress::Core::int32 okButtonWidth = 82;
        const Fortress::Core::int32 okButtonHeight = lineAdvance + 8;
        const Fortress::Core::int32 okButtonX = separatorX + separatorWidth - okButtonWidth - 6;
        const Fortress::Core::int32 okButtonY = inputLineY - 4;

        const Fortress::Core::int32 inputStartX = textX + (glyphAdvance * 7);
        Fortress::Core::int32 inputRightX = separatorX + separatorWidth - 6;
        const bool canRenderOkButton = (okButtonX - inputStartX) > (glyphAdvance * 8);
        if (canRenderOkButton) {
            inputRightX = okButtonX - 8;
        }
        const Fortress::Core::int32 inputWidthPixels = inputRightX - inputStartX;
        Fortress::Core::usize maxInputChars = 1u;
        if (inputWidthPixels > glyphAdvance) {
            maxInputChars = static_cast<Fortress::Core::usize>(inputWidthPixels / glyphAdvance);
        }

        static Fortress::Core::uint32 GInputCaretFrameCounter = 0u;
        GInputCaretFrameCounter++;
        const bool caretVisible = ((GInputCaretFrameCounter / 18u) % 2u) == 0u;

        char inputDisplayLine[96] = {};
        BuildInputDisplayLine(FKernelCommandConsole::GetCommandBuffer(),
                              maxInputChars,
                              caretVisible,
                              inputDisplayLine,
                              sizeof(inputDisplayLine));

        DrawTextLine(context.FrameContext.BackSurface,
                     GFallbackHudFont,
                     inputDisplayLine,
                     2,
                     Fortress::Video::FColor::RGB(255, 255, 230),
                     inputBg,
                     inputStartX,
                     inputLineY);

        if (canRenderOkButton) {
            DrawVistaButton(context.FrameContext.BackSurface,
                            GFallbackHudFont,
                            "OK",
                            okButtonX,
                            okButtonY,
                            okButtonWidth,
                            okButtonHeight,
                            2);
        }

        return;
    }

    DrawHudTextLine("FORTRESS HUD", Fortress::Video::FColor::RGB(255, 255, 210), cursorY);

    cursorY += lineAdvance;
    DrawHudTextLine("TYPE HELP FOR COMMANDS", Fortress::Video::FColor::RGB(210, 255, 220), cursorY);

    cursorY += lineAdvance;
        DrawFilledRect(context.FrameContext.BackSurface,
                                     separatorX,
                                     cursorY,
                                     separatorWidth,
                                     2,
                                     Fortress::Video::FColor::RGB(40, 80, 120));

    cursorY += 6;
    static char cachedRendererLine[96] = {};
    static char cachedDisplayModeLine[96] = {};
    static char cachedVfsLine[96] = {};
    static char cachedDesktopSmokeLine[96] = {};
    static char cachedDsksurfSmokeLine[96] = {};
    static char cachedDesktopStatsLine[96] = {};
    static char cachedCoreDispatchLine[196] = {};
    static char cachedCoreDispatchPerCoreLine[196] = {};
    static char cachedApWorkerLine[196] = {};
    static char cachedParallelProbeLine[196] = {};

    CapturePersistentSummaryLine("RENDERER BACKEND:", cachedRendererLine, sizeof(cachedRendererLine));
    CapturePersistentSummaryLine("DISPLAY MODE:", cachedDisplayModeLine, sizeof(cachedDisplayModeLine));
    CapturePersistentSummaryLine("VFS RWTEST PASS", cachedVfsLine, sizeof(cachedVfsLine));
    CapturePersistentSummaryLine("DESKTOP SMOKE", cachedDesktopSmokeLine, sizeof(cachedDesktopSmokeLine));
    CapturePersistentSummaryLine("DSKSURF SMOKE", cachedDsksurfSmokeLine, sizeof(cachedDsksurfSmokeLine));
    CapturePersistentSummaryLine("DESKTOP S ", cachedDesktopStatsLine, sizeof(cachedDesktopStatsLine));
    CapturePersistentSummaryLine("CDISP CORE ", cachedCoreDispatchLine, sizeof(cachedCoreDispatchLine));
    CapturePersistentSummaryLine("CDISP EXE CORE", cachedCoreDispatchPerCoreLine, sizeof(cachedCoreDispatchPerCoreLine));
    CapturePersistentSummaryLine("APW START ", cachedApWorkerLine, sizeof(cachedApWorkerLine));
    CapturePersistentSummaryLine("PARALLELPROBE MODE ", cachedParallelProbeLine, sizeof(cachedParallelProbeLine));

    if (cachedRendererLine[0] != '\0') {
        DrawHudTextLine(cachedRendererLine, Fortress::Video::FColor::RGB(255, 230, 170), cursorY);
        cursorY += lineAdvance;
    }
    if (cachedDisplayModeLine[0] != '\0') {
        DrawHudTextLine(cachedDisplayModeLine, Fortress::Video::FColor::RGB(255, 230, 170), cursorY);
        cursorY += lineAdvance;
    }
    if (cachedVfsLine[0] != '\0') {
        DrawHudTextLine(cachedVfsLine, Fortress::Video::FColor::RGB(190, 240, 190), cursorY);
        cursorY += lineAdvance;
    }
    if (cachedDesktopSmokeLine[0] != '\0') {
        DrawHudTextLine(cachedDesktopSmokeLine, Fortress::Video::FColor::RGB(190, 240, 190), cursorY);
        cursorY += lineAdvance;
    }
    if (cachedDsksurfSmokeLine[0] != '\0') {
        DrawHudTextLine(cachedDsksurfSmokeLine, Fortress::Video::FColor::RGB(190, 240, 190), cursorY);
        cursorY += lineAdvance;
    }
    if (cachedDesktopStatsLine[0] != '\0') {
        DrawHudTextLine(cachedDesktopStatsLine, Fortress::Video::FColor::RGB(180, 220, 255), cursorY);
        cursorY += lineAdvance;
    }

    if (FKernelCommandConsole::IsHudParallelStatsEnabled()) {
        DrawFilledRect(context.FrameContext.BackSurface,
                       separatorX,
                       cursorY,
                       separatorWidth,
                       2,
                       Fortress::Video::FColor::RGB(40, 80, 120));
        cursorY += lineAdvance;

        DrawHudTextLine("PARALLEL HUD", Fortress::Video::FColor::RGB(255, 230, 170), cursorY);
        cursorY += lineAdvance;

        if (cachedCoreDispatchLine[0] != '\0') {
            DrawHudTextLine(cachedCoreDispatchLine, Fortress::Video::FColor::RGB(180, 220, 255), cursorY);
            cursorY += lineAdvance;
        }
        if (cachedCoreDispatchPerCoreLine[0] != '\0') {
            DrawHudTextLine(cachedCoreDispatchPerCoreLine, Fortress::Video::FColor::RGB(180, 220, 255), cursorY);
            cursorY += lineAdvance;
        }
        if (cachedApWorkerLine[0] != '\0') {
            DrawHudTextLine(cachedApWorkerLine, Fortress::Video::FColor::RGB(180, 220, 255), cursorY);
            cursorY += lineAdvance;
        }
        if (cachedParallelProbeLine[0] != '\0') {
            DrawHudTextLine(cachedParallelProbeLine, Fortress::Video::FColor::RGB(180, 220, 255), cursorY);
            cursorY += lineAdvance;
        }
    }

    DrawFilledRect(context.FrameContext.BackSurface,
                   separatorX,
                   cursorY,
                   separatorWidth,
                   2,
                   Fortress::Video::FColor::RGB(40, 80, 120));

    cursorY += lineAdvance;
    const FKernelCommandConsole::EHudLogViewMode logViewMode = FKernelCommandConsole::GetHudLogViewMode();
    if (logViewMode != FKernelCommandConsole::EHudLogViewMode::Hidden) {
        const FKernelCommandConsole::EHudLogDetailMode detailMode = FKernelCommandConsole::GetHudLogDetailMode();
        const Fortress::Core::usize logCount =
            (logViewMode == FKernelCommandConsole::EHudLogViewMode::BootLog) ? FKernelCommandConsole::GetBootLogCount()
                                                                              : FKernelCommandConsole::GetLogCount();
        const Fortress::Core::usize maxVisibleLines =
            (cursorY <= logBottomY) ? static_cast<Fortress::Core::usize>((logBottomY - cursorY) / lineAdvance + 1) : 0u;
        if (detailMode == FKernelCommandConsole::EHudLogDetailMode::Errors ||
            detailMode == FKernelCommandConsole::EHudLogDetailMode::Warn ||
            detailMode == FKernelCommandConsole::EHudLogDetailMode::AllIssues) {
            Fortress::Core::usize matchedIndexes[128] = {};
            Fortress::Core::usize matchedCount = 0u;
            for (Fortress::Core::usize i = 0u; i < logCount && i < 128u; i++) {
                const char *line = (logViewMode == FKernelCommandConsole::EHudLogViewMode::BootLog)
                                       ? FKernelCommandConsole::GetBootLogLine(i)
                                       : FKernelCommandConsole::GetLogLine(i);
                if ((detailMode == FKernelCommandConsole::EHudLogDetailMode::Errors && IsIssueLogLine(line)) ||
                    (detailMode == FKernelCommandConsole::EHudLogDetailMode::Warn && IsWarnLogLine(line)) ||
                    (detailMode == FKernelCommandConsole::EHudLogDetailMode::AllIssues &&
                     (IsIssueLogLine(line) || IsWarnLogLine(line)))) {
                    matchedIndexes[matchedCount++] = i;
                }
            }

            Fortress::Core::usize startMatch = 0u;
            if (matchedCount > maxVisibleLines) {
                startMatch = matchedCount - maxVisibleLines;
            }

            if (matchedCount == 0u) {
                if (detailMode == FKernelCommandConsole::EHudLogDetailMode::Errors) {
                    DrawHudTextLine("NO ISSUE LINES (FAIL|ERR|INVALID|PANIC|TIMEOUT)",
                                    Fortress::Video::FColor::RGB(210, 220, 180),
                                    cursorY);
                } else if (detailMode == FKernelCommandConsole::EHudLogDetailMode::Warn) {
                    DrawHudTextLine("NO WARN LINES (WARN|DEGRADED|RETRY|FALLBACK)",
                                    Fortress::Video::FColor::RGB(210, 220, 180),
                                    cursorY);
                } else {
                    DrawHudTextLine("NO ISSUE/WARN LINES", Fortress::Video::FColor::RGB(210, 220, 180), cursorY);
                }
            } else {
                for (Fortress::Core::usize m = startMatch; m < matchedCount; m++) {
                    const Fortress::Core::usize i = matchedIndexes[m];
                    const char *line = (logViewMode == FKernelCommandConsole::EHudLogViewMode::BootLog)
                                           ? FKernelCommandConsole::GetBootLogLine(i)
                                           : FKernelCommandConsole::GetLogLine(i);
                    const Fortress::Video::FColor highlightColor =
                        (detailMode == FKernelCommandConsole::EHudLogDetailMode::Errors)
                            ? Fortress::Video::FColor::RGB(255, 210, 170)
                            : (detailMode == FKernelCommandConsole::EHudLogDetailMode::Warn)
                                  ? Fortress::Video::FColor::RGB(255, 240, 170)
                                  : Fortress::Video::FColor::RGB(255, 225, 160);
                    DrawHudTextLine(line, highlightColor, cursorY);
                    cursorY += lineAdvance;
                    if (cursorY > logBottomY) {
                        break;
                    }
                }
            }
        } else {
            Fortress::Core::usize startIndex = 0u;
            if (detailMode == FKernelCommandConsole::EHudLogDetailMode::Tail) {
                if (logCount > maxVisibleLines) {
                    startIndex = logCount - maxVisibleLines;
                }
            }

            Fortress::Core::int32 drawY = cursorY;
            if (detailMode == FKernelCommandConsole::EHudLogDetailMode::Tail && logCount > startIndex) {
                const Fortress::Core::usize visibleCount = logCount - startIndex;
                const Fortress::Core::int32 usedHeight =
                    static_cast<Fortress::Core::int32>((visibleCount - 1u) * static_cast<Fortress::Core::usize>(lineAdvance));
                const Fortress::Core::int32 anchoredY = logBottomY - usedHeight;
                if (anchoredY > drawY) {
                    drawY = anchoredY;
                }
            }

            for (Fortress::Core::usize i = startIndex; i < logCount; i++) {
                const char *line = (logViewMode == FKernelCommandConsole::EHudLogViewMode::BootLog)
                                       ? FKernelCommandConsole::GetBootLogLine(i)
                                       : FKernelCommandConsole::GetLogLine(i);
                DrawHudTextLine(line, Fortress::Video::FColor::RGB(200, 220, 255), drawY);
                drawY += lineAdvance;
                if (drawY > logBottomY) {
                    break;
                }
            }
        }
    }

    const char *pinnedLogHint =
        (logViewMode == FKernelCommandConsole::EHudLogViewMode::Hidden)
            ? "LOG VIEW OFF  (SHOWLOG [TAIL|FULL|ERRORS|WARN|ALLISSUES] | BOOTLOG)"
            : "LOG: SHOWLOG [TAIL|FULL|ERRORS|WARN|ALLISSUES] | BOOTLOG | HIDELOG";
    DrawHudTextLine(pinnedLogHint, Fortress::Video::FColor::RGB(235, 230, 170), pinnedLogHintY);

    DrawFilledRect(context.FrameContext.BackSurface,
                   separatorX,
                   inputLineY - 4,
                   separatorWidth,
                   2,
                   Fortress::Video::FColor::RGB(100, 64, 40));

    const Fortress::Video::FColor inputBg =
        SampleVerticalGradientColor(panelTopColor, panelBottomColor, panelHeight, inputLineY - panelY);
    DrawTextLine(context.FrameContext.BackSurface,
                 GFallbackHudFont,
                 "INPUT> ",
                 2,
                 Fortress::Video::FColor::RGB(255, 240, 160),
                 inputBg,
                 textX,
                 inputLineY);

    const Fortress::Core::int32 okButtonWidth = 82;
    const Fortress::Core::int32 okButtonHeight = lineAdvance + 8;
    const Fortress::Core::int32 okButtonX = separatorX + separatorWidth - okButtonWidth - 6;
    const Fortress::Core::int32 okButtonY = inputLineY - 4;

    const Fortress::Core::int32 inputStartX = textX + (glyphAdvance * 7);
    Fortress::Core::int32 inputRightX = separatorX + separatorWidth - 6;
    const bool canRenderOkButton = (okButtonX - inputStartX) > (glyphAdvance * 8);
    if (canRenderOkButton) {
        inputRightX = okButtonX - 8;
    }
    const Fortress::Core::int32 inputWidthPixels = inputRightX - inputStartX;
    Fortress::Core::usize maxInputChars = 1u;
    if (inputWidthPixels > glyphAdvance) {
        maxInputChars = static_cast<Fortress::Core::usize>(inputWidthPixels / glyphAdvance);
    }

    static Fortress::Core::uint32 GInputCaretFrameCounter = 0u;
    GInputCaretFrameCounter++;
    const bool caretVisible = ((GInputCaretFrameCounter / 18u) % 2u) == 0u;

    char inputDisplayLine[96] = {};
    BuildInputDisplayLine(FKernelCommandConsole::GetCommandBuffer(),
                          maxInputChars,
                          caretVisible,
                          inputDisplayLine,
                          sizeof(inputDisplayLine));

    DrawTextLine(context.FrameContext.BackSurface,
                 GFallbackHudFont,
                 inputDisplayLine,
                 2,
                 Fortress::Video::FColor::RGB(255, 255, 230),
                 inputBg,
                 inputStartX,
                 inputLineY);

    if (canRenderOkButton) {
        DrawVistaButton(context.FrameContext.BackSurface,
                        GFallbackHudFont,
                        "OK",
                        okButtonX,
                        okButtonY,
                        okButtonWidth,
                        okButtonHeight,
                        2);
    }
}

static bool ShouldRunCursorOverlayPass(const FFramePassContext &context) {
    return context.Options.RenderCursorOverlay;
}

static bool ShouldRunInputPulsePass(const FFramePassContext &context) {
    return context.FrameContext.IsValid() && context.Options.RenderInputPulse;
}

static void ExecuteInputPulsePass(const FFramePassContext &context) {
    const Fortress::Video::FVideoSurfaceView &surface = context.FrameContext.BackSurface;
    DrawFilledRect(surface, 10, 10, 14, 14, Fortress::Video::FColor::RGB(80, 240, 120));
    DrawFilledRect(surface, 12, 12, 10, 10, Fortress::Video::FColor::RGB(210, 255, 220));
}

static void ExecuteCursorOverlayPass(const FFramePassContext &context) {
    DrawCursorOverlayAndTarget(context.FrameContext, context.Options.ConsumeButtonEdges);
}

static void ExecuteFramePasses(const FFramePassContext &context) {
    static const FFramePassDesc Passes[] = {
        {EFramePassId::Clear, &ShouldRunClearPass, &ExecuteClearPass},
        {EFramePassId::Scene, &ShouldRunScenePass, &ExecuteScenePass},
        {EFramePassId::DesktopSurfaces, &ShouldRunDesktopSurfacesPass, &ExecuteDesktopSurfacesPass},
        {EFramePassId::InputPulse, &ShouldRunInputPulsePass, &ExecuteInputPulsePass},
        {EFramePassId::CursorOverlay, &ShouldRunCursorOverlayPass, &ExecuteCursorOverlayPass},
    };

    for (Fortress::Core::usize i = 0; i < (sizeof(Passes) / sizeof(Passes[0])); i++) {
        const FFramePassDesc &pass = Passes[i];
        if (pass.ShouldRun == nullptr || pass.Execute == nullptr) {
            continue;
        }
        if (pass.ShouldRun(context)) {
            pass.Execute(context);
        }
    }
}

} // namespace

FFrameRenderOptions FKernelFramePipeline::BuildFrameRenderOptions(Fortress::Core::uint64 fpsValue,
                                                                  bool consumeButtonEdges) {
    FFrameRenderOptions options{};
    options.FpsValue = fpsValue;
    // HUD-only stabilization mode: keep rendering path minimal while desktop text UX is hardened.
    options.RenderScene = false;
    options.WireframeEnabled = false;
    options.RenderDesktopSurfaces = false;
    options.RenderHud = true;
    (void)FKernelCommandControlPlane::ConsumeCommandPulseFrame();
    options.RenderInputPulse = false;
    options.RenderCursorOverlay = false;
    options.ConsumeButtonEdges = consumeButtonEdges;
    return options;
}

void FKernelFramePipeline::RenderFrame(FKernelRuntimeContext &runtime,
                                       const FFrameRenderOptions &options,
                                       FDesktopRuntime &desktopRuntime) {
    if (runtime.DisplayManager == nullptr || !runtime.DisplayManager->IsReady()) {
        return;
    }

    Fortress::Video::FDisplayFrameContext frameContext = runtime.DisplayManager->BeginFrame();
    if (frameContext.ModeChanged) {
        (void)runtime.CubeScene->UpdateViewport(frameContext.Mode.Width, frameContext.Mode.Height);
    }

    const Fortress::Memory::FPhysicalMemoryStats pmmStats = Fortress::Memory::FPhysicalMemoryManager::GetStats();
    const Fortress::Memory::FVirtualMemoryStats vmmStats = Fortress::Memory::FVirtualMemoryManager::GetStats();
    const Fortress::Memory::FKernelHeapStats heapStats = Fortress::Memory::FKernelHeap::GetStats();
    const auto arenaStats = Fortress::Memory::FMemoryArena::GetStats();

    const FFramePassContext passContext{
        .Runtime = runtime,
        .FrameContext = frameContext,
        .Options = options,
        .PmmStats = pmmStats,
        .VmmStats = vmmStats,
        .HeapStats = heapStats,
        .ArenaUsedBytes = static_cast<Fortress::Core::uint64>(arenaStats.UsedBytes),
        .DesktopRuntime = desktopRuntime,
    };
    ExecuteFramePasses(passContext);

    // Final HUD composite pass keeps text visible even if earlier passes repaint overlapping regions.
    ExecuteHudPass(passContext);

    runtime.DisplayManager->Present();
}

} // namespace Fortress::Kernel
