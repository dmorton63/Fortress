#include "Fortress/Kernel/FKernelFramePipeline.hpp"

#include "Fortress/Kernel/FKernelCommandConsole.hpp"
#include "Fortress/Kernel/FKernelCommandControlPlane.hpp"
#include "Fortress/Kernel/FKernelCubeScene.hpp"
#include "Fortress/Kernel/Generated/FAeroTheme.generated.hpp"
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
    SurfaceSelfTest,
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

struct FDesktopThemePalette {
    Fortress::Video::FColor WindowBackground;
    Fortress::Video::FColor TitleBarGradientStart;
    Fortress::Video::FColor TitleBarGradientEnd;
    Fortress::Video::FColor ButtonNormal;
    Fortress::Video::FColor ButtonHover;
    Fortress::Video::FColor ButtonPressed;
    Fortress::Video::FColor ButtonGlow;
    Fortress::Video::FColor TextPrimary;
    Fortress::Video::FColor TextSecondary;
    Fortress::Video::FColor Border;
    Fortress::Video::FColor Shadow;
    Fortress::Video::FColor AccentPrimary;
    Fortress::Video::FColor AccentSecondary;
};

static const FDesktopThemePalette &GetDesktopThemePalette() {
    using Fortress::Kernel::GeneratedAeroTheme::kAccentPrimary;
    using Fortress::Kernel::GeneratedAeroTheme::kAccentSecondary;
    using Fortress::Kernel::GeneratedAeroTheme::kBorder;
    using Fortress::Kernel::GeneratedAeroTheme::kButtonGlow;
    using Fortress::Kernel::GeneratedAeroTheme::kButtonHover;
    using Fortress::Kernel::GeneratedAeroTheme::kButtonNormal;
    using Fortress::Kernel::GeneratedAeroTheme::kButtonPressed;
    using Fortress::Kernel::GeneratedAeroTheme::kShadow;
    using Fortress::Kernel::GeneratedAeroTheme::kTextPrimary;
    using Fortress::Kernel::GeneratedAeroTheme::kTextSecondary;
    using Fortress::Kernel::GeneratedAeroTheme::kTitleBarGradientEnd;
    using Fortress::Kernel::GeneratedAeroTheme::kTitleBarGradientStart;
    using Fortress::Kernel::GeneratedAeroTheme::kWindowBackground;

    static const FDesktopThemePalette palette{
        .WindowBackground = kWindowBackground,
        .TitleBarGradientStart = kTitleBarGradientStart,
        .TitleBarGradientEnd = kTitleBarGradientEnd,
        .ButtonNormal = kButtonNormal,
        .ButtonHover = kButtonHover,
        .ButtonPressed = kButtonPressed,
        .ButtonGlow = kButtonGlow,
        .TextPrimary = kTextPrimary,
        .TextSecondary = kTextSecondary,
        .Border = kBorder,
        .Shadow = kShadow,
        .AccentPrimary = kAccentPrimary,
        .AccentSecondary = kAccentSecondary,
    };
    return palette;
}

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
    const Fortress::Core::uint32 packed =
        Fortress::Video::FVideoSurfaceOps::PackMasked32(color, surface.Desc.PixelMask);
    Fortress::Video::FVideoSurfaceOps::FillRect32(surface, x, y, width, height, packed);
}

static void DrawFilledRectAlpha(const Fortress::Video::FVideoSurfaceView &surface,
                                Fortress::Core::int32 x,
                                Fortress::Core::int32 y,
                                Fortress::Core::int32 width,
                                Fortress::Core::int32 height,
                                Fortress::Video::FColor color) {
    Fortress::Video::FVideoSurfaceOps::FillRectAlpha32(surface, x, y, width, height, color);
}

static void DrawVerticalGradientRect(const Fortress::Video::FVideoSurfaceView &surface,
                                                                         Fortress::Core::int32 x,
                                                                         Fortress::Core::int32 y,
                                                                         Fortress::Core::int32 width,
                                                                         Fortress::Core::int32 height,
                                                                         Fortress::Video::FColor topColor,
                                                                         Fortress::Video::FColor bottomColor) {
    Fortress::Video::FVideoSurfaceOps::FillGradientVertical32(surface,
                                   x,
                                   y,
                                   width,
                                   height,
                                   topColor,
                                   bottomColor);
}

static void DrawHorizontalGradientRect(const Fortress::Video::FVideoSurfaceView &surface,
                                       Fortress::Core::int32 x,
                                       Fortress::Core::int32 y,
                                       Fortress::Core::int32 width,
                                       Fortress::Core::int32 height,
                                       Fortress::Video::FColor leftColor,
                                       Fortress::Video::FColor rightColor) {
    Fortress::Video::FVideoSurfaceOps::FillGradientHorizontal32(surface,
                                                                x,
                                                                y,
                                                                width,
                                                                height,
                                                                leftColor,
                                                                rightColor);
}

static void DrawHorizontalGradientRectAlpha(const Fortress::Video::FVideoSurfaceView &surface,
                                            Fortress::Core::int32 x,
                                            Fortress::Core::int32 y,
                                            Fortress::Core::int32 width,
                                            Fortress::Core::int32 height,
                                            Fortress::Video::FColor leftColor,
                                            Fortress::Video::FColor rightColor) {
    Fortress::Video::FVideoSurfaceOps::FillGradientHorizontalAlpha32(surface,
                                                                     x,
                                                                     y,
                                                                     width,
                                                                     height,
                                                                     leftColor,
                                                                     rightColor);
}

static void DrawVerticalGradientRectAlpha(const Fortress::Video::FVideoSurfaceView &surface,
                                          Fortress::Core::int32 x,
                                          Fortress::Core::int32 y,
                                          Fortress::Core::int32 width,
                                          Fortress::Core::int32 height,
                                          Fortress::Video::FColor topColor,
                                          Fortress::Video::FColor bottomColor) {
    Fortress::Video::FVideoSurfaceOps::FillGradientVerticalAlpha32(surface,
                                                                   x,
                                                                   y,
                                                                   width,
                                                                   height,
                                                                   topColor,
                                                                   bottomColor);
}

static void DrawDiagonalNoiseOverlay(const Fortress::Video::FVideoSurfaceView &surface,
                                     Fortress::Core::int32 x,
                                     Fortress::Core::int32 y,
                                     Fortress::Core::int32 width,
                                     Fortress::Core::int32 height,
                                     Fortress::Video::FColor color,
                                     Fortress::Core::int32 spacing) {
    if (!surface.IsValid() || width <= 0 || height <= 0 || spacing < 2) {
        return;
    }

    for (Fortress::Core::int32 py = 0; py < height; py++) {
        for (Fortress::Core::int32 px = 0; px < width; px++) {
            const Fortress::Core::int32 diagonal = (px + py) % spacing;
            if (diagonal == 0 || diagonal == 1) {
                Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x + px, y + py, color);
            }
        }
    }
}

