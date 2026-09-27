#include "Fortress/Kernel/FKernelHudOverlay.hpp"

#include <cstddef>
#include <cstdint>

#include "Fortress/Kernel/FKernelConfig.hpp"
#include "Fortress/Kernel/FKernelCommandConsole.hpp"
#include "Fortress/Video/FVideoConsole.hpp"

namespace Fortress::Kernel {

using Fortress::Video::FColor;

static void AppendChar(char *dst, size_t dstSize, size_t &offset, char c) {
    if (offset + 1 >= dstSize) {
        return;
    }
    dst[offset++] = c;
    dst[offset] = '\0';
}

static void AppendString(char *dst, size_t dstSize, size_t &offset, const char *src) {
    if (src == nullptr) {
        return;
    }
    for (size_t i = 0; src[i] != '\0'; i++) {
        AppendChar(dst, dstSize, offset, src[i]);
    }
}

static void AppendUInt(char *dst, size_t dstSize, size_t &offset, uint64_t value) {
    char rev[24] = {};
    size_t n = 0;
    if (value == 0) {
        AppendChar(dst, dstSize, offset, '0');
        return;
    }
    while (value > 0 && n < sizeof(rev)) {
        rev[n++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    while (n > 0) {
        AppendChar(dst, dstSize, offset, rev[n - 1]);
        n--;
    }
}

static void AppendHex(char *dst, size_t dstSize, size_t &offset, uint64_t value) {
    static const char Hex[] = "0123456789ABCDEF";
    AppendString(dst, dstSize, offset, "0x");

    bool started = false;
    for (int shift = 60; shift >= 0; shift -= 4) {
        const uint8_t nibble = static_cast<uint8_t>((value >> shift) & 0xFu);
        if (!started && nibble == 0 && shift != 0) {
            continue;
        }
        started = true;
        AppendChar(dst, dstSize, offset, Hex[nibble]);
    }
}

void FKernelHudOverlay::Render(Fortress::Video::FVideoConsole &console,
                               const Fortress::Video::FVideoSurfaceView &surface,
                               Fortress::Core::uint64 fpsValue,
                               bool wireframeEnabled,
                               const Fortress::Memory::FPhysicalMemoryStats &pmmStats,
                               const Fortress::Memory::FVirtualMemoryStats &vmmStats,
                               const Fortress::Memory::FKernelHeapStats &heapStats,
                               Fortress::Core::uint64 arenaUsedBytes) {
    char line[96] = {};
    size_t pos = 0;

    AppendString(line, sizeof(line), pos, "FPS: ");
    AppendUInt(line, sizeof(line), pos, fpsValue);
    AppendString(line, sizeof(line), pos, "  FIXED: 60");

    console.SetCursor(FKernelConfig::HudOriginX, FKernelConfig::HudOriginY);
    console.SetColors(FColor::RGB(255, 255, 210), FColor::RGB(0, 0, 0));
    console.PrintLine(surface, "FORTRESS OS");
    console.PrintLine(surface, line);
    console.PrintLine(surface, "MEMORY MODULE: READY");
    console.PrintLine(surface, "VIDEO SUBSYSTEM: READY");
    console.PrintLine(surface, wireframeEnabled ? "SOFTWARE 3D: WIREFRAME ON" : "SOFTWARE 3D: WIREFRAME OFF");

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "PMM FREE PAGES: ");
    AppendUInt(line, sizeof(line), pos, pmmStats.FreePages);
    console.PrintLine(surface, line);

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "PMM TOTAL PAGES: ");
    AppendUInt(line, sizeof(line), pos, pmmStats.TotalPages);
    AppendString(line, sizeof(line), pos, " BITMAP BYTES: ");
    AppendUInt(line, sizeof(line), pos, pmmStats.BitmapBytes);
    console.PrintLine(surface, line);

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "VMM ROOT: ");
    AppendHex(line, sizeof(line), pos, vmmStats.RootTablePhysical);
    AppendString(line, sizeof(line), pos, " MAPPED: ");
    AppendUInt(line, sizeof(line), pos, vmmStats.MappedPages);
    AppendString(line, sizeof(line), pos, " TABLES: ");
    AppendUInt(line, sizeof(line), pos, vmmStats.TablePages);
    console.PrintLine(surface, line);

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "KHEAP USED: ");
    AppendUInt(line, sizeof(line), pos, heapStats.UsedBytes);
    AppendString(line, sizeof(line), pos, " FREE: ");
    AppendUInt(line, sizeof(line), pos, heapStats.FreeBytes);
    console.PrintLine(surface, line);

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "ARENA USED BYTES: ");
    AppendUInt(line, sizeof(line), pos, arenaUsedBytes);
    console.PrintLine(surface, line);

    console.PrintLine(surface, "CMD:");

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "> ");
    AppendString(line, sizeof(line), pos, FKernelCommandConsole::GetCommandBuffer());
    console.PrintLine(surface, line);

    for (size_t i = 0; i < FKernelCommandConsole::GetLogCount(); i++) {
        console.PrintLine(surface, FKernelCommandConsole::GetLogLine(i));
    }
}

} // namespace Fortress::Kernel