static void DrawRoundedRectHints(const Fortress::Video::FVideoSurfaceView &surface,
                                 Fortress::Core::int32 x,
                                 Fortress::Core::int32 y,
                                 Fortress::Core::int32 width,
                                 Fortress::Core::int32 height,
                                 Fortress::Video::FColor color) {
    if (width < 6 || height < 6) {
        return;
    }

    // Soften hard-corner edges to emulate a rounded 6-8 px capsule silhouette.
    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x + 1, y + 1, color);
    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x + 2, y, color);
    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x, y + 2, color);

    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x + width - 2, y + 1, color);
    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x + width - 3, y, color);
    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x + width - 1, y + 2, color);

    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x + 1, y + height - 2, color);
    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x, y + height - 3, color);
    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x + 2, y + height - 1, color);

    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x + width - 2, y + height - 2, color);
    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x + width - 1, y + height - 3, color);
    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x + width - 3, y + height - 1, color);
}

static Fortress::Video::FColor ScaleAlpha(Fortress::Video::FColor color, Fortress::Core::uint8 alphaScale) {
    const Fortress::Core::uint32 scaled =
        (static_cast<Fortress::Core::uint32>(color.A) * static_cast<Fortress::Core::uint32>(alphaScale)) / 255u;
    color.A = static_cast<Fortress::Core::uint8>(scaled);
    return color;
}

static Fortress::Video::FColor LerpColor(Fortress::Video::FColor a,
                                         Fortress::Video::FColor b,
                                         Fortress::Core::uint32 t,
                                         Fortress::Core::uint32 denom) {
    if (denom == 0u) {
        return a;
    }

    const Fortress::Core::uint32 r = (static_cast<Fortress::Core::uint32>(a.R) * (denom - t) +
                                      static_cast<Fortress::Core::uint32>(b.R) * t) /
                                     denom;
    const Fortress::Core::uint32 g = (static_cast<Fortress::Core::uint32>(a.G) * (denom - t) +
                                      static_cast<Fortress::Core::uint32>(b.G) * t) /
                                     denom;
    const Fortress::Core::uint32 bl = (static_cast<Fortress::Core::uint32>(a.B) * (denom - t) +
                                       static_cast<Fortress::Core::uint32>(b.B) * t) /
                                      denom;
    const Fortress::Core::uint32 al = (static_cast<Fortress::Core::uint32>(a.A) * (denom - t) +
                                       static_cast<Fortress::Core::uint32>(b.A) * t) /
                                      denom;

    return Fortress::Video::FColor{
        .R = static_cast<Fortress::Core::uint8>(r),
        .G = static_cast<Fortress::Core::uint8>(g),
        .B = static_cast<Fortress::Core::uint8>(bl),
        .A = static_cast<Fortress::Core::uint8>(al),
    };
}

static void DrawRoundedVerticalGradientRectAlpha(const Fortress::Video::FVideoSurfaceView &surface,
                                                 Fortress::Core::int32 x,
                                                 Fortress::Core::int32 y,
                                                 Fortress::Core::int32 width,
                                                 Fortress::Core::int32 height,
                                                 Fortress::Core::int32 radius,
                                                 Fortress::Video::FColor topColor,
                                                 Fortress::Video::FColor bottomColor) {
    if (!surface.IsValid() || width <= 0 || height <= 0) {
        return;
    }

    if (radius < 0) {
        radius = 0;
    }
    const Fortress::Core::int32 maxRadius = (width < height ? width : height) / 2;
    if (radius > maxRadius) {
        radius = maxRadius;
    }

    const Fortress::Core::uint32 gradientDenom = (height > 1) ? static_cast<Fortress::Core::uint32>(height - 1) : 1u;
    const Fortress::Core::int32 cornerSpan = radius - 1;
    const Fortress::Core::int32 radiusSq = cornerSpan * cornerSpan;

    for (Fortress::Core::int32 py = 0; py < height; py++) {
        const Fortress::Core::uint32 t = static_cast<Fortress::Core::uint32>(py);
        const Fortress::Video::FColor rowColor = LerpColor(topColor, bottomColor, t, gradientDenom);

        for (Fortress::Core::int32 px = 0; px < width; px++) {
            bool inside = true;
            if (radius > 0) {
                if (px < radius && py < radius) {
                    const Fortress::Core::int32 dx = cornerSpan - px;
                    const Fortress::Core::int32 dy = cornerSpan - py;
                    inside = (dx * dx + dy * dy) <= radiusSq;
                } else if (px >= (width - radius) && py < radius) {
                    const Fortress::Core::int32 dx = px - (width - radius);
                    const Fortress::Core::int32 dy = cornerSpan - py;
                    inside = (dx * dx + dy * dy) <= radiusSq;
                } else if (px < radius && py >= (height - radius)) {
                    const Fortress::Core::int32 dx = cornerSpan - px;
                    const Fortress::Core::int32 dy = py - (height - radius);
                    inside = (dx * dx + dy * dy) <= radiusSq;
                } else if (px >= (width - radius) && py >= (height - radius)) {
                    const Fortress::Core::int32 dx = px - (width - radius);
                    const Fortress::Core::int32 dy = py - (height - radius);
                    inside = (dx * dx + dy * dy) <= radiusSq;
                }
            }

            if (!inside) {
                continue;
            }

            Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x + px, y + py, rowColor);
        }
    }
}

static void DrawTaperedHorizontalLineAlpha(const Fortress::Video::FVideoSurfaceView &surface,
                                           Fortress::Core::int32 x,
                                           Fortress::Core::int32 y,
                                           Fortress::Core::int32 width,
                                           Fortress::Core::int32 inset,
                                           Fortress::Video::FColor color) {
    if (!surface.IsValid() || width <= 0) {
        return;
    }

    if (inset < 0) {
        inset = 0;
    }

    Fortress::Core::int32 startX = x + inset;
    Fortress::Core::int32 endX = x + width - inset - 1;
    if (startX > endX) {
        return;
    }

    const Fortress::Core::int32 available = endX - startX + 1;
    Fortress::Core::int32 taperPixels = 3;
    if (available < 8) {
        taperPixels = 1;
    } else if (available < 12) {
        taperPixels = 2;
    }

    const Fortress::Core::int32 centerWidth = available - (taperPixels * 2);
    if (centerWidth > 0) {
        DrawFilledRectAlpha(surface, startX + taperPixels, y, centerWidth, 1, color);
    }

    for (Fortress::Core::int32 i = 0; i < taperPixels; i++) {
        const Fortress::Core::uint8 alphaScale =
            static_cast<Fortress::Core::uint8>(((i + 1) * 255) / (taperPixels + 1));
        const Fortress::Video::FColor tapColor = ScaleAlpha(color, alphaScale);
        Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, startX + i, y, tapColor);
        Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, endX - i, y, tapColor);
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

    const FDesktopThemePalette &theme = GetDesktopThemePalette();

    // 1) Outer silhouette and soft shadow.
    DrawFilledRectAlpha(surface,
                        x + 2,
                        y + 3,
                        width,
                        height,
                        Fortress::Video::FColor{.R = 0u, .G = 0u, .B = 0u, .A = 0x34u});
    DrawVerticalGradientRect(surface,
                             x,
                             y,
                             width,
                             height,
                             Fortress::Video::FColor::RGB(46, 66, 94),
                             Fortress::Video::FColor::RGB(24, 38, 60));

    const Fortress::Core::int32 innerX = x + 2;
    const Fortress::Core::int32 innerY = y + 2;
    const Fortress::Core::int32 innerW = width - 4;
    const Fortress::Core::int32 innerH = height - 4;
    const Fortress::Core::int32 topH = innerH / 2;
    const Fortress::Core::int32 bottomH = innerH - topH;

    // 2) Cool blue glass body.
    DrawVerticalGradientRect(surface,
                             innerX,
                             innerY,
                             innerW,
                             topH,
                             Fortress::Video::FColor::RGB(142, 200, 244),
                             Fortress::Video::FColor::RGB(70, 138, 212));
    DrawVerticalGradientRect(surface,
                             innerX,
                             innerY + topH,
                             innerW,
                             bottomH,
                             Fortress::Video::FColor::RGB(58, 118, 196),
                             Fortress::Video::FColor::RGB(34, 74, 136));

    // 3) Upper glass sheen.
    const Fortress::Core::int32 glowH = (innerH * 3) / 10;
    if (glowH > 1) {
        DrawVerticalGradientRectAlpha(surface,
                                      innerX + 1,
                                      innerY + 1,
                                      innerW - 2,
                                      glowH,
                                      Fortress::Video::FColor{.R = 255u, .G = 255u, .B = 255u, .A = 0x8Eu},
                                      Fortress::Video::FColor{.R = 255u, .G = 255u, .B = 255u, .A = 0x12u});
    }

    // 4) Top-half gloss veil.
    const Fortress::Core::int32 glossH = innerH / 2;
    if (glossH > 2) {
        DrawVerticalGradientRectAlpha(surface,
                                      innerX + 1,
                                      innerY + 1,
                                      innerW - 2,
                                      glossH,
                                      Fortress::Video::FColor{.R = 206u, .G = 232u, .B = 255u, .A = 0x54u},
                                      Fortress::Video::FColor{.R = 190u, .G = 224u, .B = 255u, .A = 0x00u});
    }

    // 5) Central specular sweep (left->center and center->right).
    const Fortress::Core::int32 sweepY = innerY + 2;
    const Fortress::Core::int32 sweepH = (innerH > 8) ? ((innerH * 2) / 5) : (innerH / 2);
    const Fortress::Core::int32 sweepW = innerW - 2;
    const Fortress::Core::int32 halfSweepW = sweepW / 2;
    if (sweepH > 2 && halfSweepW > 2) {
        DrawHorizontalGradientRectAlpha(surface,
                                        innerX + 1,
                                        sweepY,
                                        halfSweepW,
                                        sweepH,
                                        Fortress::Video::FColor{.R = 255u, .G = 255u, .B = 255u, .A = 0x06u},
                                        Fortress::Video::FColor{.R = 255u, .G = 255u, .B = 255u, .A = 0x62u});
        DrawHorizontalGradientRectAlpha(surface,
                                        innerX + 1 + halfSweepW,
                                        sweepY,
                                        sweepW - halfSweepW,
                                        sweepH,
                                        Fortress::Video::FColor{.R = 255u, .G = 255u, .B = 255u, .A = 0x62u},
                                        Fortress::Video::FColor{.R = 255u, .G = 255u, .B = 255u, .A = 0x06u});
    }

    // 6) Top/left bevel highlight.
    DrawFilledRectAlpha(surface,
                        innerX,
                        innerY,
                        innerW,
                        1,
                        Fortress::Video::FColor{.R = 255u, .G = 255u, .B = 255u, .A = 0xB8u});
    DrawFilledRectAlpha(surface,
                        innerX,
                        innerY,
                        1,
                        innerH,
                        Fortress::Video::FColor{.R = 245u, .G = 252u, .B = 255u, .A = 0x7Eu});

    // 7) Bottom/right inner shadow bevel.
    DrawFilledRectAlpha(surface,
                        innerX,
                        innerY + innerH - 1,
                        innerW,
                        1,
                        Fortress::Video::FColor{.R = 6u, .G = 22u, .B = 42u, .A = 0xA8u});
    DrawFilledRectAlpha(surface,
                        innerX + innerW - 1,
                        innerY,
                        1,
                        innerH,
                        Fortress::Video::FColor{.R = 8u, .G = 24u, .B = 44u, .A = 0x88u});

    // 8) Lower tint to restore depth under glass.
    DrawVerticalGradientRectAlpha(surface,
                                  innerX + 1,
                                  innerY + innerH - 4,
                                  innerW - 2,
                                  3,
                                  Fortress::Video::FColor{.R = 0u, .G = 0u, .B = 0u, .A = 0x00u},
                                  Fortress::Video::FColor{.R = 8u, .G = 18u, .B = 34u, .A = 0x60u});

    // 9) Corner shaping hints for rounded-capsule illusion.
    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface, x + 1, y + 1, Fortress::Video::FColor{.R = 0u, .G = 0u, .B = 0u, .A = 0x58u});
    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface,
                                                    x + width - 2,
                                                    y + 1,
                                                    Fortress::Video::FColor{.R = 0u, .G = 0u, .B = 0u, .A = 0x58u});
    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface,
                                                    x + 1,
                                                    y + height - 2,
                                                    Fortress::Video::FColor{.R = 0u, .G = 0u, .B = 0u, .A = 0x78u});
    Fortress::Video::FVideoSurfaceOps::BlendPixel32(surface,
                                                    x + width - 2,
                                                    y + height - 2,
                                                    Fortress::Video::FColor{.R = 0u, .G = 0u, .B = 0u, .A = 0x78u});

    const Fortress::Core::usize labelLen = StringLength(label);
    const Fortress::Core::int32 glyphAdvance = Fortress::Video::FTextRenderer::GetGlyphAdvance(scale);
    const Fortress::Core::int32 lineAdvance = Fortress::Video::FTextRenderer::GetLineAdvance(scale);
    const Fortress::Core::int32 textWidth = static_cast<Fortress::Core::int32>(labelLen) * glyphAdvance;
    const Fortress::Core::int32 textX = x + (width - textWidth) / 2;
    const Fortress::Core::int32 textY = y + (height - lineAdvance) / 2;

    const Fortress::Video::FColor textBg = SampleVerticalGradientColor(Fortress::Video::FColor::RGB(142, 200, 244),
                                                                       Fortress::Video::FColor::RGB(34, 74, 136),
                                                                       innerH,
                                                                       (textY - innerY) + (lineAdvance / 2));
    DrawTextLine(surface,
                 font,
                 label,
                 scale,
                 theme.TextPrimary,
                 textBg,
                 textX + 1,
                 textY + 1);

    DrawTextLine(surface,
                 font,
                 label,
                 scale,
                 theme.WindowBackground,
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
    DrawVerticalGradientRect(surface,
                             0,
                             0,
                             static_cast<Fortress::Core::int32>(surface.Desc.Width),
                             static_cast<Fortress::Core::int32>(surface.Desc.Height),
                             Fortress::Video::FColor::RGB(112, 24, 28),
                             Fortress::Video::FColor::RGB(54, 12, 22));
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
    if (!context.DesktopRuntime.GetOverlay().IsReady()) {
        return;
    }

    context.DesktopRuntime.GetOverlay().Render(context.FrameContext.BackSurface);
}

static bool ShouldRunSurfaceSelfTestPass(const FFramePassContext &context) {
    return context.FrameContext.IsValid() && context.Options.RenderSurfaceSelfTest;
}

static void DrawAlphaTestBackdrop(const Fortress::Video::FVideoSurfaceView &surface,
                                  Fortress::Core::int32 x,
                                  Fortress::Core::int32 y,
                                  Fortress::Core::int32 width,
                                  Fortress::Core::int32 height) {
    const Fortress::Core::uint32 light = Fortress::Video::FVideoSurfaceOps::PackMasked32(
        Fortress::Video::FColor::RGB(194, 204, 216), surface.Desc.PixelMask);
    const Fortress::Core::uint32 dark = Fortress::Video::FVideoSurfaceOps::PackMasked32(
        Fortress::Video::FColor::RGB(130, 144, 160), surface.Desc.PixelMask);

    constexpr Fortress::Core::int32 cell = 8;
    for (Fortress::Core::int32 py = 0; py < height; py += cell) {
        for (Fortress::Core::int32 px = 0; px < width; px += cell) {
            const bool darkCell = (((px / cell) + (py / cell)) & 1) != 0;
            Fortress::Video::FVideoSurfaceOps::FillRect32(surface,
                                                          x + px,
                                                          y + py,
                                                          cell,
                                                          cell,
                                                          darkCell ? dark : light);
        }
    }
}

static void ExecuteSurfaceSelfTestPass(const FFramePassContext &context) {
    const Fortress::Video::FVideoSurfaceView &surface = context.FrameContext.BackSurface;

    const Fortress::Core::int32 panelX = 24;
    const Fortress::Core::int32 panelY = 58;
    const Fortress::Core::int32 panelW = 420;
    const Fortress::Core::int32 panelH = 190;

    const Fortress::Core::uint32 panelBorder = Fortress::Video::FVideoSurfaceOps::PackMasked32(
        Fortress::Video::FColor::RGB(36, 44, 56), surface.Desc.PixelMask);
    const Fortress::Core::uint32 panelBg = Fortress::Video::FVideoSurfaceOps::PackMasked32(
        Fortress::Video::FColor::RGB(78, 96, 120), surface.Desc.PixelMask);

    Fortress::Video::FVideoSurfaceOps::FillRect32(surface, panelX, panelY, panelW, panelH, panelBorder);
    Fortress::Video::FVideoSurfaceOps::FillRect32(surface, panelX + 2, panelY + 2, panelW - 4, panelH - 4, panelBg);

    const Fortress::Core::int32 swW = 120;
    const Fortress::Core::int32 swH = 52;
    const Fortress::Core::int32 gapX = 12;
    const Fortress::Core::int32 gapY = 12;
    const Fortress::Core::int32 row0Y = panelY + 16;
    const Fortress::Core::int32 row1Y = row0Y + swH + gapY;
    const Fortress::Core::int32 col0X = panelX + 16;
    const Fortress::Core::int32 col1X = col0X + swW + gapX;
    const Fortress::Core::int32 col2X = col1X + swW + gapX;

    // Opaque swatches.
    const Fortress::Core::uint32 opaqueSolid = Fortress::Video::FVideoSurfaceOps::PackMasked32(
        Fortress::Video::FColor::RGB(44, 156, 240), surface.Desc.PixelMask);
    Fortress::Video::FVideoSurfaceOps::FillRect32(surface, col0X, row0Y, swW, swH, opaqueSolid);
    Fortress::Video::FVideoSurfaceOps::FillGradientVertical32(surface,
                                                              col1X,
                                                              row0Y,
                                                              swW,
                                                              swH,
                                                              Fortress::Video::FColor::RGB(128, 226, 255),
                                                              Fortress::Video::FColor::RGB(28, 122, 204));
    Fortress::Video::FVideoSurfaceOps::FillGradientHorizontal32(surface,
                                                                col2X,
                                                                row0Y,
                                                                swW,
                                                                swH,
                                                                Fortress::Video::FColor::RGB(255, 184, 110),
                                                                Fortress::Video::FColor::RGB(204, 74, 128));

    // Alpha swatches over checkerboard backdrop.
    DrawAlphaTestBackdrop(surface, col0X, row1Y, swW, swH);
    DrawAlphaTestBackdrop(surface, col1X, row1Y, swW, swH);
    DrawAlphaTestBackdrop(surface, col2X, row1Y, swW, swH);

    Fortress::Video::FVideoSurfaceOps::FillRectAlpha32(surface,
                                                       col0X,
                                                       row1Y,
                                                       swW,
                                                       swH,
                                                       Fortress::Video::FColor{.R = 36u, .G = 188u, .B = 250u, .A = 128u});
    Fortress::Video::FVideoSurfaceOps::FillGradientVerticalAlpha32(
        surface,
        col1X,
        row1Y,
        swW,
        swH,
        Fortress::Video::FColor{.R = 255u, .G = 255u, .B = 255u, .A = 200u},
        Fortress::Video::FColor{.R = 42u, .G = 102u, .B = 220u, .A = 30u});
    Fortress::Video::FVideoSurfaceOps::FillGradientHorizontalAlpha32(
        surface,
        col2X,
        row1Y,
        swW,
        swH,
        Fortress::Video::FColor{.R = 255u, .G = 72u, .B = 72u, .A = 200u},
        Fortress::Video::FColor{.R = 72u, .G = 255u, .B = 144u, .A = 36u});
}

static void ExecuteHudPass(const FFramePassContext &context) {
#if defined(FORTRESS_EXPERIMENTAL_DISPLAY_LATENCY)
    const auto NotifyHudPresented = []() {
        FKernelCommandConsole::NotifyHudPresented();
    };
#endif
    Fortress::Core::int32 panelX = 8;
    Fortress::Core::int32 panelY = 8;
    Fortress::Core::int32 panelWidth =
        static_cast<Fortress::Core::int32>(context.FrameContext.BackSurface.Desc.Width) - (panelX * 2);
    Fortress::Core::int32 panelHeight =
        static_cast<Fortress::Core::int32>(context.FrameContext.BackSurface.Desc.Height) - (panelY * 2);

    if (FKernelCommandConsole::IsTerminalModeEnabled()) {
        Fortress::Core::int32 terminalX = 0;
        Fortress::Core::int32 terminalY = 0;
        Fortress::Core::int32 terminalWidth = 0;
        Fortress::Core::int32 terminalHeight = 0;
        if (FKernelCommandConsole::TryGetTerminalWindowBounds(terminalX, terminalY, terminalWidth, terminalHeight)) {
            panelX = terminalX;
            panelY = terminalY;
            panelWidth = terminalWidth;
            panelHeight = terminalHeight;
        }
    }

    const Fortress::Core::int32 screenWidth = static_cast<Fortress::Core::int32>(context.FrameContext.BackSurface.Desc.Width);
    const Fortress::Core::int32 screenHeight = static_cast<Fortress::Core::int32>(context.FrameContext.BackSurface.Desc.Height);
    if (panelX < 0) {
        panelWidth += panelX;
        panelX = 0;
    }
    if (panelY < 0) {
        panelHeight += panelY;
        panelY = 0;
    }
    if (panelX + panelWidth > screenWidth) {
        panelWidth = screenWidth - panelX;
    }
    if (panelY + panelHeight > screenHeight) {
        panelHeight = screenHeight - panelY;
    }

    if (panelWidth <= 0 || panelHeight <= 0) {
#if defined(FORTRESS_EXPERIMENTAL_DISPLAY_LATENCY)
        NotifyHudPresented();
#endif
        return;
    }

    static Fortress::Video::FFontManager GFallbackHudFont = {};
    static bool GFallbackHudFontReady = false;
    if (!GFallbackHudFontReady) {
        GFallbackHudFontReady = GFallbackHudFont.Initialize();
    }

    const bool terminalMode = FKernelCommandConsole::IsTerminalModeEnabled();
    static bool GTerminalRevealPreviouslyActive = false;
    static Fortress::Core::uint32 GTerminalRevealTicks = 0u;
    static constexpr Fortress::Core::uint32 GTerminalRevealDurationTicks = 2u;

    if (terminalMode && !GTerminalRevealPreviouslyActive) {
        GTerminalRevealTicks = 0u;
    }
    if (!terminalMode) {
        GTerminalRevealTicks = 0u;
    }
    if (terminalMode && GTerminalRevealTicks < GTerminalRevealDurationTicks) {
        GTerminalRevealTicks++;
    }
    GTerminalRevealPreviouslyActive = terminalMode;

    Fortress::Core::uint8 terminalRevealAlphaScale = 255u;
    if (terminalMode) {
        const Fortress::Core::uint32 tick =
            (GTerminalRevealTicks > GTerminalRevealDurationTicks) ? GTerminalRevealDurationTicks : GTerminalRevealTicks;
        const Fortress::Core::uint32 alpha = (tick * 255u) / GTerminalRevealDurationTicks;
        terminalRevealAlphaScale = static_cast<Fortress::Core::uint8>((alpha > 255u) ? 255u : alpha);
    }

    if (!terminalMode) {
        const FDesktopThemePalette &theme = GetDesktopThemePalette();
        const Fortress::Core::int32 screenWidth = static_cast<Fortress::Core::int32>(context.FrameContext.BackSurface.Desc.Width);
        const Fortress::Core::int32 screenHeight = static_cast<Fortress::Core::int32>(context.FrameContext.BackSurface.Desc.Height);

        const Fortress::Core::int32 topStripX = 0;
        const Fortress::Core::int32 topStripY = 0;
        const Fortress::Core::int32 topStripW = screenWidth;
        const Fortress::Core::int32 topStripH = 42;
        DrawHorizontalGradientRect(context.FrameContext.BackSurface,
                                   topStripX,
                                   topStripY,
                                   topStripW,
                                   topStripH,
                                   Fortress::Video::FColor::RGB(140, 40, 48),
                                   Fortress::Video::FColor::RGB(86, 28, 38));
        DrawFilledRect(context.FrameContext.BackSurface,
                       topStripX,
                       topStripY + topStripH - 2,
                       topStripW,
                       2,
                       theme.Border);

        const Fortress::Core::int32 taskbarH = 62;
        const Fortress::Core::int32 taskbarY = screenHeight - taskbarH;

        const Fortress::Core::int32 desktopBodyY = topStripH;
        const Fortress::Core::int32 desktopBodyH = taskbarY - desktopBodyY;
        if (desktopBodyH > 0) {
            DrawVerticalGradientRect(context.FrameContext.BackSurface,
                                     0,
                                     desktopBodyY,
                                     screenWidth,
                                     desktopBodyH,
                                     Fortress::Video::FColor::RGB(98, 22, 32),
                                     Fortress::Video::FColor::RGB(46, 10, 20));

            DrawDiagonalNoiseOverlay(context.FrameContext.BackSurface,
                                     0,
                                     desktopBodyY,
                                     screenWidth,
                                     desktopBodyH,
                                     Fortress::Video::FColor{.R = 255u, .G = 230u, .B = 230u, .A = 8u},
                                     14);
        }

        DrawFilledRectAlpha(context.FrameContext.BackSurface,
                            0,
                            taskbarY - 4,
                            screenWidth,
                            taskbarH + 6,
                            Fortress::Video::FColor{.R = theme.Shadow.R,
                                                    .G = theme.Shadow.G,
                                                    .B = theme.Shadow.B,
                                                    .A = 72u});
        DrawVerticalGradientRectAlpha(context.FrameContext.BackSurface,
                                      0,
                                      taskbarY,
                                      screenWidth,
                                      taskbarH,
                                      Fortress::Video::FColor{.R = 78u, .G = 90u, .B = 108u, .A = 172u},
                                      Fortress::Video::FColor{.R = 34u, .G = 42u, .B = 56u, .A = 176u});
        DrawFilledRect(context.FrameContext.BackSurface,
                       0,
                       taskbarY,
                       screenWidth,
                       1,
                       Fortress::Video::FColor::RGB(188, 204, 226));

        const Fortress::Core::int32 buttonY = taskbarY + 11;
        const Fortress::Core::int32 buttonH = 40;
        const Fortress::Core::int32 buttonCornerRadius = 8;
        const Fortress::Core::int32 buttonBorderInset = (buttonCornerRadius > 3) ? (buttonCornerRadius - 3) : 1;
        const Fortress::Core::int32 buttonGap = 12;
        const Fortress::Core::int32 startButtonX = 14;
        Fortress::Core::int32 startButtonW = 118;
        Fortress::Core::int32 terminalButtonW = 150;
        Fortress::Core::int32 shutdownButtonW = 150;
        if (startButtonX + startButtonW + buttonGap + terminalButtonW + buttonGap + shutdownButtonW > screenWidth - 16) {
            startButtonW = 96;
            terminalButtonW = 124;
            shutdownButtonW = 124;
        }
        const Fortress::Core::int32 terminalButtonX = startButtonX + startButtonW + buttonGap;
        const Fortress::Core::int32 shutdownButtonX = terminalButtonX + terminalButtonW + buttonGap;

        DrawRoundedVerticalGradientRectAlpha(context.FrameContext.BackSurface,
                             startButtonX,
                             buttonY,
                             startButtonW,
                             buttonH,
                                             buttonCornerRadius,
                             Fortress::Video::FColor{.R = 130u, .G = 156u, .B = 196u, .A = 148u},
                             Fortress::Video::FColor{.R = 84u, .G = 112u, .B = 152u, .A = 160u});
        DrawTaperedHorizontalLineAlpha(context.FrameContext.BackSurface,
                           startButtonX,
                           buttonY,
                           startButtonW,
                           buttonBorderInset,
                           Fortress::Video::FColor::RGB(218, 232, 248));
        DrawTaperedHorizontalLineAlpha(context.FrameContext.BackSurface,
                           startButtonX,
                           buttonY + buttonH - 1,
                           startButtonW,
                           buttonBorderInset,
                           theme.Border);
        DrawRoundedRectHints(context.FrameContext.BackSurface,
                             startButtonX,
                             buttonY,
                             startButtonW,
                             buttonH,
                             Fortress::Video::FColor{.R = 0u, .G = 0u, .B = 0u, .A = 34u});

        DrawRoundedVerticalGradientRectAlpha(context.FrameContext.BackSurface,
                             terminalButtonX,
                             buttonY,
                             terminalButtonW,
                             buttonH,
                                             buttonCornerRadius,
                             Fortress::Video::FColor{.R = 126u, .G = 138u, .B = 156u, .A = 138u},
                             Fortress::Video::FColor{.R = 78u, .G = 90u, .B = 108u, .A = 148u});
        DrawTaperedHorizontalLineAlpha(context.FrameContext.BackSurface,
                           terminalButtonX,
                           buttonY,
                           terminalButtonW,
                           buttonBorderInset,
                           Fortress::Video::FColor::RGB(194, 208, 226));
        DrawTaperedHorizontalLineAlpha(context.FrameContext.BackSurface,
                           terminalButtonX,
                           buttonY + buttonH - 1,
                           terminalButtonW,
                           buttonBorderInset,
                           theme.Border);
        DrawRoundedRectHints(context.FrameContext.BackSurface,
                             terminalButtonX,
                             buttonY,
                             terminalButtonW,
                             buttonH,
                             Fortress::Video::FColor{.R = 0u, .G = 0u, .B = 0u, .A = 34u});

        DrawRoundedVerticalGradientRectAlpha(context.FrameContext.BackSurface,
                             shutdownButtonX,
                             buttonY,
                             shutdownButtonW,
                             buttonH,
                                             buttonCornerRadius,
                             Fortress::Video::FColor{.R = 150u, .G = 92u, .B = 100u, .A = 148u},
                             Fortress::Video::FColor{.R = 106u, .G = 54u, .B = 66u, .A = 160u});
        DrawTaperedHorizontalLineAlpha(context.FrameContext.BackSurface,
                           shutdownButtonX,
                           buttonY,
                           shutdownButtonW,
                           buttonBorderInset,
                           Fortress::Video::FColor::RGB(232, 192, 198));
        DrawTaperedHorizontalLineAlpha(context.FrameContext.BackSurface,
                           shutdownButtonX,
                           buttonY + buttonH - 1,
                           shutdownButtonW,
                           buttonBorderInset,
                           theme.Border);
        DrawRoundedRectHints(context.FrameContext.BackSurface,
                             shutdownButtonX,
                             buttonY,
                             shutdownButtonW,
                             buttonH,
                             Fortress::Video::FColor{.R = 0u, .G = 0u, .B = 0u, .A = 36u});

        if (GFallbackHudFontReady) {
            DrawTextLine(context.FrameContext.BackSurface,
                         GFallbackHudFont,
                         "FORTRESS DESKTOP",
                         2,
                         theme.TextSecondary,
                         Fortress::Video::FColor::RGB(128, 36, 46),
                         topStripX + 16,
                         topStripY + 14);

            DrawTextLine(context.FrameContext.BackSurface,
                         GFallbackHudFont,
                         "Start",
                         2,
                         theme.TextSecondary,
                         Fortress::Video::FColor::RGB(92, 122, 166),
                         startButtonX + 24,
                         buttonY + 12);

            DrawTextLine(context.FrameContext.BackSurface,
                         GFallbackHudFont,
                         "Terminal",
                         2,
                         theme.TextSecondary,
                         Fortress::Video::FColor::RGB(66, 76, 96),
                         terminalButtonX + 22,
                         buttonY + 12);

            DrawTextLine(context.FrameContext.BackSurface,
                         GFallbackHudFont,
                         "Shutdown",
                         2,
                         theme.TextSecondary,
                         Fortress::Video::FColor::RGB(94, 44, 56),
                         shutdownButtonX + 22,
                         buttonY + 12);
        }
    #if defined(FORTRESS_EXPERIMENTAL_DISPLAY_LATENCY)
        NotifyHudPresented();
    #endif
        return;
    }

    const Fortress::Core::int32 textX = panelX + 12;
    const Fortress::Core::int32 separatorX = panelX + 8;
    const Fortress::Core::int32 separatorWidth = panelWidth - 16;
    const Fortress::Core::int32 panelBottomY = panelY + panelHeight - 10;
    const Fortress::Video::FColor panelTopColor = terminalMode ? Fortress::Video::FColor::RGB(224, 108, 108)
                                                                : Fortress::Video::FColor::RGB(206, 226, 248);
    const Fortress::Video::FColor panelBottomColor = terminalMode ? Fortress::Video::FColor::RGB(118, 34, 48)
                                                                   : Fortress::Video::FColor::RGB(132, 166, 214);

    if (terminalMode) {
        DrawFilledRectAlpha(context.FrameContext.BackSurface,
                            panelX + 8,
                            panelY + 10,
                            panelWidth,
                            panelHeight,
                            ScaleAlpha(Fortress::Video::FColor{.R = 0u, .G = 0u, .B = 0u, .A = 86u},
                                       terminalRevealAlphaScale));
    }

    if (terminalMode) {
        DrawVerticalGradientRectAlpha(context.FrameContext.BackSurface,
                                      panelX,
                                      panelY,
                                      panelWidth,
                                      panelHeight,
                                      Fortress::Video::FColor{.R = panelTopColor.R,
                                                              .G = panelTopColor.G,
                                                              .B = panelTopColor.B,
                                                              .A = terminalRevealAlphaScale},
                                      Fortress::Video::FColor{.R = panelBottomColor.R,
                                                              .G = panelBottomColor.G,
                                                              .B = panelBottomColor.B,
                                                              .A = terminalRevealAlphaScale});
    } else {
        DrawVerticalGradientRect(context.FrameContext.BackSurface,
                                 panelX,
                                 panelY,
                                 panelWidth,
                                 panelHeight,
                                 panelTopColor,
                                 panelBottomColor);
    }

    if (terminalMode) {
        DrawFilledRectAlpha(context.FrameContext.BackSurface,
                            panelX,
                            panelY,
                            panelWidth,
                            30,
                            Fortress::Video::FColor{.R = 122u,
                                                    .G = 34u,
                                                    .B = 42u,
                                                    .A = terminalRevealAlphaScale});
        DrawHorizontalGradientRectAlpha(context.FrameContext.BackSurface,
                                        panelX,
                                        panelY,
                                        panelWidth,
                                        30,
                                        ScaleAlpha(Fortress::Video::FColor{.R = 255u, .G = 255u, .B = 255u, .A = 22u},
                                                   terminalRevealAlphaScale),
                                        ScaleAlpha(Fortress::Video::FColor{.R = 255u, .G = 255u, .B = 255u, .A = 4u},
                                                   terminalRevealAlphaScale));
        DrawFilledRectAlpha(context.FrameContext.BackSurface,
                            panelX,
                            panelY,
                            panelWidth,
                            1,
                            Fortress::Video::FColor{.R = 238u,
                                                    .G = 184u,
                                                    .B = 184u,
                                                    .A = terminalRevealAlphaScale});
        DrawFilledRectAlpha(context.FrameContext.BackSurface,
                            panelX,
                            panelY + panelHeight - 1,
                            panelWidth,
                            1,
                            Fortress::Video::FColor{.R = 56u,
                                                    .G = 20u,
                                                    .B = 28u,
                                                    .A = terminalRevealAlphaScale});
        DrawFilledRectAlpha(context.FrameContext.BackSurface,
                            panelX - 2,
                            panelY - 2,
                            panelWidth + 4,
                            panelHeight + 4,
                            ScaleAlpha(Fortress::Video::FColor{.R = 86u, .G = 164u, .B = 255u, .A = 26u},
                                       terminalRevealAlphaScale));
        DrawRoundedRectHints(context.FrameContext.BackSurface,
                             panelX,
                             panelY,
                             panelWidth,
                             panelHeight,
                             ScaleAlpha(Fortress::Video::FColor{.R = 0u, .G = 0u, .B = 0u, .A = 42u},
                                        terminalRevealAlphaScale));
    }

    const auto DrawHudTextLine = [&](const char *line, Fortress::Video::FColor fg, Fortress::Core::int32 y) {
        const Fortress::Video::FColor bg =
            SampleVerticalGradientColor(panelTopColor, panelBottomColor, panelHeight, y - panelY);
        DrawTextLine(context.FrameContext.BackSurface, GFallbackHudFont, line, 2, fg, bg, textX, y);
    };

    if (context.Runtime.Console == nullptr) {
        if (GFallbackHudFontReady) {
            DrawHudTextLine("FORTRESS HUD", Fortress::Video::FColor::RGB(28, 54, 92), panelY + 12);
            DrawHudTextLine("CONSOLE UNAVAILABLE",
                            Fortress::Video::FColor::RGB(108, 54, 38),
                            panelY + 12 + Fortress::Video::FTextRenderer::GetLineAdvance(2));
        }
                #if defined(FORTRESS_EXPERIMENTAL_DISPLAY_LATENCY)
                    NotifyHudPresented();
                #endif
        return;
    }

    if (!GFallbackHudFontReady) {
                #if defined(FORTRESS_EXPERIMENTAL_DISPLAY_LATENCY)
                    NotifyHudPresented();
                #endif
        return;
    }

    // Render a direct command-log fallback last so runtime messages persist on screen.
    Fortress::Core::int32 cursorY = panelY + 12;
    const Fortress::Core::int32 lineAdvance = Fortress::Video::FTextRenderer::GetLineAdvance(2);
    const Fortress::Core::int32 glyphAdvance = Fortress::Video::FTextRenderer::GetGlyphAdvance(2);
    const Fortress::Core::int32 inputLineY = panelBottomY - lineAdvance;
    const Fortress::Core::int32 pinnedLogHintY = inputLineY - lineAdvance - 6;
    const Fortress::Core::int32 logBottomY = pinnedLogHintY - 8;

    if (terminalMode) {
        DrawHudTextLine("TERMINAL", Fortress::Video::FColor::RGB(64, 14, 22), cursorY);
        cursorY += lineAdvance;

        DrawFilledRect(context.FrameContext.BackSurface,
                       separatorX,
                       cursorY,
                       separatorWidth,
                       2,
                       Fortress::Video::FColor::RGB(86, 128, 178));

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
            DrawHudTextLine(line, Fortress::Video::FColor::RGB(72, 18, 26), drawY);
            drawY += lineAdvance;
            if (drawY > logBottomY) {
                break;
            }
        }

        DrawHudTextLine("TERMINAL ON", Fortress::Video::FColor::RGB(86, 24, 34), pinnedLogHintY);

        DrawFilledRect(context.FrameContext.BackSurface,
                       separatorX,
                       inputLineY - 4,
                       separatorWidth,
                       2,
                       Fortress::Video::FColor::RGB(92, 134, 182));

        const Fortress::Video::FColor inputBg = Fortress::Video::FColor::RGB(92, 24, 36);
        DrawTextLine(context.FrameContext.BackSurface,
                     GFallbackHudFont,
                     "INPUT> ",
                     2,
                     Fortress::Video::FColor::RGB(244, 212, 212),
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
        DrawFilledRectAlpha(context.FrameContext.BackSurface,
                            inputStartX - 6,
                            inputLineY - 3,
                            inputRightX - inputStartX + 8,
                            lineAdvance + 4,
                            Fortress::Video::FColor{.R = 0u, .G = 0u, .B = 0u, .A = 42u});
        const Fortress::Core::int32 inputWidthPixels = inputRightX - inputStartX;
        Fortress::Core::usize maxInputChars = 1u;
        if (inputWidthPixels > glyphAdvance) {
            maxInputChars = static_cast<Fortress::Core::usize>(inputWidthPixels / glyphAdvance);
        }

        static Fortress::Core::uint32 GInputCaretFrameCounter = 0u;
        GInputCaretFrameCounter++;
        const bool caretVisible = ((GInputCaretFrameCounter / 18u) % 2u) == 0u;

        char inputDisplayLine[256] = {};
        BuildInputDisplayLine(FKernelCommandConsole::GetCommandBuffer(),
                              maxInputChars,
                              caretVisible,
                              inputDisplayLine,
                              sizeof(inputDisplayLine));

        DrawTextLine(context.FrameContext.BackSurface,
                     GFallbackHudFont,
                     inputDisplayLine,
                     2,
                     Fortress::Video::FColor::RGB(255, 232, 232),
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

                #if defined(FORTRESS_EXPERIMENTAL_DISPLAY_LATENCY)
                    NotifyHudPresented();
                #endif
        return;
    }

    DrawHudTextLine("FORTRESS HUD", Fortress::Video::FColor::RGB(34, 58, 94), cursorY);

    cursorY += lineAdvance;
    DrawHudTextLine("TYPE HELP FOR COMMANDS", Fortress::Video::FColor::RGB(56, 82, 120), cursorY);

    cursorY += lineAdvance;
        DrawFilledRect(context.FrameContext.BackSurface,
                                     separatorX,
                                     cursorY,
                                     separatorWidth,
                                     2,
                                     Fortress::Video::FColor::RGB(86, 128, 178));

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
        DrawHudTextLine(cachedRendererLine, Fortress::Video::FColor::RGB(48, 78, 116), cursorY);
        cursorY += lineAdvance;
    }
    if (cachedDisplayModeLine[0] != '\0') {
        DrawHudTextLine(cachedDisplayModeLine, Fortress::Video::FColor::RGB(48, 78, 116), cursorY);
        cursorY += lineAdvance;
    }
    if (cachedVfsLine[0] != '\0') {
        DrawHudTextLine(cachedVfsLine, Fortress::Video::FColor::RGB(52, 90, 124), cursorY);
        cursorY += lineAdvance;
    }
    if (cachedDesktopSmokeLine[0] != '\0') {
        DrawHudTextLine(cachedDesktopSmokeLine, Fortress::Video::FColor::RGB(52, 90, 124), cursorY);
        cursorY += lineAdvance;
    }
    if (cachedDsksurfSmokeLine[0] != '\0') {
        DrawHudTextLine(cachedDsksurfSmokeLine, Fortress::Video::FColor::RGB(52, 90, 124), cursorY);
        cursorY += lineAdvance;
    }
    if (cachedDesktopStatsLine[0] != '\0') {
        DrawHudTextLine(cachedDesktopStatsLine, Fortress::Video::FColor::RGB(46, 82, 122), cursorY);
        cursorY += lineAdvance;
    }

    if (FKernelCommandConsole::IsHudParallelStatsEnabled()) {
        DrawFilledRect(context.FrameContext.BackSurface,
                       separatorX,
                       cursorY,
                       separatorWidth,
                       2,
                       Fortress::Video::FColor::RGB(86, 128, 178));
        cursorY += lineAdvance;

        DrawHudTextLine("PARALLEL HUD", Fortress::Video::FColor::RGB(48, 78, 116), cursorY);
        cursorY += lineAdvance;

        if (cachedCoreDispatchLine[0] != '\0') {
            DrawHudTextLine(cachedCoreDispatchLine, Fortress::Video::FColor::RGB(46, 82, 122), cursorY);
            cursorY += lineAdvance;
        }
        if (cachedCoreDispatchPerCoreLine[0] != '\0') {
            DrawHudTextLine(cachedCoreDispatchPerCoreLine, Fortress::Video::FColor::RGB(46, 82, 122), cursorY);
            cursorY += lineAdvance;
        }
        if (cachedApWorkerLine[0] != '\0') {
            DrawHudTextLine(cachedApWorkerLine, Fortress::Video::FColor::RGB(46, 82, 122), cursorY);
            cursorY += lineAdvance;
        }
        if (cachedParallelProbeLine[0] != '\0') {
            DrawHudTextLine(cachedParallelProbeLine, Fortress::Video::FColor::RGB(46, 82, 122), cursorY);
            cursorY += lineAdvance;
        }
    }

    DrawFilledRect(context.FrameContext.BackSurface,
                   separatorX,
                   cursorY,
                   separatorWidth,
                   2,
                   Fortress::Video::FColor::RGB(86, 128, 178));

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
                                    Fortress::Video::FColor::RGB(70, 92, 126),
                                    cursorY);
                } else if (detailMode == FKernelCommandConsole::EHudLogDetailMode::Warn) {
                    DrawHudTextLine("NO WARN LINES (WARN|DEGRADED|RETRY|FALLBACK)",
                                    Fortress::Video::FColor::RGB(70, 92, 126),
                                    cursorY);
                } else {
                    DrawHudTextLine("NO ISSUE/WARN LINES", Fortress::Video::FColor::RGB(70, 92, 126), cursorY);
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
                                    : Fortress::Video::FColor::RGB(252, 232, 174);
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
                DrawHudTextLine(line, Fortress::Video::FColor::RGB(40, 72, 108), drawY);
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
    DrawHudTextLine(pinnedLogHint, Fortress::Video::FColor::RGB(62, 92, 130), pinnedLogHintY);

    DrawFilledRect(context.FrameContext.BackSurface,
                   separatorX,
                   inputLineY - 4,
                   separatorWidth,
                   2,
                   Fortress::Video::FColor::RGB(92, 134, 182));

    const Fortress::Video::FColor inputBg =
        SampleVerticalGradientColor(panelTopColor, panelBottomColor, panelHeight, inputLineY - panelY);
    DrawTextLine(context.FrameContext.BackSurface,
                 GFallbackHudFont,
                 "INPUT> ",
                 2,
                 Fortress::Video::FColor::RGB(36, 66, 104),
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
                 Fortress::Video::FColor::RGB(24, 54, 88),
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

#if defined(FORTRESS_EXPERIMENTAL_DISPLAY_LATENCY)
    NotifyHudPresented();
#endif

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
    // Keep desktop shell as the default visual baseline; scene can be re-enabled by explicit changes.
    options.RenderScene = false;
    options.WireframeEnabled = FKernelCommandControlPlane::IsWireframeEnabled();
    options.RenderSurfaceSelfTest = FKernelCommandConsole::IsRenderSurfaceSelfTestEnabled();
    options.RenderDesktopSurfaces = true;
    options.RenderHud = true;
    options.RenderInputPulse = FKernelCommandControlPlane::ConsumeCommandPulseFrame();
    options.RenderCursorOverlay = FKernelCommandControlPlane::IsCursorOverlayEnabled();
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
    if (!frameContext.IsValid()) {
        // Recover from transient mode refresh failures by reusing current mode and acquired back surface.
        frameContext.Mode = runtime.DisplayManager->GetMode();
        frameContext.BackSurface = runtime.DisplayManager->AcquireBackSurface();
        frameContext.ModeChanged = false;
    }
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

    // In desktop (non-terminal) mode, keep interactive surface controls visible above HUD decorations.
    if (!FKernelCommandConsole::IsTerminalModeEnabled() && desktopRuntime.GetOverlay().IsReady()) {
        desktopRuntime.GetOverlay().Render(frameContext.BackSurface);
    }

    // Keep renderer self-test visible by compositing it last.
    if (ShouldRunSurfaceSelfTestPass(passContext)) {
        ExecuteSurfaceSelfTestPass(passContext);
    }

    runtime.DisplayManager->Present();
}

} // namespace Fortress::Kernel
