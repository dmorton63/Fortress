#include "Fortress/Kernel/FKernelCommandConsole.hpp"

#include <cstddef>
#include <cstdint>

#include "Fortress/Kernel/FKernelConfig.hpp"
#include "Fortress/Kernel/FKernelCommandControlPlane.hpp"
#include "Fortress/Kernel/FKernelCommandDesktopCursor.hpp"
#include "Fortress/Kernel/FKernelCommandHeap.hpp"
#include "Fortress/Kernel/FKernelCommandMemoryMap.hpp"
#include "Fortress/Kernel/FKernelApWorker.hpp"
#include "Fortress/Kernel/FKernelCoreDispatch.hpp"
#include "Fortress/Kernel/FKernelCommandRuntimeControl.hpp"
#include "Fortress/Kernel/FKernelCommandUtility.hpp"
#include "Fortress/Kernel/FKernelCommandXhci.hpp"
#include "Fortress/Kernel/FCpuCoreManager.hpp"
#include "Fortress/Kernel/FDesktopCompositor.hpp"
#include "Fortress/Kernel/FDesktopInputRouter.hpp"
#include "Fortress/Kernel/FEventManager.hpp"
#include "Fortress/Kernel/FKeyboardManager.hpp"
#include "Fortress/Kernel/FKernelInputEventPlane.hpp"
#include "Fortress/Kernel/FKernelRuntimeIds.hpp"
#include "Fortress/Kernel/FKernelSchedulerEventPlane.hpp"
#include "Fortress/Kernel/FMessageBus.hpp"
#include "Fortress/Kernel/FPortManager.hpp"
#include "Fortress/Kernel/FServiceRegistry.hpp"
#include "Fortress/Kernel/FServiceRegistryDatabaseAdapter.hpp"
#include "Fortress/Memory/FMmio.hpp"
#include "Fortress/Memory/FDmaMemoryManager.hpp"
#include "Fortress/Memory/FPinnedMappingManager.hpp"
#include "Fortress/Memory/FKernelHeap.hpp"
#include "Fortress/Memory/FPhysicalMemoryManager.hpp"
#include "Fortress/Memory/FVirtualMemoryManager.hpp"
#include "Fortress/Platform/FXhciMmioRegisters.hpp"
#include "Fortress/Platform/FXhciPciDiscovery.hpp"
#include "Fortress/Runtime/FRuntime.hpp"
#include "Fortress/Storage/IBlockDevice.hpp"
#include "Fortress/Storage/FVirtualFileSystem.hpp"
#include "Fortress/Video/FTextRenderer.hpp"
#include "Fortress/Video/FVideoConsole.hpp"

namespace Fortress::Kernel {

using Fortress::Memory::FDmaBuffer;
using Fortress::Memory::FDmaMemoryManager;
using Fortress::Memory::FKernelHeap;
using Fortress::Memory::FMmio;
using Fortress::Memory::FPinnedMapping;
using Fortress::Memory::FPinnedMappingManager;
using Fortress::Memory::FPhysicalMemoryManager;
using Fortress::Memory::FVirtualMemoryManager;
using Fortress::Kernel::FKeyboardInputEvent;
using Fortress::Kernel::FKeyboardManager;
using Fortress::Platform::FXhciMmioRegisters;
using Fortress::Platform::FXhciControllerInfo;
using Fortress::Platform::FXhciPciDiscovery;
using Fortress::Storage::FVirtualFileSystem;
using Fortress::Storage::FVirtualFileSystemRoute;
using Fortress::Storage::FVirtualFileSystemStats;
using Fortress::Video::ETextShaperMode;
using Fortress::Video::FTextRenderer;
using Fortress::Video::FTextShaperConfig;
using Fortress::Kernel::FEventManager;
using Fortress::Kernel::FEventManagerStats;
using Fortress::Kernel::FCpuCoreManager;
using Fortress::Kernel::FCpuCoreManagerStats;
using Fortress::Kernel::FKernelApWorker;
using Fortress::Kernel::FKernelCoreDispatch;
using Fortress::Kernel::FKernelCoreDispatchStats;
using Fortress::Kernel::FKernelCommandControlPlane;
using Fortress::Kernel::FKernelCommandControlStats;
using Fortress::Kernel::EKeyboardLayout;
using Fortress::Kernel::FDesktopCompositor;
using Fortress::Kernel::FDesktopCompositorStats;
using Fortress::Kernel::FDesktopInputRouter;
using Fortress::Kernel::FDesktopInputRouterStats;
using Fortress::Kernel::FKernelInputEventPlane;
using Fortress::Kernel::FKernelInputEventStats;
using Fortress::Kernel::FKernelSchedulerEventPlane;
using Fortress::Kernel::FKernelSchedulerEventStats;
using Fortress::Kernel::FMessageBus;
using Fortress::Kernel::FMessageBusStats;
using Fortress::Kernel::FServiceRegistry;
using Fortress::Kernel::FServiceRegistryStats;

static bool GWireframe = false;
static bool GPaused = false;

static constexpr size_t GMaxLogLines = 32;
static constexpr size_t GMaxBootLogLines = 128;

static char GCommandBuffer[64] = {};
static size_t GCommandLength = 0;

#if defined(FORTRESS_PARALLEL_PROBE_AUTORUN)
static constexpr const char *GParallelProbeAutorunCommands[] = {
    "parallel on",
    "parallel drain on",
    "parallel",
    "parallelprobe",
    "parallelprobe",
#if defined(FORTRESS_PARALLEL_PROBE_AUTORUN_MICRO_CANARY)
    "parallelcanary 1",
    "parallelprobe",
    "parallelprobe",
    "parallelprobe",
    "halt",
#else
    "halt",
#endif
};
static uint32_t GParallelProbeAutorunCommandIndex = 0u;
static uint32_t GParallelProbeAutorunCooldownTicks = 0u;
static constexpr uint32_t GParallelProbeAutorunInitialDelayTicks = 120u;
static constexpr uint32_t GParallelProbeAutorunInterCommandDelayTicks = 60u;
#endif

static char GLogLines[GMaxLogLines][96] = {};
static size_t GLogCount = 0;
static char GBootLogLines[GMaxBootLogLines][96] = {};
static size_t GBootLogCount = 0;
static bool GBootLogFrozen = false;
static FKernelCommandConsole::EHudLogViewMode GHudLogViewMode = FKernelCommandConsole::EHudLogViewMode::Hidden;
static FKernelCommandConsole::EHudLogDetailMode GHudLogDetailMode = FKernelCommandConsole::EHudLogDetailMode::Tail;
static bool GTerminalModeEnabled = false;
static bool GHudParallelStatsEnabled = false;
static bool GSerialMirrorReady = false;
static bool GSerialMirrorFaulted = false;
static uint8_t GLastUsbConfigValue = 0;
static bool GHaveLastUsbConfigValue = false;
static uint8_t GLastUsbMscInterfaceNumber = 0;
static bool GHaveLastUsbMscInterface = false;
static uint8_t GLastUsbMscBulkInEndpointAddress = 0;
static uint8_t GLastUsbMscBulkOutEndpointAddress = 0;
static uint16_t GLastUsbMscBulkInMaxPacketSize = 0;
static uint16_t GLastUsbMscBulkOutMaxPacketSize = 0;
static bool GHaveLastUsbMscBulkPair = false;
enum class EUsbIntrinEndpointKind : uint8_t {
    Generic = 0,
    KeyboardBoot,
    MouseBoot,
};
static uint8_t GLastUsbInterruptInEndpointAddress = 0;
static uint16_t GLastUsbInterruptInMaxPacketSize = 0;
static uint8_t GLastUsbInterruptInInterval = 0;
static bool GHaveLastUsbInterruptInEndpoint = false;
static EUsbIntrinEndpointKind GLastUsbIntrinEndpointKind = EUsbIntrinEndpointKind::Generic;
static uint8_t GLastUsbKeyboardInterruptInEndpointAddress = 0;
static uint16_t GLastUsbKeyboardInterruptInMaxPacketSize = 0;
static uint8_t GLastUsbKeyboardInterruptInInterval = 0;
static bool GHaveLastUsbKeyboardInterruptInEndpoint = false;
static uint8_t GLastUsbMouseInterruptInEndpointAddress = 0;
static uint16_t GLastUsbMouseInterruptInMaxPacketSize = 0;
static uint8_t GLastUsbMouseInterruptInInterval = 0;
static bool GHaveLastUsbMouseInterruptInEndpoint = false;
static uint8_t GUsbBootKeyboardLastModifiers = 0;
static uint8_t GUsbBootKeyboardLastKeys[6] = {};
static bool GHaveUsbBootKeyboardLastReport = false;
enum class EHidLogMode : uint8_t {
    Compact,
    Core,
    Verbose,
};

enum class ECursorLatencyMode : uint8_t {
    Smooth,
    Responsive,
};

static EHidLogMode GHidLogMode = EHidLogMode::Verbose;
static ECursorLatencyMode GCursorLatencyMode = ECursorLatencyMode::Smooth;
static constexpr int32_t GHidSmoothScale = 16;
static int32_t GHidSmoothedNormXFp = 0;
static int32_t GHidSmoothedNormYFp = 0;
static int32_t GHidSmoothedMotionXFp = 0;
static int32_t GHidSmoothedMotionYFp = 0;
static bool GHidSmoothingInitialized = false;
static bool GHaveHidAbsoluteCursor = false;
static int32_t GHidCursorNormX = 0;
static int32_t GHidCursorNormY = 0;
static bool GCursorOverlayEnabled = false;
static bool GDesktopSurfaceOverlayEnabled = false;
static bool GCursorInvertX = false;
static bool GCursorInvertY = false;
static uint32_t GCursorSensitivityPercent = 100;
static uint8_t GHidButtonsDownMask = 0;
static uint8_t GHidButtonPressEdgesMask = 0;
static bool GHaveHidLogicalAxisRange = false;
static int32_t GHidLogicalMinX = 0;
static int32_t GHidLogicalMaxX = 65535;
static int32_t GHidLogicalMinY = 0;
static int32_t GHidLogicalMaxY = 65535;
static bool GHidAutoLogicalRangeInitialized = false;
static bool GHidAutoLogicalRangeReady = false;
static bool GHidAutoLogicalRangeActive = false;
static uint32_t GHidAutoLogicalRangeSamples = 0;
static int32_t GHidAutoLogicalMinX = 0;
static int32_t GHidAutoLogicalMaxX = 0;
static int32_t GHidAutoLogicalMinY = 0;
static int32_t GHidAutoLogicalMaxY = 0;
static uint32_t GHidAutoExpandBlendSamplesRemaining = 0;
static constexpr uint32_t GHidAutoExpandBlendSampleCount = 8u;
static int32_t GHidAutoExpandBlendStartNormX = 0;
static int32_t GHidAutoExpandBlendStartNormY = 0;
static uint32_t GHidAutoExpandCooldownSamplesRemaining = 0;
static uint8_t GHidAutoExpandBelowMinXStreak = 0;
static uint8_t GHidAutoExpandAboveMaxXStreak = 0;
static uint8_t GHidAutoExpandBelowMinYStreak = 0;
static uint8_t GHidAutoExpandAboveMaxYStreak = 0;
static bool GHidLogicalRangeProbeAttempted = false;
static bool GHaveHidLogicalRangeInterfaceHint = false;
static uint8_t GHidLogicalRangeInterfaceHint = 0;
static FKernelCommandConsole::FLongOperationYieldCallback GLongOperationYieldCallback = nullptr;
static constexpr uint64_t GLongPollYieldCadence = 4096ull;
static bool GXhciIntrinLoopBackgroundEnabled = false;
static bool GXhciIntrinLoopBackgroundRunning = false;
static uint32_t GXhciIntrinLoopBackgroundTickDivider = 120u;
static uint32_t GXhciIntrinLoopBackgroundTickCounter = 0u;
static bool GXhciIntrinBackgroundAutoPausedForApDrain = false;
static bool GXhciIntrinBackgroundHaveLastReport = false;
static uint8_t GXhciIntrinBackgroundLastReport[8] = {};
static uint32_t GXhciIntrinBackgroundLastReportLength = 0u;
static Fortress::Video::FVideoConsole *GBoundVideoConsole = nullptr;
static Fortress::Kernel::FDesktopCompositor *GBoundDesktopCompositor = nullptr;
static Fortress::Kernel::FDesktopInputRouter *GBoundDesktopInputRouter = nullptr;
static constexpr uint32_t GMaxCommandPortLeases = 8u;

struct FCachedCommandPortLease {
    bool InUse = false;
    uint16_t PortId = 0u;
    uint32_t LeaseId = 0u;
};

static FCachedCommandPortLease GCommandPortLeases[GMaxCommandPortLeases] = {};
static void ProcessCommand();

static int32_t FindCachedCommandPortLeaseIndex(uint16_t portId) {
    for (uint32_t i = 0u; i < GMaxCommandPortLeases; i++) {
        if (GCommandPortLeases[i].InUse && GCommandPortLeases[i].PortId == portId) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

static bool CacheCommandPortLease(uint16_t portId, uint32_t leaseId) {
    const int32_t existing = FindCachedCommandPortLeaseIndex(portId);
    if (existing >= 0) {
        GCommandPortLeases[existing].LeaseId = leaseId;
        return true;
    }

    for (uint32_t i = 0u; i < GMaxCommandPortLeases; i++) {
        if (!GCommandPortLeases[i].InUse) {
            GCommandPortLeases[i] = FCachedCommandPortLease{
                .InUse = true,
                .PortId = portId,
                .LeaseId = leaseId,
            };
            return true;
        }
    }

    return false;
}

static void DropCachedCommandPortLease(uint16_t portId) {
    const int32_t index = FindCachedCommandPortLeaseIndex(portId);
    if (index < 0) {
        return;
    }

    GCommandPortLeases[index] = FCachedCommandPortLease{};
}

static bool GetCachedCommandPortLease(uint16_t portId, uint32_t &outLeaseId) {
    outLeaseId = 0u;
    const int32_t index = FindCachedCommandPortLeaseIndex(portId);
    if (index < 0) {
        return false;
    }

    outLeaseId = GCommandPortLeases[index].LeaseId;
    return true;
}

static void YieldLongOperationPoll(uint64_t pollIndex) {
    if (pollIndex == 0 || GLongOperationYieldCallback == nullptr) {
        return;
    }

    if ((pollIndex % GLongPollYieldCadence) == 0ull) {
        GLongOperationYieldCallback();
    }
}

#if defined(FORTRESS_PARALLEL_PROBE_AUTORUN)
static void TickParallelProbeAutorun() {
    if (GParallelProbeAutorunCommandIndex >=
        (sizeof(GParallelProbeAutorunCommands) / sizeof(GParallelProbeAutorunCommands[0]))) {
        return;
    }

    if (GCommandLength != 0) {
        return;
    }

    if (GParallelProbeAutorunCooldownTicks > 0u) {
        GParallelProbeAutorunCooldownTicks--;
        return;
    }

    const char *command = GParallelProbeAutorunCommands[GParallelProbeAutorunCommandIndex];
    if (command == nullptr) {
        GParallelProbeAutorunCommandIndex++;
        GParallelProbeAutorunCooldownTicks = GParallelProbeAutorunInterCommandDelayTicks;
        return;
    }

    size_t length = 0;
    while (command[length] != '\0' && (length + 1u) < sizeof(GCommandBuffer)) {
        GCommandBuffer[length] = command[length];
        length++;
    }
    GCommandLength = length;
    GCommandBuffer[GCommandLength] = '\0';

    ProcessCommand();

    GParallelProbeAutorunCommandIndex++;
    GParallelProbeAutorunCooldownTicks = GParallelProbeAutorunInterCommandDelayTicks;
}
#endif

struct FAllocBlock {
    uint64_t Address;
    uint64_t PageCount;
};

struct FVirtualAllocBlock {
    uint64_t VirtualAddress;
    uint64_t PhysicalAddress;
    uint64_t PageCount;
    uint64_t Flags;
    bool InUse;
};

struct FVirtualFreeRange {
    uint64_t VirtualAddress;
    uint64_t PageCount;
};

struct FDmaAllocRecord {
    FDmaBuffer Buffer;
    bool InUse;
};

struct FPinnedAllocRecord {
    FPinnedMapping Mapping;
    bool InUse;
};

static FVirtualFreeRange GVirtualFreeRanges[64] = {};
static size_t GVirtualFreeRangeCount = 0;
static uint64_t GNextVmallocVirtual = Fortress::Kernel::FKernelConfig::VmallocBase;

static void PushLog(const char *line);
static void AppendChar(char *dst, size_t dstSize, size_t &offset, char c);
static void AppendString(char *dst, size_t dstSize, size_t &offset, const char *src);
static void AppendUInt(char *dst, size_t dstSize, size_t &offset, uint64_t value);
static void AppendHex(char *dst, size_t dstSize, size_t &offset, uint64_t value);
static void AppendSInt(char *dst, size_t dstSize, size_t &offset, int64_t value);
static int32_t FixedPointToIntRounded(int32_t value, int32_t scale);
static void AppendFixedPoint1Dec(char *dst, size_t dstSize, size_t &offset, int32_t value, int32_t scale);
static int32_t ClampSInt32(int32_t value, int32_t minValue, int32_t maxValue);
static bool IsHidVerboseMode();
static bool IsHidEdgeMode();
static const char *GetCursorLatencyModeName();
static void ResetCommandInputBuffer();
static void TickBackgroundCommands();
static int32_t InferFallbackLogicalMaxFromObserved(int32_t observedMax);
static int32_t ReadSignedLE(const volatile uint8_t *bytes, uint32_t length);
static uint32_t ReadUnsignedLE(const volatile uint8_t *bytes, uint32_t length);
static int32_t NormalizeHidAbsoluteAxis(uint16_t rawValue, int32_t logicalMin, int32_t logicalMax);
static void TryUpdateAutoHidAxisLogicalRange(uint16_t absX, uint16_t absY);
static bool ParseHidAxisLogicalRange(const volatile uint8_t *descriptor,
                                     uint32_t descriptorLength,
                                     int32_t &outMinX,
                                     int32_t &outMaxX,
                                     int32_t &outMinY,
                                     int32_t &outMaxY);
static void TryUpdateHidAxisLogicalRange(const volatile uint8_t *descriptor,
                                         uint32_t descriptorLength,
                                         const char *prefix);
static char UsbBootUsageToAscii(uint8_t usage, bool shiftActive);
static void ProcessUsbBootKeyboardReport(const char *prefix, const uint8_t *data, uint32_t length);
static void LogHidReportDecode(const char *prefix,
                               const volatile uint8_t *data,
                               uint32_t length,
                               const uint8_t *previousData,
                               bool havePreviousData,
                               bool logButtonEdges);

static void YieldLongOperationFrame() {
    if (GLongOperationYieldCallback != nullptr) {
        GLongOperationYieldCallback();
    }
}

static inline void SerialOut8(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t SerialIn8(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void IoOut16(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static void HaltCpuForever() {
    __asm__ volatile("cli");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static void RequestPlatformShutdown() {
    // QEMU/ACPI power management register.
    IoOut16(0x604u, 0x2000u);
    // Bochs/QEMU-compatible fallback poweroff port.
    IoOut16(0xB004u, 0x2000u);
}

static void InitializeSerialMirror() {
    if (GSerialMirrorFaulted) {
        return;
    }

    // COM1 @ 115200 8N1
    SerialOut8(0x3F8 + 1, 0x00);
    SerialOut8(0x3F8 + 3, 0x80);
    SerialOut8(0x3F8 + 0, 0x01);
    SerialOut8(0x3F8 + 1, 0x00);
    SerialOut8(0x3F8 + 3, 0x03);
    SerialOut8(0x3F8 + 2, 0xC7);
    SerialOut8(0x3F8 + 4, 0x0B);
    GSerialMirrorReady = true;
}

static void SerialMirrorChar(char c) {
    if (!GSerialMirrorReady) {
        return;
    }

    constexpr uint32_t SerialReadyPollLimit = 100000u;
    uint32_t polls = 0u;
    while ((SerialIn8(0x3F8 + 5) & 0x20u) == 0) {
        polls++;
        if (polls >= SerialReadyPollLimit) {
            // Fail open on real hardware UART anomalies: keep kernel alive and
            // disable mirrored serial output for the rest of the boot session.
            GSerialMirrorReady = false;
            GSerialMirrorFaulted = true;
            return;
        }
    }
    SerialOut8(0x3F8, static_cast<uint8_t>(c));
}

static void SerialMirrorLine(const char *line) {
    if (GSerialMirrorFaulted) {
        return;
    }

    if (!GSerialMirrorReady) {
        InitializeSerialMirror();
    }

    if (!GSerialMirrorReady) {
        return;
    }

    if (line == nullptr) {
        return;
    }

    for (size_t i = 0; line[i] != '\0'; i++) {
        SerialMirrorChar(line[i]);
    }
    SerialMirrorChar('\r');
    SerialMirrorChar('\n');
}

static bool StrEq(const char *a, const char *b) {
    if (a == nullptr || b == nullptr) {
        return false;
    }

    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return false;
        }
        i++;
    }
    return a[i] == b[i];
}

static bool StartsWith(const char *value, const char *prefix) {
    if (value == nullptr || prefix == nullptr) {
        return false;
    }

    size_t i = 0;
    while (prefix[i] != '\0') {
        if (value[i] != prefix[i]) {
            return false;
        }
        i++;
    }

    return true;
}

static bool MatchAnyExact(const char *value, const char *first, const char *second, const char *third) {
    return StrEq(value, first) || StrEq(value, second) || StrEq(value, third);
}

static bool MatchAnyPrefix(const char *value, const char *first, const char *second, const char *third) {
    return StartsWith(value, first) || StartsWith(value, second) || StartsWith(value, third);
}

static const char *AliasArgAfterPrefix(const char *value,
                                       const char *firstPrefix,
                                       size_t firstLength,
                                       const char *secondPrefix,
                                       size_t secondLength,
                                       const char *thirdPrefix,
                                       size_t thirdLength) {
    if (value == nullptr) {
        return nullptr;
    }

    if (StartsWith(value, firstPrefix)) {
        return value + firstLength;
    }
    if (StartsWith(value, secondPrefix)) {
        return value + secondLength;
    }
    if (StartsWith(value, thirdPrefix)) {
        return value + thirdLength;
    }

    return nullptr;
}

static bool ParseUInt(const char *value, uint64_t &out) {
    if (value == nullptr || value[0] == '\0') {
        return false;
    }

    uint64_t result = 0;
    for (size_t i = 0; value[i] != '\0'; i++) {
        const char c = value[i];
        if (c < '0' || c > '9') {
            return false;
        }
        result = result * 10 + static_cast<uint64_t>(c - '0');
    }

    out = result;
    return true;
}

static bool ParseU64Auto(const char *value, uint64_t &out) {
    if (value == nullptr || value[0] == '\0') {
        return false;
    }

    uint64_t base = 10;
    size_t index = 0;
    if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        base = 16;
        index = 2;
        if (value[index] == '\0') {
            return false;
        }
    }

    uint64_t result = 0;
    for (; value[index] != '\0'; index++) {
        const char c = value[index];
        uint64_t digit = 0;

        if (c >= '0' && c <= '9') {
            digit = static_cast<uint64_t>(c - '0');
        } else if (base == 16 && c >= 'a' && c <= 'f') {
            digit = static_cast<uint64_t>(10 + (c - 'a'));
        } else if (base == 16 && c >= 'A' && c <= 'F') {
            digit = static_cast<uint64_t>(10 + (c - 'A'));
        } else {
            return false;
        }

        if (digit >= base) {
            return false;
        }

        result = result * base + digit;
    }

    out = result;
    return true;
}

static const char *SkipSpaces(const char *value) {
    if (value == nullptr) {
        return nullptr;
    }

    while (*value == ' ') {
        value++;
    }
    return value;
}

static bool ReadToken(const char *&cursor, char *out, size_t outSize) {
    if (cursor == nullptr || out == nullptr || outSize == 0) {
        return false;
    }

    cursor = SkipSpaces(cursor);
    if (cursor == nullptr || *cursor == '\0') {
        return false;
    }

    size_t i = 0;
    while (*cursor != '\0' && *cursor != ' ') {
        if (i + 1 >= outSize) {
            return false;
        }
        out[i++] = *cursor;
        cursor++;
    }

    out[i] = '\0';
    return true;
}

static char UsbBootUsageToAscii(uint8_t usage, bool shiftActive) {
    if (usage >= 0x04u && usage <= 0x1Du) {
        const char base = static_cast<char>('a' + static_cast<char>(usage - 0x04u));
        return shiftActive ? static_cast<char>(base - ('a' - 'A')) : base;
    }

    switch (usage) {
    case 0x1Eu: return shiftActive ? '!' : '1';
    case 0x1Fu: return shiftActive ? '@' : '2';
    case 0x20u: return shiftActive ? '#' : '3';
    case 0x21u: return shiftActive ? '$' : '4';
    case 0x22u: return shiftActive ? '%' : '5';
    case 0x23u: return shiftActive ? '^' : '6';
    case 0x24u: return shiftActive ? '&' : '7';
    case 0x25u: return shiftActive ? '*' : '8';
    case 0x26u: return shiftActive ? '(' : '9';
    case 0x27u: return shiftActive ? ')' : '0';
    case 0x28u: return '\n';
    case 0x2Au: return '\b';
    case 0x2Cu: return ' ';
    case 0x2Du: return shiftActive ? '_' : '-';
    case 0x2Eu: return shiftActive ? '+' : '=';
    case 0x2Fu: return shiftActive ? '{' : '[';
    case 0x30u: return shiftActive ? '}' : ']';
    case 0x31u: return shiftActive ? '|' : '\\';
    case 0x33u: return shiftActive ? ':' : ';';
    case 0x34u: return shiftActive ? '"' : '\'';
    case 0x35u: return shiftActive ? '~' : '`';
    case 0x36u: return shiftActive ? '<' : ',';
    case 0x37u: return shiftActive ? '>' : '.';
    case 0x38u: return shiftActive ? '?' : '/';
    default:
        return 0;
    }
}

static void ProcessUsbBootKeyboardReport(const char *prefix, const uint8_t *data, uint32_t length) {
    if (prefix == nullptr || data == nullptr || length < 8u) {
        return;
    }

    const uint8_t modifiers = data[0];
    const bool shiftActive = (modifiers & (0x02u | 0x20u)) != 0;

    for (uint32_t i = 2u; i < 8u; i++) {
        const uint8_t usage = data[i];
        if (usage == 0u) {
            continue;
        }

        bool alreadyPressed = false;
        if (GHaveUsbBootKeyboardLastReport) {
            for (uint32_t j = 0u; j < 6u; j++) {
                if (GUsbBootKeyboardLastKeys[j] == usage) {
                    alreadyPressed = true;
                    break;
                }
            }
        }

        if (alreadyPressed) {
            continue;
        }

        const char ascii = UsbBootUsageToAscii(usage, shiftActive);
        if (ascii == 0) {
            continue;
        }

        FKeyboardKeyEvent event{};
        event.Pressed = true;
        event.Ascii = ascii;
        event.ScanCode = usage;
        event.Modifiers = shiftActive ? KeyboardModifierShift : KeyboardModifierNone;
        if (ascii == '\n') {
            event.Key = EKeyboardKey::Enter;
        } else if (ascii == '\b') {
            event.Key = EKeyboardKey::Backspace;
        } else if (ascii == ' ') {
            event.Key = EKeyboardKey::Space;
        } else if (ascii >= '0' && ascii <= '9') {
            event.Key = EKeyboardKey::Digit;
        } else if ((ascii >= 'a' && ascii <= 'z') || (ascii >= 'A' && ascii <= 'Z')) {
            event.Key = EKeyboardKey::Letter;
        } else {
            event.Key = EKeyboardKey::Symbol;
        }
        (void)FKeyboardManager::EnqueueSyntheticKeyEvent(event);

        if (IsHidVerboseMode()) {
            char line[96] = {};
            size_t pos = 0;
            AppendString(line, sizeof(line), pos, prefix);
            AppendString(line, sizeof(line), pos, " KEY ");
            AppendHex(line, sizeof(line), pos, usage);
            AppendString(line, sizeof(line), pos, " ASCII ");
            AppendChar(line, sizeof(line), pos, ascii);
            PushLog(line);
        }
    }

    GUsbBootKeyboardLastModifiers = modifiers;
    for (uint32_t i = 0u; i < 6u; i++) {
        GUsbBootKeyboardLastKeys[i] = data[2u + i];
    }
    GHaveUsbBootKeyboardLastReport = true;
}

static void LogHidReportDecode(const char *prefix,
                               const volatile uint8_t *data,
                               uint32_t length,
                               const uint8_t *previousData,
                               bool havePreviousData,
                               bool logButtonEdges) {
    if (prefix == nullptr || data == nullptr || length == 0) {
        return;
    }

    const uint8_t buttons = data[0];
    const uint8_t currButtons = buttons & 0x7u;
    const uint8_t prevButtons =
        (havePreviousData && previousData != nullptr) ? static_cast<uint8_t>(previousData[0] & 0x7u) : currButtons;
    const uint8_t changedButtons = prevButtons ^ currButtons;
    GHidButtonsDownMask = currButtons;
    GHidButtonPressEdgesMask |= static_cast<uint8_t>(changedButtons & currButtons);

    const bool left = (buttons & 0x1u) != 0;
    const bool right = (buttons & 0x2u) != 0;
    const bool middle = (buttons & 0x4u) != 0;

    char line[256] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, prefix);
    AppendString(line, sizeof(line), pos, " BTN L");
    AppendUInt(line, sizeof(line), pos, left ? 1u : 0u);
    AppendString(line, sizeof(line), pos, " R");
    AppendUInt(line, sizeof(line), pos, right ? 1u : 0u);
    AppendString(line, sizeof(line), pos, " M");
    AppendUInt(line, sizeof(line), pos, middle ? 1u : 0u);

    if (length >= 5u) {
        const uint16_t absX = static_cast<uint16_t>(data[1] | (static_cast<uint16_t>(data[2]) << 8u));
        const uint16_t absY = static_cast<uint16_t>(data[3] | (static_cast<uint16_t>(data[4]) << 8u));
        AppendString(line, sizeof(line), pos, " AX ");
        AppendUInt(line, sizeof(line), pos, absX);
        AppendString(line, sizeof(line), pos, " AY ");
        AppendUInt(line, sizeof(line), pos, absY);

        if (IsHidVerboseMode() && havePreviousData && previousData != nullptr) {
            const uint16_t prevAbsX = static_cast<uint16_t>(previousData[1] | (static_cast<uint16_t>(previousData[2]) << 8u));
            const uint16_t prevAbsY = static_cast<uint16_t>(previousData[3] | (static_cast<uint16_t>(previousData[4]) << 8u));
            AppendString(line, sizeof(line), pos, " dAX ");
            AppendSInt(line, sizeof(line), pos, static_cast<int64_t>(static_cast<int32_t>(absX) - static_cast<int32_t>(prevAbsX)));
            AppendString(line, sizeof(line), pos, " dAY ");
            AppendSInt(line, sizeof(line), pos, static_cast<int64_t>(static_cast<int32_t>(absY) - static_cast<int32_t>(prevAbsY)));
        }

        TryUpdateAutoHidAxisLogicalRange(absX, absY);

        // Prefer descriptor logical range. If unavailable, keep a stable full-device fallback
        // inferred from observed maxima instead of activating narrow sampled auto-ranges.
        int32_t logicalMinX = 0;
        int32_t logicalMaxX = 65535;
        int32_t logicalMinY = 0;
        int32_t logicalMaxY = 65535;

        if (GHaveHidLogicalAxisRange) {
            logicalMinX = GHidLogicalMinX;
            logicalMaxX = GHidLogicalMaxX;
            logicalMinY = GHidLogicalMinY;
            logicalMaxY = GHidLogicalMaxY;
        } else if (GHidAutoLogicalRangeInitialized) {
            logicalMaxX = InferFallbackLogicalMaxFromObserved(GHidAutoLogicalMaxX);
            logicalMaxY = InferFallbackLogicalMaxFromObserved(GHidAutoLogicalMaxY);
        }
        int32_t normX = NormalizeHidAbsoluteAxis(absX, logicalMinX, logicalMaxX);
        int32_t normY = NormalizeHidAbsoluteAxis(absY, logicalMinY, logicalMaxY);
        bool usedAutoExpandBlend = false;
        uint32_t autoExpandBlendStep = 0u;
        if (!GHaveHidLogicalAxisRange && GHidAutoExpandBlendSamplesRemaining > 0u) {
            const int32_t step = static_cast<int32_t>(GHidAutoExpandBlendSampleCount - GHidAutoExpandBlendSamplesRemaining + 1u);
            usedAutoExpandBlend = true;
            autoExpandBlendStep = static_cast<uint32_t>(step);
            const int32_t oldWeight = static_cast<int32_t>(GHidAutoExpandBlendSampleCount) - step;
            normX = (GHidAutoExpandBlendStartNormX * oldWeight + normX * step) /
                static_cast<int32_t>(GHidAutoExpandBlendSampleCount);
            normY = (GHidAutoExpandBlendStartNormY * oldWeight + normY * step) /
                static_cast<int32_t>(GHidAutoExpandBlendSampleCount);
            GHidAutoExpandBlendSamplesRemaining--;
        }
        AppendString(line, sizeof(line), pos, " NX ");
        AppendUInt(line, sizeof(line), pos, static_cast<uint64_t>(normX));
        AppendString(line, sizeof(line), pos, " NY ");
        AppendUInt(line, sizeof(line), pos, static_cast<uint64_t>(normY));

        int32_t filteredNormX = normX;
        int32_t filteredNormY = normY;
        if (GHidSmoothingInitialized) {
            const int32_t smoothedNormX = FixedPointToIntRounded(GHidSmoothedNormXFp, GHidSmoothScale);
            const int32_t smoothedNormY = FixedPointToIntRounded(GHidSmoothedNormYFp, GHidSmoothScale);
            const int32_t dx = filteredNormX - smoothedNormX;
            const int32_t dy = filteredNormY - smoothedNormY;
            constexpr int32_t PositionDeadband = 2;
            if (dx >= -PositionDeadband && dx <= PositionDeadband) {
                filteredNormX = smoothedNormX;
            }
            if (dy >= -PositionDeadband && dy <= PositionDeadband) {
                filteredNormY = smoothedNormY;
            }

            const int32_t filteredDx = filteredNormX - smoothedNormX;
            const int32_t filteredDy = filteredNormY - smoothedNormY;
            const int32_t absFilteredDx = filteredDx < 0 ? -filteredDx : filteredDx;
            const int32_t absFilteredDy = filteredDy < 0 ? -filteredDy : filteredDy;
            const int32_t motionMagnitude = absFilteredDx > absFilteredDy ? absFilteredDx : absFilteredDy;

            int32_t sampleWeightNumerator = 3;
            int32_t sampleWeightDenominator = 4;
            if (GCursorLatencyMode == ECursorLatencyMode::Responsive) {
                constexpr int32_t MotionThreshold = 4;
                if (motionMagnitude >= MotionThreshold) {
                    sampleWeightNumerator = 7;
                    sampleWeightDenominator = 8;
                } else {
                    sampleWeightNumerator = 2;
                    sampleWeightDenominator = 8;
                }
            }

            const int32_t previousWeight = sampleWeightDenominator - sampleWeightNumerator;
            GHidSmoothedNormXFp = ((GHidSmoothedNormXFp * previousWeight) +
                                   (filteredNormX * GHidSmoothScale * sampleWeightNumerator)) /
                                  sampleWeightDenominator;
            GHidSmoothedNormYFp = ((GHidSmoothedNormYFp * previousWeight) +
                                   (filteredNormY * GHidSmoothScale * sampleWeightNumerator)) /
                                  sampleWeightDenominator;
        } else {
            GHidSmoothedNormXFp = filteredNormX * GHidSmoothScale;
            GHidSmoothedNormYFp = filteredNormY * GHidSmoothScale;
            GHidSmoothedMotionXFp = 0;
            GHidSmoothedMotionYFp = 0;
            GHidSmoothingInitialized = true;
        }

        if (IsHidVerboseMode()) {
            AppendString(line, sizeof(line), pos, " SNX ");
            AppendUInt(line, sizeof(line), pos, static_cast<uint64_t>(FixedPointToIntRounded(GHidSmoothedNormXFp, GHidSmoothScale)));
            AppendString(line, sizeof(line), pos, " SNY ");
            AppendUInt(line, sizeof(line), pos, static_cast<uint64_t>(FixedPointToIntRounded(GHidSmoothedNormYFp, GHidSmoothScale)));
        }

        if (IsHidVerboseMode() && havePreviousData && previousData != nullptr) {
            constexpr int32_t NormDeadzone = 0;
            constexpr int32_t MaxSmoothedNormStep = 8;
            const uint16_t prevAbsX = static_cast<uint16_t>(previousData[1] | (static_cast<uint16_t>(previousData[2]) << 8u));
            const uint16_t prevAbsY = static_cast<uint16_t>(previousData[3] | (static_cast<uint16_t>(previousData[4]) << 8u));
            int32_t prevNormX = NormalizeHidAbsoluteAxis(prevAbsX, logicalMinX, logicalMaxX);
            int32_t prevNormY = NormalizeHidAbsoluteAxis(prevAbsY, logicalMinY, logicalMaxY);
            if (!GHaveHidLogicalAxisRange && usedAutoExpandBlend) {
                const int32_t prevStep = static_cast<int32_t>(autoExpandBlendStep) - 1;
                if (prevStep <= 0) {
                    prevNormX = GHidAutoExpandBlendStartNormX;
                    prevNormY = GHidAutoExpandBlendStartNormY;
                } else {
                    const int32_t oldWeightPrev = static_cast<int32_t>(GHidAutoExpandBlendSampleCount) - prevStep;
                    prevNormX = (GHidAutoExpandBlendStartNormX * oldWeightPrev + prevNormX * prevStep) /
                        static_cast<int32_t>(GHidAutoExpandBlendSampleCount);
                    prevNormY = (GHidAutoExpandBlendStartNormY * oldWeightPrev + prevNormY * prevStep) /
                        static_cast<int32_t>(GHidAutoExpandBlendSampleCount);
                }
            }
            const int32_t dNormX = normX - prevNormX;
            const int32_t dNormY = normY - prevNormY;

            const int32_t absNormDx = dNormX < 0 ? -dNormX : dNormX;
            const int32_t absNormDy = dNormY < 0 ? -dNormY : dNormY;
            const int32_t motionDx = absNormDx <= NormDeadzone ? 0 : dNormX;
            const int32_t motionDy = absNormDy <= NormDeadzone ? 0 : dNormY;
            const int32_t smoothedInputDx = ClampSInt32(motionDx, -MaxSmoothedNormStep, MaxSmoothedNormStep);
            const int32_t smoothedInputDy = ClampSInt32(motionDy, -MaxSmoothedNormStep, MaxSmoothedNormStep);

            GHidSmoothedMotionXFp = (GHidSmoothedMotionXFp + smoothedInputDx * GHidSmoothScale) / 2;
            GHidSmoothedMotionYFp = (GHidSmoothedMotionYFp + smoothedInputDy * GHidSmoothScale) / 2;

            AppendString(line, sizeof(line), pos, " dNX ");
            AppendSInt(line, sizeof(line), pos, dNormX);
            AppendString(line, sizeof(line), pos, " dNY ");
            AppendSInt(line, sizeof(line), pos, dNormY);
            AppendString(line, sizeof(line), pos, " MDX ");
            AppendSInt(line, sizeof(line), pos, motionDx);
            AppendString(line, sizeof(line), pos, " MDY ");
            AppendSInt(line, sizeof(line), pos, motionDy);
            AppendString(line, sizeof(line), pos, " SDX ");
            AppendSInt(line, sizeof(line), pos, FixedPointToIntRounded(GHidSmoothedMotionXFp, GHidSmoothScale));
            AppendString(line, sizeof(line), pos, " SDY ");
            AppendSInt(line, sizeof(line), pos, FixedPointToIntRounded(GHidSmoothedMotionYFp, GHidSmoothScale));
            AppendString(line, sizeof(line), pos, " SDXf ");
            AppendFixedPoint1Dec(line, sizeof(line), pos, GHidSmoothedMotionXFp, GHidSmoothScale);
            AppendString(line, sizeof(line), pos, " SDYf ");
            AppendFixedPoint1Dec(line, sizeof(line), pos, GHidSmoothedMotionYFp, GHidSmoothScale);
        }

        GHidCursorNormX = FixedPointToIntRounded(GHidSmoothedNormXFp, GHidSmoothScale);
        GHidCursorNormY = FixedPointToIntRounded(GHidSmoothedNormYFp, GHidSmoothScale);
        GHaveHidAbsoluteCursor = true;

        if (length >= 6u) {
            const int8_t wheel = static_cast<int8_t>(data[5]);
            AppendString(line, sizeof(line), pos, " W ");
            AppendSInt(line, sizeof(line), pos, wheel);
        }
    } else if (length >= 3u) {
        const int8_t relX = static_cast<int8_t>(data[1]);
        const int8_t relY = static_cast<int8_t>(data[2]);
        AppendString(line, sizeof(line), pos, " DX ");
        AppendSInt(line, sizeof(line), pos, relX);
        AppendString(line, sizeof(line), pos, " DY ");
        AppendSInt(line, sizeof(line), pos, relY);
        if (length >= 4u) {
            const int8_t wheel = static_cast<int8_t>(data[3]);
            AppendString(line, sizeof(line), pos, " W ");
            AppendSInt(line, sizeof(line), pos, wheel);
        }
    }

    PushLog(line);

    if (logButtonEdges && havePreviousData && previousData != nullptr) {
        if (changedButtons != 0u) {
            char edgeLine[160] = {};
            size_t edgePos = 0;
            AppendString(edgeLine, sizeof(edgeLine), edgePos, prefix);
            AppendString(edgeLine, sizeof(edgeLine), edgePos, " EVT");

            if ((changedButtons & 0x1u) != 0) {
                AppendString(edgeLine, sizeof(edgeLine), edgePos, (currButtons & 0x1u) != 0 ? " LDOWN" : " LUP");
            }
            if ((changedButtons & 0x2u) != 0) {
                AppendString(edgeLine, sizeof(edgeLine), edgePos, (currButtons & 0x2u) != 0 ? " RDOWN" : " RUP");
            }
            if ((changedButtons & 0x4u) != 0) {
                AppendString(edgeLine, sizeof(edgeLine), edgePos, (currButtons & 0x4u) != 0 ? " MDOWN" : " MUP");
            }

            PushLog(edgeLine);
        }
    }
}

static int32_t FixedPointToIntRounded(int32_t value, int32_t scale) {
    if (scale <= 0) {
        return 0;
    }

    if (value >= 0) {
        return (value + (scale / 2)) / scale;
    }
    return (value - (scale / 2)) / scale;
}

static void AppendFixedPoint1Dec(char *dst, size_t dstSize, size_t &offset, int32_t value, int32_t scale) {
    if (scale <= 0) {
        AppendString(dst, dstSize, offset, "0.0");
        return;
    }

    int32_t absValue = value;
    if (absValue < 0) {
        AppendChar(dst, dstSize, offset, '-');
        absValue = -absValue;
    }

    const int32_t whole = absValue / scale;
    const int32_t fraction = ((absValue % scale) * 10 + (scale / 2)) / scale;

    if (fraction >= 10) {
        AppendUInt(dst, dstSize, offset, static_cast<uint64_t>(whole + 1));
        AppendString(dst, dstSize, offset, ".0");
        return;
    }

    AppendUInt(dst, dstSize, offset, static_cast<uint64_t>(whole));
    AppendChar(dst, dstSize, offset, '.');
    AppendUInt(dst, dstSize, offset, static_cast<uint64_t>(fraction));
}

static int32_t ClampSInt32(int32_t value, int32_t minValue, int32_t maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

static bool IsHidVerboseMode() {
    return GHidLogMode == EHidLogMode::Verbose;
}

static bool IsHidEdgeMode() {
    return GHidLogMode == EHidLogMode::Core || GHidLogMode == EHidLogMode::Verbose;
}

static const char *GetCursorLatencyModeName() {
    return GCursorLatencyMode == ECursorLatencyMode::Responsive ? "RESP" : "SMOOTH";
}

static int32_t ReadSignedLE(const volatile uint8_t *bytes, uint32_t length) {
    if (bytes == nullptr || length == 0u || length > 4u) {
        return 0;
    }

    uint32_t value = 0;
    for (uint32_t i = 0; i < length; i++) {
        value |= static_cast<uint32_t>(bytes[i]) << (i * 8u);
    }

    if (length < 4u) {
        const uint32_t signBit = 1u << ((length * 8u) - 1u);
        if ((value & signBit) != 0u) {
            const uint32_t extendMask = ~((1u << (length * 8u)) - 1u);
            value |= extendMask;
        }
    }

    return static_cast<int32_t>(value);
}

static uint32_t ReadUnsignedLE(const volatile uint8_t *bytes, uint32_t length) {
    if (bytes == nullptr || length == 0u || length > 4u) {
        return 0;
    }

    uint32_t value = 0;
    for (uint32_t i = 0; i < length; i++) {
        value |= static_cast<uint32_t>(bytes[i]) << (i * 8u);
    }
    return value;
}

static int32_t NormalizeHidAbsoluteAxis(uint16_t rawValue, int32_t logicalMin, int32_t logicalMax) {
    if (logicalMax <= logicalMin) {
        return static_cast<int32_t>((static_cast<uint32_t>(rawValue) * 1000u) / 65535u);
    }

    int32_t value = static_cast<int32_t>(rawValue);
    if (logicalMin < 0) {
        value = static_cast<int32_t>(static_cast<int16_t>(rawValue));
    }

    value = ClampSInt32(value, logicalMin, logicalMax);
    const int64_t numerator = static_cast<int64_t>(value - logicalMin) * 1000ll;
    const int64_t denominator = static_cast<int64_t>(logicalMax - logicalMin);
    return static_cast<int32_t>(numerator / denominator);
}

static int32_t InferFallbackLogicalMaxFromObserved(int32_t observedMax) {
    if (observedMax <= 0) {
        return 65535;
    }

    // Common absolute tablets report 15-bit coordinates (0..32767).
    // If observed maxima stay in that envelope, map against 32767.
    if (observedMax <= 40960) {
        return 32767;
    }

    return 65535;
}

static void TryUpdateAutoHidAxisLogicalRange(uint16_t absX, uint16_t absY) {
    if (GHaveHidLogicalAxisRange) {
        return;
    }

    // When auto-range is active, expand bounds if new samples fall outside current limits.
    // This avoids hard clamping to 0/1000 when a later run starts in a different region.
    if (GHidAutoLogicalRangeReady && GHidAutoLogicalRangeActive) {
        const int32_t x = static_cast<int32_t>(absX);
        const int32_t y = static_cast<int32_t>(absY);

        constexpr int32_t ExpandGuard = 96;
        constexpr int32_t EmergencyExpandOvershoot = 2048;
        constexpr uint8_t MinOutlierStreak = 2u;
        constexpr uint32_t ExpandCooldownSamples = 10u;

        const bool belowMinX = x < (GHidAutoLogicalMinX - ExpandGuard);
        const bool aboveMaxX = x > (GHidAutoLogicalMaxX + ExpandGuard);
        const bool belowMinY = y < (GHidAutoLogicalMinY - ExpandGuard);
        const bool aboveMaxY = y > (GHidAutoLogicalMaxY + ExpandGuard);

        GHidAutoExpandBelowMinXStreak = belowMinX
            ? static_cast<uint8_t>(GHidAutoExpandBelowMinXStreak < 255u ? (GHidAutoExpandBelowMinXStreak + 1u) : 255u)
            : 0u;
        GHidAutoExpandAboveMaxXStreak = aboveMaxX
            ? static_cast<uint8_t>(GHidAutoExpandAboveMaxXStreak < 255u ? (GHidAutoExpandAboveMaxXStreak + 1u) : 255u)
            : 0u;
        GHidAutoExpandBelowMinYStreak = belowMinY
            ? static_cast<uint8_t>(GHidAutoExpandBelowMinYStreak < 255u ? (GHidAutoExpandBelowMinYStreak + 1u) : 255u)
            : 0u;
        GHidAutoExpandAboveMaxYStreak = aboveMaxY
            ? static_cast<uint8_t>(GHidAutoExpandAboveMaxYStreak < 255u ? (GHidAutoExpandAboveMaxYStreak + 1u) : 255u)
            : 0u;

        if (GHidAutoExpandCooldownSamplesRemaining > 0u) {
            GHidAutoExpandCooldownSamplesRemaining--;
        }

        const bool emergencyExpand =
            (x < (GHidAutoLogicalMinX - EmergencyExpandOvershoot)) ||
            (x > (GHidAutoLogicalMaxX + EmergencyExpandOvershoot)) ||
            (y < (GHidAutoLogicalMinY - EmergencyExpandOvershoot)) ||
            (y > (GHidAutoLogicalMaxY + EmergencyExpandOvershoot));

        const bool hysteresisReady =
            (GHidAutoExpandBelowMinXStreak >= MinOutlierStreak) ||
            (GHidAutoExpandAboveMaxXStreak >= MinOutlierStreak) ||
            (GHidAutoExpandBelowMinYStreak >= MinOutlierStreak) ||
            (GHidAutoExpandAboveMaxYStreak >= MinOutlierStreak);

        if (!emergencyExpand && (GHidAutoExpandCooldownSamplesRemaining > 0u || !hysteresisReady)) {
            return;
        }

        bool expanded = false;
        const int32_t spanX = GHidAutoLogicalMaxX - GHidAutoLogicalMinX;
        const int32_t spanY = GHidAutoLogicalMaxY - GHidAutoLogicalMinY;
        const int32_t basePadX = (spanX / 6) > 768 ? (spanX / 6) : 768;
        const int32_t basePadY = (spanY / 6) > 768 ? (spanY / 6) : 768;

        if (x < GHidAutoLogicalMinX) {
            const int32_t overshoot = GHidAutoLogicalMinX - x;
            const int32_t grow = (overshoot + (basePadX / 2)) > basePadX ? (overshoot + (basePadX / 2)) : basePadX;
            GHidAutoLogicalMinX = ClampSInt32(x - grow, 0, 65535);
            expanded = true;
        }
        if (x > GHidAutoLogicalMaxX) {
            const int32_t overshoot = x - GHidAutoLogicalMaxX;
            const int32_t grow = (overshoot + (basePadX / 2)) > basePadX ? (overshoot + (basePadX / 2)) : basePadX;
            GHidAutoLogicalMaxX = ClampSInt32(x + grow, 0, 65535);
            expanded = true;
        }
        if (y < GHidAutoLogicalMinY) {
            const int32_t overshoot = GHidAutoLogicalMinY - y;
            const int32_t grow = (overshoot + (basePadY / 2)) > basePadY ? (overshoot + (basePadY / 2)) : basePadY;
            GHidAutoLogicalMinY = ClampSInt32(y - grow, 0, 65535);
            expanded = true;
        }
        if (y > GHidAutoLogicalMaxY) {
            const int32_t overshoot = y - GHidAutoLogicalMaxY;
            const int32_t grow = (overshoot + (basePadY / 2)) > basePadY ? (overshoot + (basePadY / 2)) : basePadY;
            GHidAutoLogicalMaxY = ClampSInt32(y + grow, 0, 65535);
            expanded = true;
        }

        if (expanded) {
            GHidAutoExpandBlendStartNormX = GHidCursorNormX;
            GHidAutoExpandBlendStartNormY = GHidCursorNormY;
            GHidAutoExpandBlendSamplesRemaining = GHidAutoExpandBlendSampleCount;
            GHidAutoExpandCooldownSamplesRemaining = ExpandCooldownSamples;
            GHidAutoExpandBelowMinXStreak = 0u;
            GHidAutoExpandAboveMaxXStreak = 0u;
            GHidAutoExpandBelowMinYStreak = 0u;
            GHidAutoExpandAboveMaxYStreak = 0u;
            PushLog("HIDRANGE AUTO EXPAND");
        }
        return;
    }

    // Once auto-range is ready but not active yet, keep bounds fixed until next run.
    if (GHidAutoLogicalRangeReady) {
        return;
    }

    const int32_t x = static_cast<int32_t>(absX);
    const int32_t y = static_cast<int32_t>(absY);

    if (!GHidAutoLogicalRangeInitialized) {
        GHidAutoLogicalRangeInitialized = true;
        GHidAutoLogicalRangeSamples = 1u;
        GHidAutoLogicalMinX = x;
        GHidAutoLogicalMaxX = x;
        GHidAutoLogicalMinY = y;
        GHidAutoLogicalMaxY = y;
        return;
    }

    GHidAutoLogicalRangeSamples++;
    if (x < GHidAutoLogicalMinX) {
        GHidAutoLogicalMinX = x;
    }
    if (x > GHidAutoLogicalMaxX) {
        GHidAutoLogicalMaxX = x;
    }
    if (y < GHidAutoLogicalMinY) {
        GHidAutoLogicalMinY = y;
    }
    if (y > GHidAutoLogicalMaxY) {
        GHidAutoLogicalMaxY = y;
    }

    const int32_t spanX = GHidAutoLogicalMaxX - GHidAutoLogicalMinX;
    const int32_t spanY = GHidAutoLogicalMaxY - GHidAutoLogicalMinY;
    constexpr uint32_t MinAutoRangeSamples = 48u;
    constexpr int32_t MinAutoRangeSpanX = 2048;
    constexpr int32_t MinAutoRangeSpanY = 2048;
    if (!GHidAutoLogicalRangeReady && GHidAutoLogicalRangeSamples >= MinAutoRangeSamples &&
        spanX >= MinAutoRangeSpanX && spanY >= MinAutoRangeSpanY) {
        // Add headroom so edge samples do not immediately clamp to 0/1000.
        constexpr int32_t MinPad = 1024;
        constexpr int32_t EdgeGuard = 256;
        const int32_t padXDesired = (spanX / 8) > MinPad ? (spanX / 8) : MinPad;
        const int32_t padYDesired = (spanY / 8) > MinPad ? (spanY / 8) : MinPad;

        const int32_t maxPadXLow = GHidAutoLogicalMinX > EdgeGuard ? (GHidAutoLogicalMinX - EdgeGuard) : 0;
        const int32_t maxPadXHigh = GHidAutoLogicalMaxX < (65535 - EdgeGuard) ? ((65535 - EdgeGuard) - GHidAutoLogicalMaxX) : 0;
        const int32_t maxPadYLow = GHidAutoLogicalMinY > EdgeGuard ? (GHidAutoLogicalMinY - EdgeGuard) : 0;
        const int32_t maxPadYHigh = GHidAutoLogicalMaxY < (65535 - EdgeGuard) ? ((65535 - EdgeGuard) - GHidAutoLogicalMaxY) : 0;

        const int32_t padXLow = padXDesired < maxPadXLow ? padXDesired : maxPadXLow;
        const int32_t padXHigh = padXDesired < maxPadXHigh ? padXDesired : maxPadXHigh;
        const int32_t padYLow = padYDesired < maxPadYLow ? padYDesired : maxPadYLow;
        const int32_t padYHigh = padYDesired < maxPadYHigh ? padYDesired : maxPadYHigh;

        GHidAutoLogicalMinX = ClampSInt32(GHidAutoLogicalMinX - padXLow, 0, 65535);
        GHidAutoLogicalMaxX = ClampSInt32(GHidAutoLogicalMaxX + padXHigh, 0, 65535);
        GHidAutoLogicalMinY = ClampSInt32(GHidAutoLogicalMinY - padYLow, 0, 65535);
        GHidAutoLogicalMaxY = ClampSInt32(GHidAutoLogicalMaxY + padYHigh, 0, 65535);

        GHidAutoLogicalRangeReady = true;

        char line[160] = {};
        size_t pos = 0;
        AppendString(line, sizeof(line), pos, "HIDRANGE AUTO READY X ");
        AppendSInt(line, sizeof(line), pos, GHidAutoLogicalMinX);
        AppendString(line, sizeof(line), pos, "..");
        AppendSInt(line, sizeof(line), pos, GHidAutoLogicalMaxX);
        AppendString(line, sizeof(line), pos, " Y ");
        AppendSInt(line, sizeof(line), pos, GHidAutoLogicalMinY);
        AppendString(line, sizeof(line), pos, "..");
        AppendSInt(line, sizeof(line), pos, GHidAutoLogicalMaxY);
        AppendString(line, sizeof(line), pos, " PX ");
        AppendSInt(line, sizeof(line), pos, padXLow);
        AppendChar(line, sizeof(line), pos, '/');
        AppendSInt(line, sizeof(line), pos, padXHigh);
        AppendString(line, sizeof(line), pos, " PY ");
        AppendSInt(line, sizeof(line), pos, padYLow);
        AppendChar(line, sizeof(line), pos, '/');
        AppendSInt(line, sizeof(line), pos, padYHigh);
        PushLog(line);
    }
}

static bool ParseHidAxisLogicalRange(const volatile uint8_t *descriptor,
                                     uint32_t descriptorLength,
                                     int32_t &outMinX,
                                     int32_t &outMaxX,
                                     int32_t &outMinY,
                                     int32_t &outMaxY) {
    if (descriptor == nullptr || descriptorLength == 0u) {
        return false;
    }

    uint32_t usagePage = 0;
    int32_t logicalMin = 0;
    int32_t logicalMax = 0;
    bool haveLogicalMin = false;
    bool haveLogicalMax = false;

    uint32_t usages[16] = {};
    uint32_t usageCount = 0;
    uint32_t usageMin = 0;
    uint32_t usageMax = 0;
    bool haveUsageRange = false;

    bool foundX = false;
    bool foundY = false;

    uint32_t offset = 0;
    while (offset < descriptorLength) {
        const uint8_t itemPrefix = descriptor[offset++];

        if (itemPrefix == 0xFEu) {
            if (offset + 2u > descriptorLength) {
                break;
            }
            const uint8_t longDataSize = descriptor[offset];
            offset += 2u;
            if (offset + static_cast<uint32_t>(longDataSize) > descriptorLength) {
                break;
            }
            offset += static_cast<uint32_t>(longDataSize);
            continue;
        }

        uint32_t dataSize = itemPrefix & 0x3u;
        if (dataSize == 3u) {
            dataSize = 4u;
        }

        const uint32_t itemType = (itemPrefix >> 2u) & 0x3u;
        const uint32_t itemTag = (itemPrefix >> 4u) & 0xFu;

        if (offset + dataSize > descriptorLength) {
            break;
        }

        const volatile uint8_t *itemData = descriptor + offset;
        const uint32_t unsignedValue = ReadUnsignedLE(itemData, dataSize);
        const int32_t signedValue = ReadSignedLE(itemData, dataSize);
        offset += dataSize;

        if (itemType == 1u) {
            if (itemTag == 0u) {
                usagePage = unsignedValue;
            } else if (itemTag == 1u) {
                logicalMin = signedValue;
                haveLogicalMin = true;
            } else if (itemTag == 2u) {
                logicalMax = (haveLogicalMin && logicalMin < 0) ? signedValue : static_cast<int32_t>(unsignedValue);
                haveLogicalMax = true;
            }
            continue;
        }

        if (itemType == 2u) {
            if (itemTag == 0u) {
                if (usageCount < (sizeof(usages) / sizeof(usages[0]))) {
                    usages[usageCount++] = unsignedValue;
                }
            } else if (itemTag == 1u) {
                usageMin = unsignedValue;
                haveUsageRange = true;
            } else if (itemTag == 2u) {
                usageMax = unsignedValue;
                haveUsageRange = true;
            }
            continue;
        }

        if (itemType == 0u) {
            const bool isInputItem = itemTag == 8u;
            if (isInputItem && usagePage == 0x01u && haveLogicalMin && haveLogicalMax) {
                for (uint32_t i = 0; i < usageCount; i++) {
                    if (!foundX && usages[i] == 0x30u) {
                        outMinX = logicalMin;
                        outMaxX = logicalMax;
                        foundX = true;
                    } else if (!foundY && usages[i] == 0x31u) {
                        outMinY = logicalMin;
                        outMaxY = logicalMax;
                        foundY = true;
                    }
                }

                if (haveUsageRange) {
                    if (!foundX && usageMin <= 0x30u && usageMax >= 0x30u) {
                        outMinX = logicalMin;
                        outMaxX = logicalMax;
                        foundX = true;
                    }
                    if (!foundY && usageMin <= 0x31u && usageMax >= 0x31u) {
                        outMinY = logicalMin;
                        outMaxY = logicalMax;
                        foundY = true;
                    }
                }
            }

            usageCount = 0;
            haveUsageRange = false;

            if (foundX && foundY) {
                return true;
            }
        }
    }

    return foundX && foundY;
}

static void TryUpdateHidAxisLogicalRange(const volatile uint8_t *descriptor,
                                         uint32_t descriptorLength,
                                         const char *prefix) {
    if (descriptor == nullptr || descriptorLength == 0u) {
        return;
    }

    int32_t minX = 0;
    int32_t maxX = 0;
    int32_t minY = 0;
    int32_t maxY = 0;
    if (!ParseHidAxisLogicalRange(descriptor, descriptorLength, minX, maxX, minY, maxY)) {
        return;
    }

    if (maxX <= minX || maxY <= minY) {
        return;
    }

    GHaveHidLogicalAxisRange = true;
    GHidLogicalMinX = minX;
    GHidLogicalMaxX = maxX;
    GHidLogicalMinY = minY;
    GHidLogicalMaxY = maxY;

    char line[160] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, prefix != nullptr ? prefix : "HID RANGE");
    AppendString(line, sizeof(line), pos, " X ");
    AppendSInt(line, sizeof(line), pos, minX);
    AppendString(line, sizeof(line), pos, "..");
    AppendSInt(line, sizeof(line), pos, maxX);
    AppendString(line, sizeof(line), pos, " Y ");
    AppendSInt(line, sizeof(line), pos, minY);
    AppendString(line, sizeof(line), pos, "..");
    AppendSInt(line, sizeof(line), pos, maxY);
    PushLog(line);
}

static bool ReserveVirtualRange(uint64_t pageCount, uint64_t &outVirtualAddress) {
    if (pageCount == 0) {
        return false;
    }

    for (size_t i = 0; i < GVirtualFreeRangeCount; i++) {
        FVirtualFreeRange &range = GVirtualFreeRanges[i];
        if (range.PageCount < pageCount) {
            continue;
        }

        outVirtualAddress = range.VirtualAddress;
        range.VirtualAddress += pageCount * FVirtualMemoryManager::PageSize;
        range.PageCount -= pageCount;

        if (range.PageCount == 0) {
            for (size_t j = i; j + 1 < GVirtualFreeRangeCount; j++) {
                GVirtualFreeRanges[j] = GVirtualFreeRanges[j + 1];
            }
            GVirtualFreeRangeCount--;
        }
        return true;
    }

    outVirtualAddress = GNextVmallocVirtual;
    GNextVmallocVirtual += pageCount * FVirtualMemoryManager::PageSize;
    return true;
}

static bool ReleaseVirtualRange(uint64_t virtualAddress, uint64_t pageCount) {
    if (pageCount == 0 || GVirtualFreeRangeCount >= (sizeof(GVirtualFreeRanges) / sizeof(GVirtualFreeRanges[0]))) {
        return false;
    }

    size_t insertAt = 0;
    while (insertAt < GVirtualFreeRangeCount && GVirtualFreeRanges[insertAt].VirtualAddress < virtualAddress) {
        insertAt++;
    }

    for (size_t i = GVirtualFreeRangeCount; i > insertAt; i--) {
        GVirtualFreeRanges[i] = GVirtualFreeRanges[i - 1];
    }

    GVirtualFreeRanges[insertAt] = FVirtualFreeRange{.VirtualAddress = virtualAddress, .PageCount = pageCount};
    GVirtualFreeRangeCount++;

    if (insertAt > 0) {
        FVirtualFreeRange &prev = GVirtualFreeRanges[insertAt - 1];
        FVirtualFreeRange &curr = GVirtualFreeRanges[insertAt];
        const uint64_t prevEnd = prev.VirtualAddress + prev.PageCount * FVirtualMemoryManager::PageSize;
        if (prevEnd == curr.VirtualAddress) {
            prev.PageCount += curr.PageCount;
            for (size_t i = insertAt; i + 1 < GVirtualFreeRangeCount; i++) {
                GVirtualFreeRanges[i] = GVirtualFreeRanges[i + 1];
            }
            GVirtualFreeRangeCount--;
            insertAt--;
        }
    }

    if (insertAt + 1 < GVirtualFreeRangeCount) {
        FVirtualFreeRange &curr = GVirtualFreeRanges[insertAt];
        FVirtualFreeRange &next = GVirtualFreeRanges[insertAt + 1];
        const uint64_t currEnd = curr.VirtualAddress + curr.PageCount * FVirtualMemoryManager::PageSize;
        if (currEnd == next.VirtualAddress) {
            curr.PageCount += next.PageCount;
            for (size_t i = insertAt + 1; i + 1 < GVirtualFreeRangeCount; i++) {
                GVirtualFreeRanges[i] = GVirtualFreeRanges[i + 1];
            }
            GVirtualFreeRangeCount--;
        }
    }

    return true;
}

static bool UnmapRangeWithRollback(uint64_t virtualAddress,
                                   uint64_t physicalAddress,
                                   uint64_t pageCount,
                                   uint64_t flags) {
    uint64_t unmapped = 0;
    for (uint64_t i = 0; i < pageCount; i++) {
        if (!FVirtualMemoryManager::UnmapPage(virtualAddress + i * FVirtualMemoryManager::PageSize)) {
            for (uint64_t r = 0; r < unmapped; r++) {
                (void)FVirtualMemoryManager::MapPage(virtualAddress + r * FVirtualMemoryManager::PageSize,
                                                     physicalAddress + r * FVirtualMemoryManager::PageSize,
                                                     flags);
            }
            return false;
        }
        unmapped++;
    }
    return true;
}

static void RunMemorySelfTest() {
    PushLog("MEMTEST START");

    bool passed = true;

    const auto pmmBefore = FPhysicalMemoryManager::GetStats();
    const uint64_t pmmPages = 16;
    const uint64_t pmmAddress = FPhysicalMemoryManager::AllocatePages(pmmPages, 1);
    if (pmmAddress == 0 || !FPhysicalMemoryManager::FreePages(pmmAddress, pmmPages)) {
        PushLog("MEMTEST PMM FAIL");
        passed = false;
    } else {
        const auto pmmAfter = FPhysicalMemoryManager::GetStats();
        if (pmmAfter.FreePages != pmmBefore.FreePages) {
            PushLog("MEMTEST PMM LEAK");
            passed = false;
        } else {
            PushLog("MEMTEST PMM OK");
        }
    }

    const uint64_t vmmPages = 4;
    const uint64_t vmmFlags = FVirtualMemoryManager::FlagsKernelRWNX;
    const uint64_t vmmPhysical = FPhysicalMemoryManager::AllocatePages(vmmPages, 1);
    uint64_t vmmVirtual = 0;
    bool vmmOk = true;

    if (vmmPhysical == 0 || !ReserveVirtualRange(vmmPages, vmmVirtual)) {
        vmmOk = false;
    }

    if (vmmOk && !FVirtualMemoryManager::MapPages(vmmVirtual, vmmPhysical, vmmPages, vmmFlags)) {
        vmmOk = false;
    }

    if (vmmOk) {
        const uint64_t xlat0 = FVirtualMemoryManager::Translate(vmmVirtual);
        const uint64_t xlatN = FVirtualMemoryManager::Translate(
            vmmVirtual + (vmmPages - 1) * FVirtualMemoryManager::PageSize);
        if (xlat0 != vmmPhysical || xlatN != (vmmPhysical + (vmmPages - 1) * FVirtualMemoryManager::PageSize)) {
            vmmOk = false;
        }
    }

    if (vmmOk) {
        if (!UnmapRangeWithRollback(vmmVirtual, vmmPhysical, vmmPages, vmmFlags)) {
            vmmOk = false;
        }
    }

    if (vmmOk && !FPhysicalMemoryManager::FreePages(vmmPhysical, vmmPages)) {
        vmmOk = false;
    }

    if (vmmVirtual != 0) {
        (void)ReleaseVirtualRange(vmmVirtual, vmmPages);
    }

    if (!vmmOk) {
        PushLog("MEMTEST VMM FAIL");
        passed = false;
    } else {
        PushLog("MEMTEST VMM OK");
    }

    void *heapA = FKernelHeap::Allocate(256, 16);
    void *heapB = FKernelHeap::Allocate(1536, 32);
    bool heapOk = (heapA != nullptr && heapB != nullptr);
    if (heapOk) {
        heapOk = FKernelHeap::Free(heapA) && FKernelHeap::Free(heapB);
    }

    if (!heapOk) {
        PushLog("MEMTEST HEAP FAIL");
        passed = false;
    } else {
        PushLog("MEMTEST HEAP OK");
    }

    const auto dmaBefore = FDmaMemoryManager::GetStats();
    FDmaBuffer dmaBuffer{};
    bool dmaOk = FDmaMemoryManager::AllocateBuffer(8192, 4096, true, dmaBuffer);
    if (dmaOk) {
        if (dmaBuffer.PhysicalAddress > 0xFFFFFFFFull) {
            dmaOk = false;
        }
    }

    if (dmaOk) {
        dmaOk = FDmaMemoryManager::FreeBuffer(dmaBuffer);
    }

    if (dmaOk) {
        const auto dmaAfter = FDmaMemoryManager::GetStats();
        if (dmaAfter.ReservedPages != dmaBefore.ReservedPages) {
            dmaOk = false;
        }
    }

    if (!dmaOk) {
        PushLog("MEMTEST DMA FAIL");
        passed = false;
    } else {
        PushLog("MEMTEST DMA OK");
    }

    FPinnedMapping pinned{};
    const uint64_t pinPhysical = FPhysicalMemoryManager::AllocatePage();
    bool pinOk = false;
    if (pinPhysical != 0) {
        const uint64_t pinFlags = FVirtualMemoryManager::FlagsKernelRWNX;
        pinOk = FPinnedMappingManager::MapPhysicalRange(pinPhysical, FVirtualMemoryManager::PageSize, pinFlags, pinned);
    }

    if (pinOk) {
        pinOk = (FVirtualMemoryManager::Translate(pinned.VirtualAddress) == pinPhysical);
    }

    if (pinOk) {
        pinOk = FPinnedMappingManager::UnmapPhysicalRange(pinned);
    }

    if (pinPhysical != 0 && !FPhysicalMemoryManager::FreePage(pinPhysical)) {
        pinOk = false;
    }

    if (!pinOk) {
        PushLog("MEMTEST PIN FAIL");
        passed = false;
    } else {
        PushLog("MEMTEST PIN OK");
    }

    PushLog(passed ? "MEMTEST PASS" : "MEMTEST FAIL");
}

static bool BufferIsAligned(const FDmaBuffer &buffer, uint64_t alignment) {
    if (alignment == 0) {
        return true;
    }
    return (buffer.PhysicalAddress % alignment) == 0 && (buffer.VirtualAddress % alignment) == 0;
}

static void RunXhciMemorySmokeTest() {
    PushLog("XHCI MEMTEST START");

    bool passed = true;

    FDmaBuffer dcbaa{};
    FDmaBuffer cmdRing{};
    FDmaBuffer evtSegTable{};
    FDmaBuffer evtRing{};
    FDmaBuffer trRing{};
    FDmaBuffer inputCtx{};

    bool ok = FDmaMemoryManager::AllocateBuffer(4096, 64, true, dcbaa);
    if (ok) {
        ok = BufferIsAligned(dcbaa, 64);
    }

    if (ok) {
        ok = FDmaMemoryManager::AllocateBuffer(4096, 64, true, cmdRing);
    }
    if (ok) {
        ok = BufferIsAligned(cmdRing, 64);
    }

    if (ok) {
        ok = FDmaMemoryManager::AllocateBuffer(4096, 64, true, evtSegTable);
    }
    if (ok) {
        ok = BufferIsAligned(evtSegTable, 64);
    }

    if (ok) {
        ok = FDmaMemoryManager::AllocateBuffer(16384, 64, true, evtRing);
    }
    if (ok) {
        ok = BufferIsAligned(evtRing, 64);
    }

    if (ok) {
        ok = FDmaMemoryManager::AllocateBuffer(16384, 64, true, trRing);
    }
    if (ok) {
        ok = BufferIsAligned(trRing, 64);
    }

    if (ok) {
        ok = FDmaMemoryManager::AllocateBuffer(4096, 64, true, inputCtx);
    }
    if (ok) {
        ok = BufferIsAligned(inputCtx, 64);
    }

    if (!ok) {
        passed = false;
        PushLog("XHCI MEMTEST ALLOC FAIL");
    }

    if (passed && dcbaa.PhysicalAddress > 0xFFFFFFFFull) {
        passed = false;
        PushLog("XHCI MEMTEST >4G FAIL");
    }
    if (passed && cmdRing.PhysicalAddress > 0xFFFFFFFFull) {
        passed = false;
        PushLog("XHCI MEMTEST >4G FAIL");
    }
    if (passed && evtSegTable.PhysicalAddress > 0xFFFFFFFFull) {
        passed = false;
        PushLog("XHCI MEMTEST >4G FAIL");
    }
    if (passed && evtRing.PhysicalAddress > 0xFFFFFFFFull) {
        passed = false;
        PushLog("XHCI MEMTEST >4G FAIL");
    }
    if (passed && trRing.PhysicalAddress > 0xFFFFFFFFull) {
        passed = false;
        PushLog("XHCI MEMTEST >4G FAIL");
    }
    if (passed && inputCtx.PhysicalAddress > 0xFFFFFFFFull) {
        passed = false;
        PushLog("XHCI MEMTEST >4G FAIL");
    }

    bool freeOk = true;
    if (inputCtx.Valid && !FDmaMemoryManager::FreeBuffer(inputCtx)) {
        freeOk = false;
    }
    if (trRing.Valid && !FDmaMemoryManager::FreeBuffer(trRing)) {
        freeOk = false;
    }
    if (evtRing.Valid && !FDmaMemoryManager::FreeBuffer(evtRing)) {
        freeOk = false;
    }
    if (evtSegTable.Valid && !FDmaMemoryManager::FreeBuffer(evtSegTable)) {
        freeOk = false;
    }
    if (cmdRing.Valid && !FDmaMemoryManager::FreeBuffer(cmdRing)) {
        freeOk = false;
    }
    if (dcbaa.Valid && !FDmaMemoryManager::FreeBuffer(dcbaa)) {
        freeOk = false;
    }

    if (!freeOk) {
        passed = false;
        PushLog("XHCI MEMTEST FREE FAIL");
    }

    PushLog(passed ? "XHCI MEMTEST PASS" : "XHCI MEMTEST FAIL");
}

static void RunMmioSmokeTest() {
    PushLog("MMIO TEST START");

    bool passed = true;
    const uint64_t page = FPhysicalMemoryManager::AllocatePage();
    if (page == 0) {
        PushLog("MMIO TEST ALLOC FAIL");
        return;
    }

    FPinnedMapping mapping{};
    if (!FPinnedMappingManager::MapPhysicalRange(page,
                                                 FVirtualMemoryManager::PageSize,
                                                 FVirtualMemoryManager::FlagsDeviceRWUCNX,
                                                 mapping)) {
        (void)FPhysicalMemoryManager::FreePage(page);
        PushLog("MMIO TEST MAP FAIL");
        return;
    }

    const uint64_t mmioBase = mapping.VirtualAddress;
    FMmio::Write32(mmioBase + 0x0, 0x11223344u);
    FMmio::Write32(mmioBase + 0x4, 0xAABBCCDDu);
    FMmio::WriteBarrier();

    const uint32_t r0 = FMmio::Read32(mmioBase + 0x0);
    const uint32_t r1 = FMmio::Read32(mmioBase + 0x4);
    FMmio::ReadBarrier();

    if (r0 != 0x11223344u || r1 != 0xAABBCCDDu) {
        passed = false;
        PushLog("MMIO TEST RW FAIL");
    }

    FPinnedMapping duplicate{};
    if (FPinnedMappingManager::MapPhysicalRange(page,
                                                FVirtualMemoryManager::PageSize,
                                                FVirtualMemoryManager::FlagsDeviceRWUCNX,
                                                duplicate)) {
        passed = false;
        PushLog("MMIO TEST OWNERSHIP FAIL");
        (void)FPinnedMappingManager::UnmapPhysicalRange(duplicate);
    }

    if (!FPinnedMappingManager::UnmapPhysicalRange(mapping)) {
        passed = false;
        PushLog("MMIO TEST UNMAP FAIL");
    }

    if (!FPhysicalMemoryManager::FreePage(page)) {
        passed = false;
        PushLog("MMIO TEST FREE FAIL");
    }

    PushLog(passed ? "MMIO TEST PASS" : "MMIO TEST FAIL");
}

static bool IsVirtualRangeMapped(uint64_t virtualAddress, uint64_t sizeBytes) {
    if (virtualAddress == 0 || sizeBytes == 0) {
        return false;
    }

    const uint64_t pageSize = FVirtualMemoryManager::PageSize;
    const uint64_t start = virtualAddress & ~(pageSize - 1ull);
    const uint64_t endInclusive = virtualAddress + sizeBytes - 1ull;
    const uint64_t end = endInclusive & ~(pageSize - 1ull);

    for (uint64_t page = start;; page += pageSize) {
        if (FVirtualMemoryManager::Translate(page) == 0) {
            return false;
        }

        if (page == end) {
            break;
        }
    }

    return true;
}

static bool IsHigherHalfCanonicalAddress(uint64_t address) {
    return address >= 0xFFFF800000000000ull;
}

static void CpuPause() {
    __asm__ volatile("pause");
}

static bool WaitForUsbStsBit(FXhciMmioRegisters &regs,
                             uint32_t bitMask,
                             bool set,
                             uint64_t maxPolls,
                             uint64_t &outPolls) {
    outPolls = 0;
    for (uint64_t i = 0; i < maxPolls; i++) {
        const uint32_t status = regs.ReadUsbSts();
        const bool bitSet = (status & bitMask) != 0;
        if (bitSet == set) {
            outPolls = i + 1;
            return true;
        }
        CpuPause();
        YieldLongOperationPoll(i + 1);
    }
    return false;
}

static bool WaitForUsbCmdBit(FXhciMmioRegisters &regs,
                             uint32_t bitMask,
                             bool set,
                             uint64_t maxPolls,
                             uint64_t &outPolls) {
    outPolls = 0;
    for (uint64_t i = 0; i < maxPolls; i++) {
        const uint32_t cmd = regs.ReadUsbCmd();
        const bool bitSet = (cmd & bitMask) != 0;
        if (bitSet == set) {
            outPolls = i + 1;
            return true;
        }
        CpuPause();
        YieldLongOperationPoll(i + 1);
    }
    return false;
}

static bool IsXhciRegisterWindowSafe(const FXhciMmioRegisters &regs,
                                     uint64_t mappedVirtualBase,
                                     uint64_t mappedSizeBytes) {
    const uint64_t mapBase = mappedVirtualBase;
    const uint64_t mapEnd = mapBase + mappedSizeBytes;

    const uint64_t capBase = regs.GetCapabilityBase();
    const uint64_t opBase = regs.GetOperationalBase();
    const uint64_t dbBase = regs.GetDoorbellBase();
    const uint64_t rtBase = regs.GetRuntimeBase();

    const uint64_t capEnd = capBase + 0x80ull;
    const uint64_t opEnd = opBase + 0x40ull;
    const uint64_t dbEnd = dbBase + 0x10ull;
    const uint64_t rtEnd = rtBase + 0x40ull;

    const bool capOk = capBase >= mapBase && capEnd <= mapEnd;
    const bool opOk = opBase >= mapBase && opEnd <= mapEnd;
    const bool dbOk = dbBase >= mapBase && dbEnd <= mapEnd;
    const bool rtOk = rtBase >= mapBase && rtEnd <= mapEnd;

    return capOk && opOk && dbOk && rtOk;
}

static void WriteLe64(volatile uint32_t *words, uint64_t value) {
    words[0] = static_cast<uint32_t>(value & 0xFFFFFFFFull);
    words[1] = static_cast<uint32_t>((value >> 32u) & 0xFFFFFFFFull);
}

static bool WaitForCommandCompletion(volatile uint32_t *eventRingWords,
                                     uint32_t trbCount,
                                     uint64_t expectedCommandPointer,
                                     uint64_t maxPolls,
                                     uint32_t &outCompletionCode,
                                     uint32_t &outSlotId,
                                     uint32_t &outEventIndex) {
    outCompletionCode = 0;
    outSlotId = 0;
    outEventIndex = 0;

    for (uint64_t poll = 0; poll < maxPolls; poll++) {
        for (uint32_t i = 0; i < trbCount; i++) {
            const volatile uint32_t *trb = eventRingWords + (i * 4u);
            const uint32_t control = trb[3];
            const uint32_t cycle = control & 0x1u;
            const uint32_t type = (control >> 10u) & 0x3Fu;
            if (cycle == 0u || type != 33u) {
                continue;
            }

            const uint64_t commandPointer =
                static_cast<uint64_t>(trb[0]) | (static_cast<uint64_t>(trb[1]) << 32u);
            if ((commandPointer & ~0xFull) != (expectedCommandPointer & ~0xFull)) {
                continue;
            }

            outCompletionCode = (trb[2] >> 24u) & 0xFFu;
            outSlotId = (control >> 24u) & 0xFFu;
            outEventIndex = i;

            // Clear consumed event entry so polling does not match stale completions.
            volatile uint32_t *mutableTrb = eventRingWords + (i * 4u);
            mutableTrb[0] = 0;
            mutableTrb[1] = 0;
            mutableTrb[2] = 0;
            mutableTrb[3] = 0;
            return true;
        }
        CpuPause();
        YieldLongOperationPoll(poll + 1);
    }

    return false;
}

static bool WaitForTransferCompletion(volatile uint32_t *eventRingWords,
                                      uint32_t trbCount,
                                      uint64_t expectedTrbPointerA,
                                      uint64_t expectedTrbPointerB,
                                      uint64_t expectedTrbPointerC,
                                      uint32_t expectedSlotId,
                                      uint32_t expectedEndpointId,
                                      uint64_t maxPolls,
                                      uint32_t &outCompletionCode,
                                      uint32_t &outEndpointId,
                                      uint32_t &outEventIndex) {
    (void)expectedTrbPointerA;
    (void)expectedTrbPointerB;
    (void)expectedTrbPointerC;
    outCompletionCode = 0;
    outEndpointId = 0;
    outEventIndex = 0;

    for (uint64_t poll = 0; poll < maxPolls; poll++) {
        for (uint32_t i = 0; i < trbCount; i++) {
            const volatile uint32_t *trb = eventRingWords + (i * 4u);
            const uint32_t control = trb[3];
            const uint32_t cycle = control & 0x1u;
            const uint32_t type = (control >> 10u) & 0x3Fu;
            if (type != 32u) {
                continue;
            }

            // Some controllers may publish a transfer event with an unexpected cycle while
            // software is still in early boot polling mode; reject only fully empty entries.
            if (cycle == 0u && trb[0] == 0u && trb[1] == 0u && trb[2] == 0u) {
                continue;
            }

            const uint32_t slotId = (control >> 24u) & 0xFFu;
            if (slotId != expectedSlotId) {
                continue;
            }

            const uint32_t endpointId = (control >> 16u) & 0x1Fu;
            if (expectedEndpointId != 0u && endpointId != expectedEndpointId) {
                continue;
            }

            outCompletionCode = (trb[2] >> 24u) & 0xFFu;
            outEndpointId = endpointId;
            outEventIndex = i;

            // Clear consumed event entry so subsequent polls do not repeatedly match it.
            volatile uint32_t *mutableTrb = eventRingWords + (i * 4u);
            mutableTrb[0] = 0;
            mutableTrb[1] = 0;
            mutableTrb[2] = 0;
            mutableTrb[3] = 0;
            return true;
        }
        CpuPause();
        YieldLongOperationPoll(poll + 1);
    }

    return false;
}

static void AckConsumedEvent(FXhciMmioRegisters &regs,
                             uint64_t eventRingPhysicalBase,
                             uint32_t eventIndex) {
    const uint64_t eventAddress = (eventRingPhysicalBase + (static_cast<uint64_t>(eventIndex) * 16ull)) & ~0xFull;
    regs.WriteInterrupterErdp(0, eventAddress | (1ull << 3u));
}

static bool DrainOneEventTrb(FXhciMmioRegisters &regs,
                             volatile uint32_t *eventRingWords,
                             uint32_t trbCount,
                             uint64_t eventRingPhysicalBase,
                             uint32_t &outType,
                             uint32_t &outSlot,
                             uint32_t &outEndpoint,
                             uint32_t &outCompletionCode) {
    outType = 0u;
    outSlot = 0u;
    outEndpoint = 0u;
    outCompletionCode = 0u;

    for (uint32_t i = 0; i < trbCount; i++) {
        volatile uint32_t *trb = eventRingWords + (i * 4u);
        const uint32_t d0 = trb[0];
        const uint32_t d1 = trb[1];
        const uint32_t d2 = trb[2];
        const uint32_t d3 = trb[3];
        if (d0 == 0u && d1 == 0u && d2 == 0u && d3 == 0u) {
            continue;
        }

        outType = (d3 >> 10u) & 0x3Fu;
        outSlot = (d3 >> 24u) & 0xFFu;
        outEndpoint = (d3 >> 16u) & 0x1Fu;
        outCompletionCode = (d2 >> 24u) & 0xFFu;

        trb[0] = 0u;
        trb[1] = 0u;
        trb[2] = 0u;
        trb[3] = 0u;
        AckConsumedEvent(regs, eventRingPhysicalBase, i);
        return true;
    }

    return false;
}

static void LogEventRingDebug(const volatile uint32_t *eventRingWords,
                              uint32_t trbCount,
                              uint32_t maxEntriesToLog) {
    uint32_t logged = 0;
    for (uint32_t i = 0; i < trbCount && logged < maxEntriesToLog; i++) {
        const volatile uint32_t *trb = eventRingWords + (i * 4u);
        const uint32_t d0 = trb[0];
        const uint32_t d1 = trb[1];
        const uint32_t d2 = trb[2];
        const uint32_t d3 = trb[3];
        if (d0 == 0u && d1 == 0u && d2 == 0u && d3 == 0u) {
            continue;
        }

        const uint32_t type = (d3 >> 10u) & 0x3Fu;
        const uint32_t cycle = d3 & 0x1u;
        const uint32_t slot = (d3 >> 24u) & 0xFFu;
        const uint32_t ep = (d3 >> 16u) & 0x1Fu;
        const uint32_t cc = (d2 >> 24u) & 0xFFu;

        char line[220] = {};
        size_t pos = 0;
        AppendString(line, sizeof(line), pos, "XHCI EVT I ");
        AppendUInt(line, sizeof(line), pos, i);
        AppendString(line, sizeof(line), pos, " T ");
        AppendUInt(line, sizeof(line), pos, type);
        AppendString(line, sizeof(line), pos, " C ");
        AppendUInt(line, sizeof(line), pos, cycle);
        AppendString(line, sizeof(line), pos, " S ");
        AppendUInt(line, sizeof(line), pos, slot);
        AppendString(line, sizeof(line), pos, " EP ");
        AppendUInt(line, sizeof(line), pos, ep);
        AppendString(line, sizeof(line), pos, " CC ");
        AppendUInt(line, sizeof(line), pos, cc);
        PushLog(line);

        logged++;
    }

    if (logged == 0u) {
        PushLog("XHCI EVT EMPTY");
    }
}

static void LogXhciRegisterSnapshot(uint64_t capabilityBaseVirtualAddress) {
    // We read through CAP and OP regs up to roughly +0x78 from the capability base.
    if (!IsVirtualRangeMapped(capabilityBaseVirtualAddress, 0x80)) {
        PushLog("XHCIREGS VA UNMAPPED");
        PushLog("USE XHCIREGS OR XHCIPROBE");
        return;
    }

    FXhciMmioRegisters regs;
    if (!regs.Initialize(capabilityBaseVirtualAddress)) {
        PushLog("XHCI REGS INIT FAIL");
        return;
    }

    const uint64_t capLength = regs.ReadCapLength();
    if (capLength < 0x20ull || capLength > 0x400ull) {
        PushLog("XHCIREGS CAPLEN INVALID");
        return;
    }

    if (!IsVirtualRangeMapped(capabilityBaseVirtualAddress + capLength, 0x40)) {
        PushLog("XHCIREGS OP UNMAPPED");
        PushLog("USE XHCIREGS OR XHCIPROBE");
        return;
    }

    char line[96] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "XHCI CAPLEN ");
    AppendUInt(line, sizeof(line), pos, regs.ReadCapLength());
    AppendString(line, sizeof(line), pos, " VER ");
    AppendHex(line, sizeof(line), pos, regs.ReadHciVersion());
    PushLog(line);

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "XHCI HCSP1 ");
    AppendHex(line, sizeof(line), pos, regs.ReadHcsParams1());
    AppendString(line, sizeof(line), pos, " DB ");
    AppendHex(line, sizeof(line), pos, regs.ReadDbOff());
    PushLog(line);

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "XHCI RTS ");
    AppendHex(line, sizeof(line), pos, regs.ReadRtsOff());
    AppendString(line, sizeof(line), pos, " OP ");
    AppendHex(line, sizeof(line), pos, regs.GetOperationalBase());
    PushLog(line);

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "XHCI CMD ");
    AppendHex(line, sizeof(line), pos, regs.ReadUsbCmd());
    AppendString(line, sizeof(line), pos, " STS ");
    AppendHex(line, sizeof(line), pos, regs.ReadUsbSts());
    PushLog(line);

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "XHCI CRCR ");
    AppendHex(line, sizeof(line), pos, regs.ReadCrcr());
    AppendString(line, sizeof(line), pos, " CFG ");
    AppendHex(line, sizeof(line), pos, regs.ReadConfig());
    PushLog(line);
}

static bool LogXhciRegisterSnapshotFromPhysical(uint64_t capabilityBasePhysicalAddress) {
    FPinnedMapping mapping{};
    if (!FPinnedMappingManager::MapPhysicalRange(capabilityBasePhysicalAddress,
                                                 0x1000,
                                                 FVirtualMemoryManager::FlagsDeviceRWUCNX,
                                                 mapping)) {
        return false;
    }

    LogXhciRegisterSnapshot(mapping.VirtualAddress);
    const bool unmapOk = FPinnedMappingManager::UnmapPhysicalRange(mapping);
    return unmapOk;
}

static void RunXhciAutoProbe() {
    PushLog("XHCI PROBE START");

    FXhciControllerInfo info{};
    if (!FXhciPciDiscovery::DiscoverFirst(info) || !info.Found) {
        PushLog("XHCI PROBE NONE");
        return;
    }

    char line[96] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "XHCI BDF ");
    AppendUInt(line, sizeof(line), pos, info.Bus);
    AppendChar(line, sizeof(line), pos, ':');
    AppendUInt(line, sizeof(line), pos, info.Device);
    AppendChar(line, sizeof(line), pos, '.');
    AppendUInt(line, sizeof(line), pos, info.Function);
    AppendString(line, sizeof(line), pos, " VID ");
    AppendHex(line, sizeof(line), pos, info.VendorId);
    AppendString(line, sizeof(line), pos, " DID ");
    AppendHex(line, sizeof(line), pos, info.DeviceId);
    PushLog(line);

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "XHCI BAR0 ");
    AppendHex(line, sizeof(line), pos, info.MmioBase);
    AppendString(line, sizeof(line), pos, " SZ ");
    AppendHex(line, sizeof(line), pos, info.MmioSize);
    PushLog(line);

    FPinnedMapping mapping{};
    if (!FPinnedMappingManager::MapPhysicalRange(info.MmioBase,
                                                 info.MmioSize,
                                                 FVirtualMemoryManager::FlagsDeviceRWUCNX,
                                                 mapping)) {
        PushLog("XHCI PROBE MAP FAIL");
        return;
    }

    LogXhciRegisterSnapshot(mapping.VirtualAddress);

    if (!FPinnedMappingManager::UnmapPhysicalRange(mapping)) {
        PushLog("XHCI PROBE UNMAP FAIL");
        return;
    }

    PushLog("XHCI PROBE PASS");
}

static void RunXhciInit() {
    PushLog("XHCI INIT START");

    FXhciControllerInfo info{};
    if (!FXhciPciDiscovery::DiscoverFirst(info) || !info.Found) {
        PushLog("XHCI INIT NONE");
        return;
    }

    FPinnedMapping mapping{};
    if (!FPinnedMappingManager::MapPhysicalRange(info.MmioBase,
                                                 info.MmioSize,
                                                 FVirtualMemoryManager::FlagsDeviceRWUCNX,
                                                 mapping)) {
        PushLog("XHCI INIT MAP FAIL");
        return;
    }

    FXhciMmioRegisters regs;
    if (!regs.Initialize(mapping.VirtualAddress)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mapping);
        PushLog("XHCI INIT REGS FAIL");
        return;
    }

    constexpr uint32_t UsbCmdRunStop = 1u << 0;
    constexpr uint32_t UsbCmdHostControllerReset = 1u << 1;
    constexpr uint32_t UsbStsHostControllerHalted = 1u << 0;
    constexpr uint32_t UsbStsControllerNotReady = 1u << 11;
    constexpr uint64_t MaxPolls = 2000000ull;

    uint64_t polls = 0;
    if (!WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mapping);
        PushLog("XHCI INIT CNR TIMEOUT");
        return;
    }

    char line[96] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "XHCI INIT CNR CLEAR ");
    AppendUInt(line, sizeof(line), pos, polls);
    PushLog(line);

    uint32_t cmd = regs.ReadUsbCmd();
    regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
    if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mapping);
        PushLog("XHCI INIT HALT TIMEOUT");
        return;
    }

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "XHCI INIT HALTED ");
    AppendUInt(line, sizeof(line), pos, polls);
    PushLog(line);

    cmd = regs.ReadUsbCmd();
    regs.WriteUsbCmd(cmd | UsbCmdHostControllerReset);
    if (!WaitForUsbCmdBit(regs, UsbCmdHostControllerReset, false, MaxPolls, polls)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mapping);
        PushLog("XHCI INIT RESET TIMEOUT");
        return;
    }

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "XHCI INIT RESET DONE ");
    AppendUInt(line, sizeof(line), pos, polls);
    PushLog(line);

    if (!WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mapping);
        PushLog("XHCI INIT POSTRST CNR TIMEOUT");
        return;
    }

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "XHCI INIT READY ");
    AppendUInt(line, sizeof(line), pos, polls);
    PushLog(line);

    cmd = regs.ReadUsbCmd();
    regs.WriteUsbCmd(cmd | UsbCmdRunStop);
    if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, false, MaxPolls, polls)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mapping);
        PushLog("XHCI INIT RUN TIMEOUT");
        return;
    }

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "XHCI INIT RUNNING ");
    AppendUInt(line, sizeof(line), pos, polls);
    PushLog(line);

    cmd = regs.ReadUsbCmd();
    regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
    if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mapping);
        PushLog("XHCI INIT STOP TIMEOUT");
        return;
    }

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "XHCI INIT STOPPED ");
    AppendUInt(line, sizeof(line), pos, polls);
    PushLog(line);

    if (!FPinnedMappingManager::UnmapPhysicalRange(mapping)) {
        PushLog("XHCI INIT UNMAP FAIL");
        return;
    }

    PushLog("XHCI INIT PASS");
}

static void RunXhciRingTest() {
    PushLog("XHCI RINGTEST START");

    FXhciControllerInfo info{};
    if (!FXhciPciDiscovery::DiscoverFirst(info) || !info.Found) {
        PushLog("XHCI RINGTEST NONE");
        return;
    }

    FPinnedMapping mmio{};
    if (!FPinnedMappingManager::MapPhysicalRange(info.MmioBase,
                                                 info.MmioSize,
                                                 FVirtualMemoryManager::FlagsDeviceRWUCNX,
                                                 mmio)) {
        PushLog("XHCI RINGTEST MAP FAIL");
        return;
    }

    FXhciMmioRegisters regs;
    if (!regs.Initialize(mmio.VirtualAddress)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog("XHCI RINGTEST REGS FAIL");
        return;
    }

    const uint64_t mappedVirtualBase = mmio.VirtualAddress - (info.MmioBase - (info.MmioBase & ~(FVirtualMemoryManager::PageSize - 1ull)));
    const uint64_t mappedSizeBytes = mmio.PageCount * FVirtualMemoryManager::PageSize;
    if (!IsXhciRegisterWindowSafe(regs, mappedVirtualBase, mappedSizeBytes)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog("XHCI RINGTEST MMIO WINDOW FAIL");
        return;
    }

    constexpr uint32_t UsbCmdRunStop = 1u << 0;
    constexpr uint32_t UsbCmdHostControllerReset = 1u << 1;
    constexpr uint32_t UsbStsHostControllerHalted = 1u << 0;
    constexpr uint32_t UsbStsControllerNotReady = 1u << 11;
    constexpr uint32_t UsbCmdInterruptEnable = 1u << 2;

    constexpr uint64_t MaxPolls = 750000ull;
    constexpr uint32_t TrbTypeLink = 6u;
    constexpr uint32_t TrbTypeNoOpCommand = 23u;
    constexpr uint32_t TrbTypeCommandCompletion = 33u;

    bool passed = true;
    uint64_t polls = 0;

    FDmaBuffer dcbaa{};
    FDmaBuffer cmdRing{};
    FDmaBuffer evtRing{};
    FDmaBuffer erst{};

    bool haveDcbaa = false;
    bool haveCmdRing = false;
    bool haveEvtRing = false;
    bool haveErst = false;

    if (!WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog("XHCI RINGTEST CNR TIMEOUT");
        passed = false;
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls)) {
            PushLog("XHCI RINGTEST HALT TIMEOUT");
            passed = false;
        }
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdHostControllerReset);
        if (!WaitForUsbCmdBit(regs, UsbCmdHostControllerReset, false, MaxPolls, polls)) {
            PushLog("XHCI RINGTEST RESET TIMEOUT");
            passed = false;
        }
    }

    if (passed && !WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog("XHCI RINGTEST READY TIMEOUT");
        passed = false;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, dcbaa)) {
        PushLog("XHCI RINGTEST DCBAA FAIL");
        passed = false;
    } else if (passed) {
        haveDcbaa = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, cmdRing)) {
        PushLog("XHCI RINGTEST CR FAIL");
        passed = false;
    } else if (passed) {
        haveCmdRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, evtRing)) {
        PushLog("XHCI RINGTEST ER FAIL");
        passed = false;
    } else if (passed) {
        haveEvtRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, erst)) {
        PushLog("XHCI RINGTEST ERST FAIL");
        passed = false;
    } else if (passed) {
        haveErst = true;
    }

    if (passed && ((dcbaa.PhysicalAddress & 0x3Full) != 0 || (cmdRing.PhysicalAddress & 0x3Full) != 0 ||
                   (evtRing.PhysicalAddress & 0x3Full) != 0 || (erst.PhysicalAddress & 0x3Full) != 0)) {
        PushLog("XHCI RINGTEST ALIGN FAIL");
        passed = false;
    }

    if (passed) {
        Fortress::Runtime::Memset(reinterpret_cast<void *>(dcbaa.VirtualAddress), 0, static_cast<Fortress::Core::usize>(dcbaa.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(cmdRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(cmdRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(evtRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(evtRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(erst.VirtualAddress), 0, static_cast<Fortress::Core::usize>(erst.SizeBytes));

        volatile uint32_t *cmdWords = reinterpret_cast<volatile uint32_t *>(cmdRing.VirtualAddress);
        cmdWords[3] = (TrbTypeNoOpCommand << 10u) | 1u;

        constexpr uint32_t CmdRingTrbCount = 256u;
        volatile uint32_t *linkWords = cmdWords + ((CmdRingTrbCount - 1u) * 4u);
        WriteLe64(linkWords, cmdRing.PhysicalAddress);
        linkWords[2] = 0;
        linkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

        volatile uint32_t *erstWords = reinterpret_cast<volatile uint32_t *>(erst.VirtualAddress);
        WriteLe64(erstWords, evtRing.PhysicalAddress);
        erstWords[2] = 256u;
        erstWords[3] = 0;
    }

    if (passed) {
        regs.WriteConfig(8u);
        regs.WriteDcbaap(dcbaa.PhysicalAddress & ~0x3Full);
        regs.WriteCrcr((cmdRing.PhysicalAddress & ~0x3Full) | 1ull);

        regs.WriteInterrupterIman(0, 0u);
        regs.WriteInterrupterImod(0, 0u);
        regs.WriteInterrupterErstsz(0, 1u);
        regs.WriteInterrupterErstba(0, erst.PhysicalAddress & ~0x3Full);
        regs.WriteInterrupterErdp(0, evtRing.PhysicalAddress & ~0xFull);

        const uint64_t dcbaapRead = regs.ReadDcbaap() & ~0x3Full;
        const uint64_t crcrRead = regs.ReadCrcr() & ~0x3Full;
        const uint64_t erstbaRead = regs.ReadInterrupterErstba(0) & ~0x3Full;

        if (dcbaapRead != (dcbaa.PhysicalAddress & ~0x3Full) ||
            crcrRead != (cmdRing.PhysicalAddress & ~0x3Full) ||
            erstbaRead != (erst.PhysicalAddress & ~0x3Full)) {
            PushLog("XHCI RINGTEST LATCH FAIL");
            passed = false;
        }
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdRunStop | UsbCmdInterruptEnable);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, false, MaxPolls, polls)) {
            PushLog("XHCI RINGTEST RUN TIMEOUT");
            passed = false;
        }
    }

    if (passed) {
        regs.RingDoorbell(0, 0, 0);

        volatile uint32_t *eventWords = reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress);
        bool completionSeen = false;
        uint32_t completionCode = 0;

        for (uint64_t i = 0; i < MaxPolls; i++) {
            const uint32_t control = eventWords[3];
            const uint32_t type = (control >> 10u) & 0x3Fu;
            const uint32_t cycle = control & 0x1u;

            if (cycle == 1u && type == TrbTypeCommandCompletion) {
                completionCode = (eventWords[2] >> 24u) & 0xFFu;
                completionSeen = true;
                break;
            }
            CpuPause();
        }

        if (!completionSeen) {
            PushLog("XHCI RINGTEST NOEVENT");
            passed = false;
        } else if (completionCode != 1u) {
            PushLog("XHCI RINGTEST CC FAIL");
            passed = false;
        } else {
            PushLog("XHCI RINGTEST NOOP OK");
        }
    }

    {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        (void)WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls);
    }

    bool cleanupOk = true;
    if (haveErst && !FDmaMemoryManager::FreeBuffer(erst)) {
        cleanupOk = false;
    }
    if (haveEvtRing && !FDmaMemoryManager::FreeBuffer(evtRing)) {
        cleanupOk = false;
    }
    if (haveCmdRing && !FDmaMemoryManager::FreeBuffer(cmdRing)) {
        cleanupOk = false;
    }
    if (haveDcbaa && !FDmaMemoryManager::FreeBuffer(dcbaa)) {
        cleanupOk = false;
    }
    if (!FPinnedMappingManager::UnmapPhysicalRange(mmio)) {
        cleanupOk = false;
    }

    if (!cleanupOk) {
        PushLog("XHCI RINGTEST CLEANUP FAIL");
    }

    PushLog(passed ? "XHCI RINGTEST PASS" : "XHCI RINGTEST FAIL");
}

static void RunXhciEnum() {
    PushLog("XHCI ENUM START");

    FXhciControllerInfo info{};
    if (!FXhciPciDiscovery::DiscoverFirst(info) || !info.Found) {
        PushLog("XHCI ENUM NONE");
        return;
    }

    FPinnedMapping mmio{};
    if (!FPinnedMappingManager::MapPhysicalRange(info.MmioBase,
                                                 info.MmioSize,
                                                 FVirtualMemoryManager::FlagsDeviceRWUCNX,
                                                 mmio)) {
        PushLog("XHCI ENUM MAP FAIL");
        return;
    }

    FXhciMmioRegisters regs;
    if (!regs.Initialize(mmio.VirtualAddress)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog("XHCI ENUM REGS FAIL");
        return;
    }

    const uint64_t mappedVirtualBase = mmio.VirtualAddress - (info.MmioBase - (info.MmioBase & ~(FVirtualMemoryManager::PageSize - 1ull)));
    const uint64_t mappedSizeBytes = mmio.PageCount * FVirtualMemoryManager::PageSize;
    if (!IsXhciRegisterWindowSafe(regs, mappedVirtualBase, mappedSizeBytes)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog("XHCI ENUM MMIO WINDOW FAIL");
        return;
    }

    constexpr uint32_t UsbCmdRunStop = 1u << 0;
    constexpr uint32_t UsbCmdHostControllerReset = 1u << 1;
    constexpr uint32_t UsbCmdInterruptEnable = 1u << 2;
    constexpr uint32_t UsbStsHostControllerHalted = 1u << 0;
    constexpr uint32_t UsbStsControllerNotReady = 1u << 11;
    constexpr uint64_t MaxPolls = 800000ull;

    constexpr uint32_t TrbTypeLink = 6u;
    constexpr uint32_t TrbTypeEnableSlot = 9u;
    constexpr uint32_t TrbTypeCommandCompletion = 33u;

    FDmaBuffer dcbaa{};
    FDmaBuffer cmdRing{};
    FDmaBuffer evtRing{};
    FDmaBuffer erst{};
    bool haveDcbaa = false;
    bool haveCmdRing = false;
    bool haveEvtRing = false;
    bool haveErst = false;

    bool passed = true;
    uint64_t polls = 0;

    if (!WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog("XHCI ENUM CNR TIMEOUT");
        passed = false;
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls)) {
            PushLog("XHCI ENUM HALT TIMEOUT");
            passed = false;
        }
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdHostControllerReset);
        if (!WaitForUsbCmdBit(regs, UsbCmdHostControllerReset, false, MaxPolls, polls)) {
            PushLog("XHCI ENUM RESET TIMEOUT");
            passed = false;
        }
    }

    if (passed && !WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog("XHCI ENUM READY TIMEOUT");
        passed = false;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, dcbaa)) {
        PushLog("XHCI ENUM DCBAA FAIL");
        passed = false;
    } else if (passed) {
        haveDcbaa = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, cmdRing)) {
        PushLog("XHCI ENUM CR FAIL");
        passed = false;
    } else if (passed) {
        haveCmdRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, evtRing)) {
        PushLog("XHCI ENUM ER FAIL");
        passed = false;
    } else if (passed) {
        haveEvtRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, erst)) {
        PushLog("XHCI ENUM ERST FAIL");
        passed = false;
    } else if (passed) {
        haveErst = true;
    }

    if (passed) {
        Fortress::Runtime::Memset(reinterpret_cast<void *>(dcbaa.VirtualAddress), 0, static_cast<Fortress::Core::usize>(dcbaa.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(cmdRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(cmdRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(evtRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(evtRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(erst.VirtualAddress), 0, static_cast<Fortress::Core::usize>(erst.SizeBytes));

        volatile uint32_t *cmdWords = reinterpret_cast<volatile uint32_t *>(cmdRing.VirtualAddress);
        cmdWords[0] = 0;
        cmdWords[1] = 0;
        cmdWords[2] = 0;
        cmdWords[3] = (TrbTypeEnableSlot << 10u) | 1u;

        constexpr uint32_t CmdRingTrbCount = 256u;
        volatile uint32_t *linkWords = cmdWords + ((CmdRingTrbCount - 1u) * 4u);
        WriteLe64(linkWords, cmdRing.PhysicalAddress);
        linkWords[2] = 0;
        linkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

        volatile uint32_t *erstWords = reinterpret_cast<volatile uint32_t *>(erst.VirtualAddress);
        WriteLe64(erstWords, evtRing.PhysicalAddress);
        erstWords[2] = 256u;
        erstWords[3] = 0;
    }

    if (passed) {
        regs.WriteConfig(8u);
        regs.WriteDcbaap(dcbaa.PhysicalAddress & ~0x3Full);
        regs.WriteCrcr((cmdRing.PhysicalAddress & ~0x3Full) | 1ull);

        regs.WriteInterrupterIman(0, 0u);
        regs.WriteInterrupterImod(0, 0u);
        regs.WriteInterrupterErstsz(0, 1u);
        regs.WriteInterrupterErstba(0, erst.PhysicalAddress & ~0x3Full);
        regs.WriteInterrupterErdp(0, evtRing.PhysicalAddress & ~0xFull);
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdRunStop | UsbCmdInterruptEnable);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, false, MaxPolls, polls)) {
            PushLog("XHCI ENUM RUN TIMEOUT");
            passed = false;
        }
    }

    if (passed) {
        regs.RingDoorbell(0, 0, 0);

        volatile uint32_t *eventWords = reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress);
        bool completionSeen = false;
        uint32_t completionCode = 0;
        uint32_t slotId = 0;

        for (uint64_t i = 0; i < MaxPolls; i++) {
            const uint32_t control = eventWords[3];
            const uint32_t type = (control >> 10u) & 0x3Fu;
            const uint32_t cycle = control & 0x1u;

            if (cycle == 1u && type == TrbTypeCommandCompletion) {
                completionCode = (eventWords[2] >> 24u) & 0xFFu;
                slotId = (control >> 24u) & 0xFFu;
                completionSeen = true;
                break;
            }
            CpuPause();
        }

        if (!completionSeen) {
            PushLog("XHCI ENUM NOEVENT");
            passed = false;
        } else if (completionCode != 1u) {
            PushLog("XHCI ENUM CCS FAIL");
            passed = false;
        } else if (slotId == 0) {
            PushLog("XHCI ENUM SLOTID FAIL");
            passed = false;
        } else {
            char line[96] = {};
            size_t pos = 0;
            AppendString(line, sizeof(line), pos, "XHCI ENUM SLOT ");
            AppendUInt(line, sizeof(line), pos, slotId);
            PushLog(line);
            PushLog("XHCI ENUM ADDRDEV TODO");
        }
    }

    {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        (void)WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls);
    }

    bool cleanupOk = true;
    if (haveErst && !FDmaMemoryManager::FreeBuffer(erst)) {
        cleanupOk = false;
    }
    if (haveEvtRing && !FDmaMemoryManager::FreeBuffer(evtRing)) {
        cleanupOk = false;
    }
    if (haveCmdRing && !FDmaMemoryManager::FreeBuffer(cmdRing)) {
        cleanupOk = false;
    }
    if (haveDcbaa && !FDmaMemoryManager::FreeBuffer(dcbaa)) {
        cleanupOk = false;
    }
    if (!FPinnedMappingManager::UnmapPhysicalRange(mmio)) {
        cleanupOk = false;
    }

    if (!cleanupOk) {
        PushLog("XHCI ENUM CLEANUP FAIL");
    }

    PushLog(passed ? "XHCI ENUM PASS" : "XHCI ENUM FAIL");
}

static void RunXhciAddressDevice() {
    PushLog("XHCI ADDRDEV START");

    FXhciControllerInfo info{};
    if (!FXhciPciDiscovery::DiscoverFirst(info) || !info.Found) {
        PushLog("XHCI ADDRDEV NONE");
        return;
    }

    FPinnedMapping mmio{};
    if (!FPinnedMappingManager::MapPhysicalRange(info.MmioBase,
                                                 info.MmioSize,
                                                 FVirtualMemoryManager::FlagsDeviceRWUCNX,
                                                 mmio)) {
        PushLog("XHCI ADDRDEV MAP FAIL");
        return;
    }

    FXhciMmioRegisters regs;
    if (!regs.Initialize(mmio.VirtualAddress)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog("XHCI ADDRDEV REGS FAIL");
        return;
    }

    const uint64_t mappedVirtualBase = mmio.VirtualAddress - (info.MmioBase - (info.MmioBase & ~(FVirtualMemoryManager::PageSize - 1ull)));
    const uint64_t mappedSizeBytes = mmio.PageCount * FVirtualMemoryManager::PageSize;
    if (!IsXhciRegisterWindowSafe(regs, mappedVirtualBase, mappedSizeBytes)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog("XHCI ADDRDEV MMIO WINDOW FAIL");
        return;
    }

    constexpr uint32_t UsbCmdRunStop = 1u << 0;
    constexpr uint32_t UsbCmdHostControllerReset = 1u << 1;
    constexpr uint32_t UsbCmdInterruptEnable = 1u << 2;
    constexpr uint32_t UsbStsHostControllerHalted = 1u << 0;
    constexpr uint32_t UsbStsControllerNotReady = 1u << 11;
    constexpr uint64_t MaxPolls = 3000000ull;

    constexpr uint32_t TrbTypeLink = 6u;
    constexpr uint32_t TrbTypeEnableSlot = 9u;
    constexpr uint32_t TrbTypeAddressDevice = 11u;
    constexpr uint32_t TrbTypeCommandCompletion = 33u;
    (void)TrbTypeCommandCompletion;

    FDmaBuffer dcbaa{};
    FDmaBuffer outputContext{};
    FDmaBuffer inputContext{};
    FDmaBuffer cmdRing{};
    FDmaBuffer evtRing{};
    FDmaBuffer erst{};
    FDmaBuffer ep0Ring{};

    bool haveDcbaa = false;
    bool haveOutputContext = false;
    bool haveInputContext = false;
    bool haveCmdRing = false;
    bool haveEvtRing = false;
    bool haveErst = false;
    bool haveEp0Ring = false;

    bool passed = true;
    uint64_t polls = 0;

    if (!WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog("XHCI ADDRDEV CNR TIMEOUT");
        passed = false;
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls)) {
            PushLog("XHCI ADDRDEV HALT TIMEOUT");
            passed = false;
        }
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdHostControllerReset);
        if (!WaitForUsbCmdBit(regs, UsbCmdHostControllerReset, false, MaxPolls, polls)) {
            PushLog("XHCI ADDRDEV RESET TIMEOUT");
            passed = false;
        }
    }

    if (passed && !WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog("XHCI ADDRDEV READY TIMEOUT");
        passed = false;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, dcbaa)) {
        PushLog("XHCI ADDRDEV DCBAA FAIL");
        passed = false;
    } else if (passed) {
        haveDcbaa = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, outputContext)) {
        PushLog("XHCI ADDRDEV OUTCTX FAIL");
        passed = false;
    } else if (passed) {
        haveOutputContext = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, inputContext)) {
        PushLog("XHCI ADDRDEV INCTX FAIL");
        passed = false;
    } else if (passed) {
        haveInputContext = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, cmdRing)) {
        PushLog("XHCI ADDRDEV CR FAIL");
        passed = false;
    } else if (passed) {
        haveCmdRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, evtRing)) {
        PushLog("XHCI ADDRDEV ER FAIL");
        passed = false;
    } else if (passed) {
        haveEvtRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, erst)) {
        PushLog("XHCI ADDRDEV ERST FAIL");
        passed = false;
    } else if (passed) {
        haveErst = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, ep0Ring)) {
        PushLog("XHCI ADDRDEV EP0RING FAIL");
        passed = false;
    } else if (passed) {
        haveEp0Ring = true;
    }

    uint8_t connectedPort = 0;
    uint32_t connectedPortSc = 0;
    if (passed) {
        const uint32_t hcs1 = regs.ReadHcsParams1();
        const uint32_t maxPorts = (hcs1 >> 24u) & 0xFFu;
        for (uint8_t port = 1; port <= maxPorts; port++) {
            const uint32_t portSc = regs.ReadPortSc(port);
            if ((portSc & 0x1u) != 0) {
                connectedPort = port;
                connectedPortSc = portSc;
                break;
            }
        }

        if (connectedPort == 0) {
            PushLog("XHCI ADDRDEV NOPORT");
            passed = false;
        }
    }

    if (passed) {
        Fortress::Runtime::Memset(reinterpret_cast<void *>(dcbaa.VirtualAddress), 0, static_cast<Fortress::Core::usize>(dcbaa.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(outputContext.VirtualAddress), 0, static_cast<Fortress::Core::usize>(outputContext.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(inputContext.VirtualAddress), 0, static_cast<Fortress::Core::usize>(inputContext.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(cmdRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(cmdRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(evtRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(evtRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(erst.VirtualAddress), 0, static_cast<Fortress::Core::usize>(erst.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(ep0Ring.VirtualAddress), 0, static_cast<Fortress::Core::usize>(ep0Ring.SizeBytes));

        volatile uint64_t *dcbaaEntries = reinterpret_cast<volatile uint64_t *>(dcbaa.VirtualAddress);
        dcbaaEntries[0] = 0;

        volatile uint32_t *cmdWords = reinterpret_cast<volatile uint32_t *>(cmdRing.VirtualAddress);
        cmdWords[0] = 0;
        cmdWords[1] = 0;
        cmdWords[2] = 0;
        cmdWords[3] = (TrbTypeEnableSlot << 10u) | 1u;

        constexpr uint32_t CmdRingTrbCount = 256u;
        volatile uint32_t *linkWords = cmdWords + ((CmdRingTrbCount - 1u) * 4u);
        WriteLe64(linkWords, cmdRing.PhysicalAddress);
        linkWords[2] = 0;
        linkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

        volatile uint32_t *erstWords = reinterpret_cast<volatile uint32_t *>(erst.VirtualAddress);
        WriteLe64(erstWords, evtRing.PhysicalAddress);
        erstWords[2] = 256u;
        erstWords[3] = 0;
    }

    if (passed) {
        regs.WriteConfig(8u);
        regs.WriteDcbaap(dcbaa.PhysicalAddress & ~0x3Full);
        regs.WriteCrcr((cmdRing.PhysicalAddress & ~0x3Full) | 1ull);
        regs.WriteInterrupterIman(0, 0u);
        regs.WriteInterrupterImod(0, 0u);
        regs.WriteInterrupterErstsz(0, 1u);
        regs.WriteInterrupterErstba(0, erst.PhysicalAddress & ~0x3Full);
        regs.WriteInterrupterErdp(0, evtRing.PhysicalAddress & ~0xFull);
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdRunStop | UsbCmdInterruptEnable);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, false, MaxPolls, polls)) {
            PushLog("XHCI ADDRDEV RUN TIMEOUT");
            passed = false;
        }
    }

    uint32_t slotId = 0;
    if (passed) {
        regs.RingDoorbell(0, 0, 0);

        const uint64_t enableSlotCmdPointer = cmdRing.PhysicalAddress;
        uint32_t completionCode = 0;
        uint32_t eventIndex = 0;
        if (!WaitForCommandCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                      256u,
                                      enableSlotCmdPointer,
                                      MaxPolls,
                                      completionCode,
                                      slotId,
                                      eventIndex)) {
            PushLog("XHCI ADDRDEV ES NOEVENT");
            passed = false;
        } else if (completionCode != 1u || slotId == 0) {
            PushLog("XHCI ADDRDEV ES FAIL");
            passed = false;
        } else {
            AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
        }
    }

    if (passed) {
        volatile uint64_t *dcbaaEntries = reinterpret_cast<volatile uint64_t *>(dcbaa.VirtualAddress);
        dcbaaEntries[slotId] = outputContext.PhysicalAddress & ~0x3Full;

        const uint32_t hcc1 = regs.ReadHccParams1();
        const uint32_t contextSize = ((hcc1 & (1u << 2u)) != 0) ? 64u : 32u;
        const uint32_t portSpeed = (connectedPortSc >> 10u) & 0xFu;

        uint32_t maxPacketSize = 8u;
        if (portSpeed == 3u) {
            maxPacketSize = 64u;
        } else if (portSpeed >= 4u) {
            maxPacketSize = 512u;
        }

        volatile uint32_t *inputWords = reinterpret_cast<volatile uint32_t *>(inputContext.VirtualAddress);
        inputWords[1] = 0x3u;

        volatile uint8_t *inputBytes = reinterpret_cast<volatile uint8_t *>(inputContext.VirtualAddress);
        volatile uint32_t *slotCtx = reinterpret_cast<volatile uint32_t *>(inputBytes + contextSize);
        volatile uint32_t *ep0Ctx = reinterpret_cast<volatile uint32_t *>(inputBytes + (2u * contextSize));

        slotCtx[0] = (portSpeed << 20u) | (1u << 27u);
        slotCtx[1] = static_cast<uint32_t>(connectedPort) << 16u;

        ep0Ctx[1] = (4u << 3u) | (maxPacketSize << 16u);
        WriteLe64(ep0Ctx + 2, (ep0Ring.PhysicalAddress & ~0xFull) | 1ull);

        volatile uint32_t *cmdWords = reinterpret_cast<volatile uint32_t *>(cmdRing.VirtualAddress);
        volatile uint32_t *addrTrb = cmdWords + 4u;
        WriteLe64(addrTrb, inputContext.PhysicalAddress & ~0xFull);
        addrTrb[2] = 0;
        addrTrb[3] = (TrbTypeAddressDevice << 10u) | (1u << 9u) | (static_cast<uint32_t>(slotId) << 24u) | 1u;

        regs.RingDoorbell(0, 0, 0);

        const uint64_t addressCmdPointer = cmdRing.PhysicalAddress + 16ull;
        uint32_t completionCode = 0;
        uint32_t returnedSlotId = 0;
        uint32_t eventIndex = 0;
        if (!WaitForCommandCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                      256u,
                                      addressCmdPointer,
                                      MaxPolls,
                                      completionCode,
                                      returnedSlotId,
                                      eventIndex)) {
            PushLog("XHCI ADDRDEV AD NOEVENT");
            passed = false;
        } else if (completionCode != 1u || returnedSlotId != slotId) {
            PushLog("XHCI ADDRDEV AD FAIL");
            passed = false;
        } else {
            AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            char line[96] = {};
            size_t pos = 0;
            AppendString(line, sizeof(line), pos, "XHCI ADDRDEV SLOT ");
            AppendUInt(line, sizeof(line), pos, slotId);
            PushLog(line);
        }
    }

    {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        (void)WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls);
    }

    bool cleanupOk = true;
    if (haveEp0Ring && !FDmaMemoryManager::FreeBuffer(ep0Ring)) {
        cleanupOk = false;
    }
    if (haveErst && !FDmaMemoryManager::FreeBuffer(erst)) {
        cleanupOk = false;
    }
    if (haveEvtRing && !FDmaMemoryManager::FreeBuffer(evtRing)) {
        cleanupOk = false;
    }
    if (haveCmdRing && !FDmaMemoryManager::FreeBuffer(cmdRing)) {
        cleanupOk = false;
    }
    if (haveInputContext && !FDmaMemoryManager::FreeBuffer(inputContext)) {
        cleanupOk = false;
    }
    if (haveOutputContext && !FDmaMemoryManager::FreeBuffer(outputContext)) {
        cleanupOk = false;
    }
    if (haveDcbaa && !FDmaMemoryManager::FreeBuffer(dcbaa)) {
        cleanupOk = false;
    }
    if (!FPinnedMappingManager::UnmapPhysicalRange(mmio)) {
        cleanupOk = false;
    }

    if (!cleanupOk) {
        PushLog("XHCI ADDRDEV CLEANUP FAIL");
    }

    PushLog(passed ? "XHCI ADDRDEV PASS" : "XHCI ADDRDEV FAIL");
}

static void RunXhciGetDescriptor() {
    PushLog("XHCI GETDESC START");

    FXhciControllerInfo info{};
    if (!FXhciPciDiscovery::DiscoverFirst(info) || !info.Found) {
        PushLog("XHCI GETDESC NONE");
        return;
    }

    FPinnedMapping mmio{};
    if (!FPinnedMappingManager::MapPhysicalRange(info.MmioBase,
                                                 info.MmioSize,
                                                 FVirtualMemoryManager::FlagsDeviceRWUCNX,
                                                 mmio)) {
        PushLog("XHCI GETDESC MAP FAIL");
        return;
    }

    FXhciMmioRegisters regs;
    if (!regs.Initialize(mmio.VirtualAddress)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog("XHCI GETDESC REGS FAIL");
        return;
    }

    const uint64_t mappedVirtualBase = mmio.VirtualAddress - (info.MmioBase - (info.MmioBase & ~(FVirtualMemoryManager::PageSize - 1ull)));
    const uint64_t mappedSizeBytes = mmio.PageCount * FVirtualMemoryManager::PageSize;
    if (!IsXhciRegisterWindowSafe(regs, mappedVirtualBase, mappedSizeBytes)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog("XHCI GETDESC MMIO WINDOW FAIL");
        return;
    }

    constexpr uint32_t UsbCmdRunStop = 1u << 0;
    constexpr uint32_t UsbCmdHostControllerReset = 1u << 1;
    constexpr uint32_t UsbCmdInterruptEnable = 1u << 2;
    constexpr uint32_t UsbStsHostControllerHalted = 1u << 0;
    constexpr uint32_t UsbStsControllerNotReady = 1u << 11;
    constexpr uint64_t MaxPolls = 3000000ull;

    constexpr uint32_t TrbTypeLink = 6u;
    constexpr uint32_t TrbTypeEnableSlot = 9u;
    constexpr uint32_t TrbTypeAddressDevice = 11u;
    constexpr uint32_t TrbTypeSetupStage = 2u;
    constexpr uint32_t TrbTypeDataStage = 3u;
    constexpr uint32_t TrbTypeStatusStage = 4u;

    constexpr uint32_t CcSuccess = 1u;
    constexpr uint32_t EndpointIdControl = 1u;

    FDmaBuffer dcbaa{};
    FDmaBuffer outputContext{};
    FDmaBuffer inputContext{};
    FDmaBuffer cmdRing{};
    FDmaBuffer evtRing{};
    FDmaBuffer erst{};
    FDmaBuffer ep0Ring{};
    FDmaBuffer descriptorBuffer{};

    bool haveDcbaa = false;
    bool haveOutputContext = false;
    bool haveInputContext = false;
    bool haveCmdRing = false;
    bool haveEvtRing = false;
    bool haveErst = false;
    bool haveEp0Ring = false;
    bool haveDescriptorBuffer = false;

    bool passed = true;
    uint64_t polls = 0;

    if (!WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog("XHCI GETDESC CNR TIMEOUT");
        passed = false;
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls)) {
            PushLog("XHCI GETDESC HALT TIMEOUT");
            passed = false;
        }
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdHostControllerReset);
        if (!WaitForUsbCmdBit(regs, UsbCmdHostControllerReset, false, MaxPolls, polls)) {
            PushLog("XHCI GETDESC RESET TIMEOUT");
            passed = false;
        }
    }

    if (passed && !WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog("XHCI GETDESC READY TIMEOUT");
        passed = false;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, dcbaa)) {
        PushLog("XHCI GETDESC DCBAA FAIL");
        passed = false;
    } else if (passed) {
        haveDcbaa = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, outputContext)) {
        PushLog("XHCI GETDESC OUTCTX FAIL");
        passed = false;
    } else if (passed) {
        haveOutputContext = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, inputContext)) {
        PushLog("XHCI GETDESC INCTX FAIL");
        passed = false;
    } else if (passed) {
        haveInputContext = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, cmdRing)) {
        PushLog("XHCI GETDESC CR FAIL");
        passed = false;
    } else if (passed) {
        haveCmdRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, evtRing)) {
        PushLog("XHCI GETDESC ER FAIL");
        passed = false;
    } else if (passed) {
        haveEvtRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, erst)) {
        PushLog("XHCI GETDESC ERST FAIL");
        passed = false;
    } else if (passed) {
        haveErst = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, ep0Ring)) {
        PushLog("XHCI GETDESC EP0RING FAIL");
        passed = false;
    } else if (passed) {
        haveEp0Ring = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, descriptorBuffer)) {
        PushLog("XHCI GETDESC DESCBUF FAIL");
        passed = false;
    } else if (passed) {
        haveDescriptorBuffer = true;
    }

    uint8_t connectedPort = 0;
    uint32_t connectedPortSc = 0;
    if (passed) {
        const uint32_t hcs1 = regs.ReadHcsParams1();
        const uint32_t maxPorts = (hcs1 >> 24u) & 0xFFu;
        for (uint8_t port = 1; port <= maxPorts; port++) {
            const uint32_t portSc = regs.ReadPortSc(port);
            if ((portSc & 0x1u) != 0) {
                connectedPort = port;
                connectedPortSc = portSc;
                break;
            }
        }

        if (connectedPort == 0) {
            PushLog("XHCI GETDESC NOPORT");
            passed = false;
        }
    }

    uint32_t slotId = 0;
    if (passed) {
        Fortress::Runtime::Memset(reinterpret_cast<void *>(dcbaa.VirtualAddress), 0, static_cast<Fortress::Core::usize>(dcbaa.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(outputContext.VirtualAddress), 0, static_cast<Fortress::Core::usize>(outputContext.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(inputContext.VirtualAddress), 0, static_cast<Fortress::Core::usize>(inputContext.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(cmdRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(cmdRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(evtRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(evtRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(erst.VirtualAddress), 0, static_cast<Fortress::Core::usize>(erst.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(ep0Ring.VirtualAddress), 0, static_cast<Fortress::Core::usize>(ep0Ring.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(descriptorBuffer.VirtualAddress), 0, static_cast<Fortress::Core::usize>(descriptorBuffer.SizeBytes));

        volatile uint64_t *dcbaaEntries = reinterpret_cast<volatile uint64_t *>(dcbaa.VirtualAddress);
        dcbaaEntries[0] = 0;

        volatile uint32_t *cmdWords = reinterpret_cast<volatile uint32_t *>(cmdRing.VirtualAddress);
        cmdWords[0] = 0;
        cmdWords[1] = 0;
        cmdWords[2] = 0;
        cmdWords[3] = (TrbTypeEnableSlot << 10u) | 1u;

        constexpr uint32_t CmdRingTrbCount = 256u;
        volatile uint32_t *cmdLinkWords = cmdWords + ((CmdRingTrbCount - 1u) * 4u);
        WriteLe64(cmdLinkWords, cmdRing.PhysicalAddress);
        cmdLinkWords[2] = 0;
        cmdLinkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

        volatile uint32_t *erstWords = reinterpret_cast<volatile uint32_t *>(erst.VirtualAddress);
        WriteLe64(erstWords, evtRing.PhysicalAddress);
        erstWords[2] = 256u;
        erstWords[3] = 0;

        regs.WriteConfig(8u);
        regs.WriteDcbaap(dcbaa.PhysicalAddress & ~0x3Full);
        regs.WriteCrcr((cmdRing.PhysicalAddress & ~0x3Full) | 1ull);
        regs.WriteInterrupterIman(0, 0u);
        regs.WriteInterrupterImod(0, 0u);
        regs.WriteInterrupterErstsz(0, 1u);
        regs.WriteInterrupterErstba(0, erst.PhysicalAddress & ~0x3Full);
        regs.WriteInterrupterErdp(0, evtRing.PhysicalAddress & ~0xFull);

        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdRunStop | UsbCmdInterruptEnable);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, false, MaxPolls, polls)) {
            PushLog("XHCI GETDESC RUN TIMEOUT");
            passed = false;
        }

        if (passed) {
            regs.RingDoorbell(0, 0, 0);
            uint32_t completionCode = 0;
            uint32_t eventIndex = 0;
            if (!WaitForCommandCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                          256u,
                                          cmdRing.PhysicalAddress,
                                          MaxPolls,
                                          completionCode,
                                          slotId,
                                          eventIndex)) {
                PushLog("XHCI GETDESC ES NOEVENT");
                passed = false;
            } else if (completionCode != CcSuccess || slotId == 0) {
                PushLog("XHCI GETDESC ES FAIL");
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }
        }

        if (passed) {
            dcbaaEntries[slotId] = outputContext.PhysicalAddress & ~0x3Full;

            const uint32_t hcc1 = regs.ReadHccParams1();
            const uint32_t contextSize = ((hcc1 & (1u << 2u)) != 0) ? 64u : 32u;
            const uint32_t portSpeed = (connectedPortSc >> 10u) & 0xFu;

            uint32_t maxPacketSize = 64u;
            if (portSpeed >= 4u) {
                maxPacketSize = 512u;
            }

            char speedLine[96] = {};
            size_t speedPos = 0;
            AppendString(speedLine, sizeof(speedLine), speedPos, "XHCI GETCFG PORTSPD ");
            AppendUInt(speedLine, sizeof(speedLine), speedPos, portSpeed);
            AppendString(speedLine, sizeof(speedLine), speedPos, " MPS0 ");
            AppendUInt(speedLine, sizeof(speedLine), speedPos, maxPacketSize);
            PushLog(speedLine);

            volatile uint32_t *inputWords = reinterpret_cast<volatile uint32_t *>(inputContext.VirtualAddress);
            inputWords[1] = 0x3u;

            volatile uint8_t *inputBytes = reinterpret_cast<volatile uint8_t *>(inputContext.VirtualAddress);
            volatile uint32_t *slotCtx = reinterpret_cast<volatile uint32_t *>(inputBytes + contextSize);
            volatile uint32_t *ep0Ctx = reinterpret_cast<volatile uint32_t *>(inputBytes + (2u * contextSize));

            slotCtx[0] = (portSpeed << 20u) | (1u << 27u);
            slotCtx[1] = static_cast<uint32_t>(connectedPort) << 16u;

            ep0Ctx[1] = 3u | (4u << 3u) | (maxPacketSize << 16u);
            WriteLe64(ep0Ctx + 2, (ep0Ring.PhysicalAddress & ~0xFull) | 1ull);

            volatile uint32_t *addrTrb = cmdWords + 4u;
            WriteLe64(addrTrb, inputContext.PhysicalAddress & ~0xFull);
            addrTrb[2] = 0;
            addrTrb[3] = (TrbTypeAddressDevice << 10u) | (static_cast<uint32_t>(slotId) << 24u) | 1u;

            regs.RingDoorbell(0, 0, 0);

            uint32_t completionCode = 0;
            uint32_t returnedSlotId = 0;
            uint32_t eventIndex = 0;
            if (!WaitForCommandCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                          256u,
                                          cmdRing.PhysicalAddress + 16ull,
                                          MaxPolls,
                                          completionCode,
                                          returnedSlotId,
                                          eventIndex)) {
                PushLog("XHCI GETDESC AD NOEVENT");
                passed = false;
            } else if (completionCode != CcSuccess || returnedSlotId != slotId) {
                PushLog("XHCI GETDESC AD FAIL");
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }
        }

        if (passed) {
            volatile uint32_t *ep0Words = reinterpret_cast<volatile uint32_t *>(ep0Ring.VirtualAddress);
            // Setup Stage TRB embeds USB setup packet bytes directly in the TRB payload.
            ep0Words[0] = 0x01000680u; // bmRequestType=0x80, bRequest=0x06, wValue=0x0100
            ep0Words[1] = 0x00120000u; // wIndex=0x0000, wLength=18
            ep0Words[2] = 8u;
            ep0Words[3] = (TrbTypeSetupStage << 10u) | (1u << 6u) | (1u << 4u) | (3u << 16u) | 1u;

            ep0Words[4] = static_cast<uint32_t>(descriptorBuffer.PhysicalAddress & 0xFFFFFFFFull);
            ep0Words[5] = static_cast<uint32_t>((descriptorBuffer.PhysicalAddress >> 32u) & 0xFFFFFFFFull);
            ep0Words[6] = 18u;
            ep0Words[7] = (TrbTypeDataStage << 10u) | (1u << 4u) | (1u << 16u) | 1u;

            ep0Words[8] = 0;
            ep0Words[9] = 0;
            ep0Words[10] = 0;
            ep0Words[11] = (TrbTypeStatusStage << 10u) | (1u << 5u) | 1u;

            constexpr uint32_t Ep0TrbCount = 256u;
            volatile uint32_t *ep0LinkWords = ep0Words + ((Ep0TrbCount - 1u) * 4u);
            WriteLe64(ep0LinkWords, ep0Ring.PhysicalAddress);
            ep0LinkWords[2] = 0;
            ep0LinkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

            regs.RingDoorbell(static_cast<uint8_t>(slotId), 1u, 0u);

            uint32_t completionCode = 0;
            uint32_t endpointId = 0;
            uint32_t eventIndex = 0;
            if (!WaitForTransferCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                           256u,
                                           ep0Ring.PhysicalAddress,
                                           ep0Ring.PhysicalAddress + 16ull,
                                           ep0Ring.PhysicalAddress + 32ull,
                                           slotId,
                                           EndpointIdControl,
                                           MaxPolls,
                                           completionCode,
                                           endpointId,
                                           eventIndex)) {
                PushLog("XHCI GETDESC TD NOEVENT");
                LogEventRingDebug(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress), 256u, 8u);
                passed = false;
            } else if (completionCode != CcSuccess || endpointId != EndpointIdControl) {
                PushLog("XHCI GETDESC TD FAIL");
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }
        }

        if (passed) {
            volatile uint8_t *desc = reinterpret_cast<volatile uint8_t *>(descriptorBuffer.VirtualAddress);
            const uint8_t length = desc[0];
            const uint8_t dtype = desc[1];
            if (length < 18u || dtype != 1u) {
                PushLog("XHCI GETDESC DATA INVALID");
                passed = false;
            } else {
                const uint16_t usbBcd = static_cast<uint16_t>(desc[2] | (static_cast<uint16_t>(desc[3]) << 8u));
                const uint16_t vid = static_cast<uint16_t>(desc[8] | (static_cast<uint16_t>(desc[9]) << 8u));
                const uint16_t pid = static_cast<uint16_t>(desc[10] | (static_cast<uint16_t>(desc[11]) << 8u));
                const uint8_t bDeviceClass = desc[4];
                const uint8_t bDeviceSubClass = desc[5];
                const uint8_t bDeviceProtocol = desc[6];
                const uint8_t bMaxPacketSize0 = desc[7];

                char line[96] = {};
                size_t pos = 0;
                AppendString(line, sizeof(line), pos, "USB VID ");
                AppendHex(line, sizeof(line), pos, vid);
                AppendString(line, sizeof(line), pos, " PID ");
                AppendHex(line, sizeof(line), pos, pid);
                PushLog(line);

                pos = 0;
                line[0] = '\0';
                AppendString(line, sizeof(line), pos, "USB BCD ");
                AppendHex(line, sizeof(line), pos, usbBcd);
                AppendString(line, sizeof(line), pos, " MPS0 ");
                AppendUInt(line, sizeof(line), pos, bMaxPacketSize0);
                PushLog(line);

                pos = 0;
                line[0] = '\0';
                AppendString(line, sizeof(line), pos, "USB CLS ");
                AppendUInt(line, sizeof(line), pos, bDeviceClass);
                AppendString(line, sizeof(line), pos, " SUB ");
                AppendUInt(line, sizeof(line), pos, bDeviceSubClass);
                AppendString(line, sizeof(line), pos, " PROTO ");
                AppendUInt(line, sizeof(line), pos, bDeviceProtocol);
                PushLog(line);
            }
        }
    }

    {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        (void)WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls);
    }

    bool cleanupOk = true;
    if (haveDescriptorBuffer && !FDmaMemoryManager::FreeBuffer(descriptorBuffer)) {
        cleanupOk = false;
    }
    if (haveEp0Ring && !FDmaMemoryManager::FreeBuffer(ep0Ring)) {
        cleanupOk = false;
    }
    if (haveErst && !FDmaMemoryManager::FreeBuffer(erst)) {
        cleanupOk = false;
    }
    if (haveEvtRing && !FDmaMemoryManager::FreeBuffer(evtRing)) {
        cleanupOk = false;
    }
    if (haveCmdRing && !FDmaMemoryManager::FreeBuffer(cmdRing)) {
        cleanupOk = false;
    }
    if (haveInputContext && !FDmaMemoryManager::FreeBuffer(inputContext)) {
        cleanupOk = false;
    }
    if (haveOutputContext && !FDmaMemoryManager::FreeBuffer(outputContext)) {
        cleanupOk = false;
    }
    if (haveDcbaa && !FDmaMemoryManager::FreeBuffer(dcbaa)) {
        cleanupOk = false;
    }
    if (!FPinnedMappingManager::UnmapPhysicalRange(mmio)) {
        cleanupOk = false;
    }

    if (!cleanupOk) {
        PushLog("XHCI GETDESC CLEANUP FAIL");
    }

    PushLog(passed ? "XHCI GETDESC PASS" : "XHCI GETDESC FAIL");
}

static void RunXhciGetConfigDescriptor() {
    PushLog("XHCI GETCFG START V5");

    FXhciControllerInfo info{};
    if (!FXhciPciDiscovery::DiscoverFirst(info) || !info.Found) {
        PushLog("XHCI GETCFG NONE");
        return;
    }

    FPinnedMapping mmio{};
    if (!FPinnedMappingManager::MapPhysicalRange(info.MmioBase,
                                                 info.MmioSize,
                                                 FVirtualMemoryManager::FlagsDeviceRWUCNX,
                                                 mmio)) {
        PushLog("XHCI GETCFG MAP FAIL");
        return;
    }

    FXhciMmioRegisters regs;
    if (!regs.Initialize(mmio.VirtualAddress)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog("XHCI GETCFG REGS FAIL");
        return;
    }

    const uint64_t mappedVirtualBase = mmio.VirtualAddress - (info.MmioBase - (info.MmioBase & ~(FVirtualMemoryManager::PageSize - 1ull)));
    const uint64_t mappedSizeBytes = mmio.PageCount * FVirtualMemoryManager::PageSize;
    if (!IsXhciRegisterWindowSafe(regs, mappedVirtualBase, mappedSizeBytes)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog("XHCI GETCFG MMIO WINDOW FAIL");
        return;
    }

    constexpr uint32_t UsbCmdRunStop = 1u << 0;
    constexpr uint32_t UsbCmdHostControllerReset = 1u << 1;
    constexpr uint32_t UsbCmdInterruptEnable = 1u << 2;
    constexpr uint32_t UsbStsHostControllerHalted = 1u << 0;
    constexpr uint32_t UsbStsControllerNotReady = 1u << 11;
    constexpr uint64_t MaxPolls = 200000ull;

    constexpr uint32_t TrbTypeLink = 6u;
    constexpr uint32_t TrbTypeEnableSlot = 9u;
    constexpr uint32_t TrbTypeAddressDevice = 11u;
    constexpr uint32_t TrbTypeSetupStage = 2u;
    constexpr uint32_t TrbTypeDataStage = 3u;
    constexpr uint32_t TrbTypeStatusStage = 4u;

    constexpr uint32_t CcSuccess = 1u;
    constexpr uint32_t CcShortPacket = 13u;
    constexpr uint32_t EndpointIdControl = 1u;
    constexpr uint32_t InitialConfigBytes = 9u;
    constexpr uint32_t MaxConfigBytes = 512u;

    FDmaBuffer dcbaa{};
    FDmaBuffer outputContext{};
    FDmaBuffer inputContext{};
    FDmaBuffer cmdRing{};
    FDmaBuffer evtRing{};
    FDmaBuffer erst{};
    FDmaBuffer ep0Ring{};
    FDmaBuffer descriptorBuffer{};

    bool haveDcbaa = false;
    bool haveOutputContext = false;
    bool haveInputContext = false;
    bool haveCmdRing = false;
    bool haveEvtRing = false;
    bool haveErst = false;
    bool haveEp0Ring = false;
    bool haveDescriptorBuffer = false;

    bool passed = true;
    const char *failureStage = "NONE";
    uint64_t polls = 0;

    if (!WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog("XHCI GETCFG CNR TIMEOUT");
        failureStage = "CNR";
        passed = false;
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls)) {
            PushLog("XHCI GETCFG HALT TIMEOUT");
            failureStage = "HALT";
            passed = false;
        }
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdHostControllerReset);
        if (!WaitForUsbCmdBit(regs, UsbCmdHostControllerReset, false, MaxPolls, polls)) {
            PushLog("XHCI GETCFG RESET TIMEOUT");
            failureStage = "RESET";
            passed = false;
        }
    }

    if (passed && !WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog("XHCI GETCFG READY TIMEOUT");
        failureStage = "READY";
        passed = false;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, dcbaa)) {
        PushLog("XHCI GETCFG DCBAA FAIL");
        failureStage = "DCBAA";
        passed = false;
    } else if (passed) {
        haveDcbaa = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, outputContext)) {
        PushLog("XHCI GETCFG OUTCTX FAIL");
        failureStage = "OUTCTX";
        passed = false;
    } else if (passed) {
        haveOutputContext = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, inputContext)) {
        PushLog("XHCI GETCFG INCTX FAIL");
        failureStage = "INCTX";
        passed = false;
    } else if (passed) {
        haveInputContext = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, cmdRing)) {
        PushLog("XHCI GETCFG CR FAIL");
        failureStage = "CMDRING";
        passed = false;
    } else if (passed) {
        haveCmdRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, evtRing)) {
        PushLog("XHCI GETCFG ER FAIL");
        failureStage = "EVTRING";
        passed = false;
    } else if (passed) {
        haveEvtRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, erst)) {
        PushLog("XHCI GETCFG ERST FAIL");
        failureStage = "ERST";
        passed = false;
    } else if (passed) {
        haveErst = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, ep0Ring)) {
        PushLog("XHCI GETCFG EP0RING FAIL");
        failureStage = "EP0RING";
        passed = false;
    } else if (passed) {
        haveEp0Ring = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, descriptorBuffer)) {
        PushLog("XHCI GETCFG DESCBUF FAIL");
        failureStage = "DESCBUF";
        passed = false;
    } else if (passed) {
        haveDescriptorBuffer = true;
    }

    uint8_t connectedPort = 0;
    uint32_t connectedPortSc = 0;
    if (passed) {
        const uint32_t hcs1 = regs.ReadHcsParams1();
        const uint32_t maxPorts = (hcs1 >> 24u) & 0xFFu;
        for (uint8_t port = 1; port <= maxPorts; port++) {
            const uint32_t portSc = regs.ReadPortSc(port);
            if ((portSc & 0x1u) != 0) {
                connectedPort = port;
                connectedPortSc = portSc;
                break;
            }
        }

        if (connectedPort == 0) {
            PushLog("XHCI GETCFG NOPORT");
            failureStage = "NOPORT";
            passed = false;
        }
    }

    uint32_t slotId = 0;
    if (passed) {
        Fortress::Runtime::Memset(reinterpret_cast<void *>(dcbaa.VirtualAddress), 0, static_cast<Fortress::Core::usize>(dcbaa.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(outputContext.VirtualAddress), 0, static_cast<Fortress::Core::usize>(outputContext.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(inputContext.VirtualAddress), 0, static_cast<Fortress::Core::usize>(inputContext.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(cmdRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(cmdRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(evtRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(evtRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(erst.VirtualAddress), 0, static_cast<Fortress::Core::usize>(erst.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(ep0Ring.VirtualAddress), 0, static_cast<Fortress::Core::usize>(ep0Ring.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(descriptorBuffer.VirtualAddress), 0, static_cast<Fortress::Core::usize>(descriptorBuffer.SizeBytes));

        volatile uint64_t *dcbaaEntries = reinterpret_cast<volatile uint64_t *>(dcbaa.VirtualAddress);
        dcbaaEntries[0] = 0;

        volatile uint32_t *cmdWords = reinterpret_cast<volatile uint32_t *>(cmdRing.VirtualAddress);
        cmdWords[0] = 0;
        cmdWords[1] = 0;
        cmdWords[2] = 0;
        cmdWords[3] = (TrbTypeEnableSlot << 10u) | 1u;

        constexpr uint32_t CmdRingTrbCount = 256u;
        volatile uint32_t *cmdLinkWords = cmdWords + ((CmdRingTrbCount - 1u) * 4u);
        WriteLe64(cmdLinkWords, cmdRing.PhysicalAddress);
        cmdLinkWords[2] = 0;
        cmdLinkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

        volatile uint32_t *erstWords = reinterpret_cast<volatile uint32_t *>(erst.VirtualAddress);
        WriteLe64(erstWords, evtRing.PhysicalAddress);
        erstWords[2] = 256u;
        erstWords[3] = 0;

        regs.WriteConfig(8u);
        regs.WriteDcbaap(dcbaa.PhysicalAddress & ~0x3Full);
        regs.WriteCrcr((cmdRing.PhysicalAddress & ~0x3Full) | 1ull);
        regs.WriteInterrupterIman(0, 0u);
        regs.WriteInterrupterImod(0, 0u);
        regs.WriteInterrupterErstsz(0, 1u);
        regs.WriteInterrupterErstba(0, erst.PhysicalAddress & ~0x3Full);
        regs.WriteInterrupterErdp(0, evtRing.PhysicalAddress & ~0xFull);

        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdRunStop | UsbCmdInterruptEnable);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, false, MaxPolls, polls)) {
            PushLog("XHCI GETCFG RUN TIMEOUT");
            failureStage = "RUN";
            passed = false;
        }

        if (passed) {
            regs.RingDoorbell(0, 0, 0);
            uint32_t completionCode = 0;
            uint32_t eventIndex = 0;
            if (!WaitForCommandCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                          256u,
                                          cmdRing.PhysicalAddress,
                                          MaxPolls,
                                          completionCode,
                                          slotId,
                                          eventIndex)) {
                PushLog("XHCI GETCFG ES NOEVENT");
                failureStage = "ENABLE_SLOT_NOEVENT";
                passed = false;
            } else if (completionCode != CcSuccess || slotId == 0) {
                PushLog("XHCI GETCFG ES FAIL");
                failureStage = "ENABLE_SLOT_FAIL";
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }
        }

        if (passed) {
            dcbaaEntries[slotId] = outputContext.PhysicalAddress & ~0x3Full;

            const uint32_t hcc1 = regs.ReadHccParams1();
            const uint32_t contextSize = ((hcc1 & (1u << 2u)) != 0) ? 64u : 32u;
            const uint32_t portSpeed = (connectedPortSc >> 10u) & 0xFu;

            uint32_t maxPacketSize = 8u;
            if (portSpeed == 3u) {
                maxPacketSize = 64u;
            } else if (portSpeed >= 4u) {
                maxPacketSize = 512u;
            }

            volatile uint32_t *inputWords = reinterpret_cast<volatile uint32_t *>(inputContext.VirtualAddress);
            inputWords[1] = 0x3u;

            volatile uint8_t *inputBytes = reinterpret_cast<volatile uint8_t *>(inputContext.VirtualAddress);
            volatile uint32_t *slotCtx = reinterpret_cast<volatile uint32_t *>(inputBytes + contextSize);
            volatile uint32_t *ep0Ctx = reinterpret_cast<volatile uint32_t *>(inputBytes + (2u * contextSize));

            slotCtx[0] = (portSpeed << 20u) | (1u << 27u);
            slotCtx[1] = static_cast<uint32_t>(connectedPort) << 16u;

            ep0Ctx[1] = 3u | (4u << 3u) | (maxPacketSize << 16u);
            WriteLe64(ep0Ctx + 2, (ep0Ring.PhysicalAddress & ~0xFull) | 1ull);

            volatile uint32_t *addrTrb = cmdWords + 4u;
            WriteLe64(addrTrb, inputContext.PhysicalAddress & ~0xFull);
            addrTrb[2] = 0;
            addrTrb[3] = (TrbTypeAddressDevice << 10u) | (static_cast<uint32_t>(slotId) << 24u) | 1u;

            regs.RingDoorbell(0, 0, 0);

            uint32_t completionCode = 0;
            uint32_t returnedSlotId = 0;
            uint32_t eventIndex = 0;
            if (!WaitForCommandCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                          256u,
                                          cmdRing.PhysicalAddress + 16ull,
                                          MaxPolls,
                                          completionCode,
                                          returnedSlotId,
                                          eventIndex)) {
                PushLog("XHCI GETCFG AD NOEVENT");
                failureStage = "ADDRDEV_NOEVENT";
                passed = false;
            } else if (completionCode != CcSuccess || returnedSlotId != slotId) {
                PushLog("XHCI GETCFG AD FAIL");
                failureStage = "ADDRDEV_FAIL";
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }
        }

        if (passed) {
            volatile uint32_t *ep0Words = reinterpret_cast<volatile uint32_t *>(ep0Ring.VirtualAddress);

            ep0Words[0] = 0x02000680u; // bmRequestType=0x80, bRequest=0x06, wValue=0x0200
            ep0Words[1] = ((InitialConfigBytes & 0xFFFFu) << 16u); // wIndex=0x0000, wLength=header
            ep0Words[2] = 8u;
            ep0Words[3] = (TrbTypeSetupStage << 10u) | (1u << 6u) | (1u << 4u) | (3u << 16u) | 1u;

            ep0Words[4] = static_cast<uint32_t>(descriptorBuffer.PhysicalAddress & 0xFFFFFFFFull);
            ep0Words[5] = static_cast<uint32_t>((descriptorBuffer.PhysicalAddress >> 32u) & 0xFFFFFFFFull);
            ep0Words[6] = InitialConfigBytes;
            ep0Words[7] = (TrbTypeDataStage << 10u) | (1u << 4u) | (1u << 16u) | 1u;

            ep0Words[8] = 0;
            ep0Words[9] = 0;
            ep0Words[10] = 0;
            ep0Words[11] = (TrbTypeStatusStage << 10u) | (1u << 5u) | 1u;

            constexpr uint32_t Ep0TrbCount = 256u;
            volatile uint32_t *ep0LinkWords = ep0Words + ((Ep0TrbCount - 1u) * 4u);
            WriteLe64(ep0LinkWords, ep0Ring.PhysicalAddress);
            ep0LinkWords[2] = 0;
            ep0LinkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

            regs.RingDoorbell(static_cast<uint8_t>(slotId), 1u, 0u);

            uint32_t completionCode = 0;
            uint32_t endpointId = 0;
            uint32_t eventIndex = 0;
            bool transferEventReceived = WaitForTransferCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                                                   256u,
                                                                   ep0Ring.PhysicalAddress,
                                                                   ep0Ring.PhysicalAddress + 16ull,
                                                                   ep0Ring.PhysicalAddress + 32ull,
                                                                   slotId,
                                                                   EndpointIdControl,
                                                                   MaxPolls,
                                                                   completionCode,
                                                                   endpointId,
                                                                   eventIndex);
            bool transferRecovered = false;

            if (!transferEventReceived) {
                for (uint32_t i = 0u; i < 16u; i++) {
                    uint32_t drainedType = 0u;
                    uint32_t drainedSlot = 0u;
                    uint32_t drainedEndpoint = 0u;
                    uint32_t drainedCc = 0u;
                    if (!DrainOneEventTrb(regs,
                                          reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                          256u,
                                          evtRing.PhysicalAddress,
                                          drainedType,
                                          drainedSlot,
                                          drainedEndpoint,
                                          drainedCc)) {
                        break;
                    }

                    char drainLine[128] = {};
                    size_t drainPos = 0;
                    AppendString(drainLine, sizeof(drainLine), drainPos, "XHCI GETCFG DRAIN EVT T ");
                    AppendUInt(drainLine, sizeof(drainLine), drainPos, drainedType);
                    AppendString(drainLine, sizeof(drainLine), drainPos, " S ");
                    AppendUInt(drainLine, sizeof(drainLine), drainPos, drainedSlot);
                    AppendString(drainLine, sizeof(drainLine), drainPos, " EP ");
                    AppendUInt(drainLine, sizeof(drainLine), drainPos, drainedEndpoint);
                    AppendString(drainLine, sizeof(drainLine), drainPos, " CC ");
                    AppendUInt(drainLine, sizeof(drainLine), drainPos, drainedCc);
                    PushLog(drainLine);

                    if (drainedType == 32u && drainedSlot == slotId && drainedEndpoint == EndpointIdControl &&
                        (drainedCc == CcSuccess || drainedCc == CcShortPacket)) {
                        transferEventReceived = true;
                        transferRecovered = true;
                        completionCode = drainedCc;
                        endpointId = drainedEndpoint;
                        break;
                    }
                }
            }

            if (!transferEventReceived && !transferRecovered) {
                PushLog("XHCI GETCFG RETRY9 START");
                Fortress::Runtime::Memset(reinterpret_cast<void *>(descriptorBuffer.VirtualAddress),
                                          0,
                                          static_cast<Fortress::Core::usize>(descriptorBuffer.SizeBytes));

                volatile uint32_t *retry9Ep0Words = reinterpret_cast<volatile uint32_t *>(ep0Ring.VirtualAddress);
                constexpr uint32_t Retry9Bytes = 9u;
                retry9Ep0Words[0] = 0x02000680u;
                retry9Ep0Words[1] = ((Retry9Bytes & 0xFFFFu) << 16u);
                retry9Ep0Words[2] = 8u;
                retry9Ep0Words[3] = (TrbTypeSetupStage << 10u) | (1u << 6u) | (1u << 4u) | (3u << 16u) | 1u;
                retry9Ep0Words[4] = static_cast<uint32_t>(descriptorBuffer.PhysicalAddress & 0xFFFFFFFFull);
                retry9Ep0Words[5] = static_cast<uint32_t>((descriptorBuffer.PhysicalAddress >> 32u) & 0xFFFFFFFFull);
                retry9Ep0Words[6] = Retry9Bytes;
                retry9Ep0Words[7] = (TrbTypeDataStage << 10u) | (1u << 4u) | (1u << 16u) | 1u;
                retry9Ep0Words[8] = 0u;
                retry9Ep0Words[9] = 0u;
                retry9Ep0Words[10] = 0u;
                retry9Ep0Words[11] = (TrbTypeStatusStage << 10u) | (1u << 5u) | 1u;
                regs.RingDoorbell(static_cast<uint8_t>(slotId), 1u, 0u);

                uint32_t retry9Cc = 0u;
                uint32_t retry9EndpointId = 0u;
                uint32_t retry9EventIndex = 0u;
                if (WaitForTransferCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                              256u,
                                              ep0Ring.PhysicalAddress,
                                              ep0Ring.PhysicalAddress + 16ull,
                                              ep0Ring.PhysicalAddress + 32ull,
                                              slotId,
                                              EndpointIdControl,
                                              MaxPolls / 4ull,
                                              retry9Cc,
                                              retry9EndpointId,
                                              retry9EventIndex)) {
                    AckConsumedEvent(regs, evtRing.PhysicalAddress, retry9EventIndex);
                    if ((retry9Cc == CcSuccess || retry9Cc == CcShortPacket) && retry9EndpointId == EndpointIdControl) {
                        transferRecovered = true;
                        PushLog("XHCI GETCFG RETRY9 EVENT");
                    }
                }
            }

            if (!transferEventReceived && !transferRecovered) {
                bool dataLooksValid = false;
                for (uint64_t spin = 0ull; spin < (MaxPolls / 4ull); spin++) {
                    const volatile uint8_t *descProbe = reinterpret_cast<volatile uint8_t *>(descriptorBuffer.VirtualAddress);
                    const uint8_t len = descProbe[0];
                    const uint8_t dtype = descProbe[1];
                    const uint16_t totalLength = static_cast<uint16_t>(descProbe[2] | (static_cast<uint16_t>(descProbe[3]) << 8u));
                    if (len >= 9u && dtype == 2u && totalLength >= 9u) {
                        dataLooksValid = true;
                        break;
                    }
                    CpuPause();
                    YieldLongOperationPoll(spin + 1ull);
                }

                if (dataLooksValid) {
                    transferRecovered = true;
                    PushLog("XHCI GETCFG NOEVENT DATA");
                }
            }

            if (!transferEventReceived && !transferRecovered) {
                PushLog("XHCI GETCFG TD NOEVENT");
                failureStage = "GETCFG_TRANSFER_NOEVENT";
                LogEventRingDebug(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress), 256u, 8u);
                passed = false;
            } else if (transferEventReceived &&
                       ((completionCode != CcSuccess && completionCode != CcShortPacket) || endpointId != EndpointIdControl)) {
                PushLog("XHCI GETCFG TD FAIL");
                failureStage = "GETCFG_TRANSFER_FAIL";
                passed = false;
            } else if (transferEventReceived) {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }

            if (transferRecovered) {
                PushLog("XHCI GETCFG TD RECOVERED");
            }
        }

        if (passed) {
            volatile uint8_t *desc = reinterpret_cast<volatile uint8_t *>(descriptorBuffer.VirtualAddress);
            const uint8_t length = desc[0];
            const uint8_t dtype = desc[1];
            if (length < 9u || dtype != 2u) {
                PushLog("XHCI GETCFG DATA INVALID");
                failureStage = "DATA_INVALID";
                passed = false;
            } else {
                uint16_t totalLength = static_cast<uint16_t>(desc[2] | (static_cast<uint16_t>(desc[3]) << 8u));
                const uint8_t interfaceCountHeader = desc[4];
                const uint8_t configValue = desc[5];
                const uint8_t attributes = desc[7];
                const uint8_t maxPowerUnits = desc[8];

                uint32_t requestedFullLength = totalLength;
                if (requestedFullLength < 9u) {
                    requestedFullLength = 9u;
                }
                if (requestedFullLength > MaxConfigBytes) {
                    requestedFullLength = MaxConfigBytes;
                }

                if (requestedFullLength > InitialConfigBytes) {
                    Fortress::Runtime::Memset(reinterpret_cast<void *>(descriptorBuffer.VirtualAddress),
                                              0,
                                              static_cast<Fortress::Core::usize>(descriptorBuffer.SizeBytes));

                    volatile uint32_t *ep0Words = reinterpret_cast<volatile uint32_t *>(ep0Ring.VirtualAddress);
                    ep0Words[0] = 0x02000680u;
                    ep0Words[1] = ((requestedFullLength & 0xFFFFu) << 16u);
                    ep0Words[2] = 8u;
                    ep0Words[3] = (TrbTypeSetupStage << 10u) | (1u << 6u) | (1u << 4u) | (3u << 16u) | 1u;

                    ep0Words[4] = static_cast<uint32_t>(descriptorBuffer.PhysicalAddress & 0xFFFFFFFFull);
                    ep0Words[5] = static_cast<uint32_t>((descriptorBuffer.PhysicalAddress >> 32u) & 0xFFFFFFFFull);
                    ep0Words[6] = requestedFullLength;
                    ep0Words[7] = (TrbTypeDataStage << 10u) | (1u << 4u) | (1u << 16u) | 1u;

                    ep0Words[8] = 0u;
                    ep0Words[9] = 0u;
                    ep0Words[10] = 0u;
                    ep0Words[11] = (TrbTypeStatusStage << 10u) | (1u << 5u) | 1u;

                    regs.RingDoorbell(static_cast<uint8_t>(slotId), 1u, 0u);

                    uint32_t fullCc = 0u;
                    uint32_t fullEndpointId = 0u;
                    uint32_t fullEventIndex = 0u;
                    if (!WaitForTransferCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                                   256u,
                                                   ep0Ring.PhysicalAddress,
                                                   ep0Ring.PhysicalAddress + 16ull,
                                                   ep0Ring.PhysicalAddress + 32ull,
                                                   slotId,
                                                   EndpointIdControl,
                                                   MaxPolls,
                                                   fullCc,
                                                   fullEndpointId,
                                                   fullEventIndex)) {
                        PushLog("XHCI GETCFG TD2 NOEVENT");
                        failureStage = "GETCFG_TRANSFER2_NOEVENT";
                        passed = false;
                    } else if ((fullCc != CcSuccess && fullCc != CcShortPacket) || fullEndpointId != EndpointIdControl) {
                        PushLog("XHCI GETCFG TD2 FAIL");
                        failureStage = "GETCFG_TRANSFER2_FAIL";
                        passed = false;
                    } else {
                        AckConsumedEvent(regs, evtRing.PhysicalAddress, fullEventIndex);

                        desc = reinterpret_cast<volatile uint8_t *>(descriptorBuffer.VirtualAddress);
                        const uint8_t fullLength = desc[0];
                        const uint8_t fullDtype = desc[1];
                        if (fullLength < 9u || fullDtype != 2u) {
                            PushLog("XHCI GETCFG DATA2 INVALID");
                            failureStage = "DATA2_INVALID";
                            passed = false;
                        }
                    }
                }

                if (!passed) {
                    goto getcfg_parse_done;
                }

                GLastUsbConfigValue = configValue;
                GHaveLastUsbConfigValue = true;

                if (totalLength < 9u) {
                    totalLength = 9u;
                }
                uint32_t parseLength = totalLength;
                if (parseLength > MaxConfigBytes) {
                    parseLength = MaxConfigBytes;
                }

                uint32_t parsedInterfaces = 0;
                uint32_t parsedEndpoints = 0;
                uint32_t parsedHid = 0;
                uint32_t offset = 0;
                bool foundInterruptInEndpoint = false;
                bool foundKeyboardInterruptInEndpoint = false;
                bool foundMouseInterruptInEndpoint = false;
                bool foundMscBulkPair = false;
                uint8_t currentInterfaceNumber = 0;
                bool haveCurrentInterface = false;
                uint8_t currentInterfaceClass = 0;
                uint8_t currentInterfaceSubclass = 0;
                uint8_t currentInterfaceProtocol = 0;
                bool haveCurrentInterfaceDescriptor = false;
                bool mscCandidateActive = false;
                uint8_t mscCandidateInterface = 0;
                uint8_t mscCandidateBulkInEndpoint = 0;
                uint8_t mscCandidateBulkOutEndpoint = 0;
                uint16_t mscCandidateBulkInMaxPacket = 0;
                uint16_t mscCandidateBulkOutMaxPacket = 0;
                bool foundHidReportDescriptorInfo = false;
                uint8_t hidReportInterfaceNumber = 0;
                uint16_t hidReportDescriptorLength = 0;

                uint8_t keyboardEndpointAddress = 0;
                uint16_t keyboardEndpointMaxPacketSize = 0;
                uint8_t keyboardEndpointInterval = 0;
                uint8_t mouseEndpointAddress = 0;
                uint16_t mouseEndpointMaxPacketSize = 0;
                uint8_t mouseEndpointInterval = 0;
                uint8_t interruptInEndpointAddress = 0;
                uint16_t interruptInEndpointMaxPacketSize = 0;
                uint8_t interruptInEndpointInterval = 0;
                uint8_t mscInterfaceNumber = 0;
                uint8_t mscBulkInEndpointAddress = 0;
                uint8_t mscBulkOutEndpointAddress = 0;
                uint16_t mscBulkInMaxPacketSize = 0;
                uint16_t mscBulkOutMaxPacketSize = 0;

                while (offset + 2u <= parseLength) {
                    const uint8_t dlen = desc[offset + 0u];
                    const uint8_t dt = desc[offset + 1u];
                    if (dlen < 2u) {
                        break;
                    }
                    if (offset + static_cast<uint32_t>(dlen) > parseLength) {
                        break;
                    }

                    if (dt == 4u) {
                        if (!foundMscBulkPair && mscCandidateActive &&
                            mscCandidateBulkInEndpoint != 0u && mscCandidateBulkOutEndpoint != 0u) {
                            foundMscBulkPair = true;
                            mscInterfaceNumber = mscCandidateInterface;
                            mscBulkInEndpointAddress = mscCandidateBulkInEndpoint;
                            mscBulkOutEndpointAddress = mscCandidateBulkOutEndpoint;
                            mscBulkInMaxPacketSize = mscCandidateBulkInMaxPacket;
                            mscBulkOutMaxPacketSize = mscCandidateBulkOutMaxPacket;
                        }

                        parsedInterfaces++;
                        if (dlen >= 9u) {
                            currentInterfaceNumber = desc[offset + 2u];
                            haveCurrentInterface = true;
                            currentInterfaceClass = desc[offset + 5u];
                            currentInterfaceSubclass = desc[offset + 6u];
                            currentInterfaceProtocol = desc[offset + 7u];
                            haveCurrentInterfaceDescriptor = true;
                            mscCandidateActive = (currentInterfaceClass == 0x08u);
                            mscCandidateInterface = currentInterfaceNumber;
                            mscCandidateBulkInEndpoint = 0u;
                            mscCandidateBulkOutEndpoint = 0u;
                            mscCandidateBulkInMaxPacket = 0u;
                            mscCandidateBulkOutMaxPacket = 0u;
                        } else {
                            haveCurrentInterfaceDescriptor = false;
                            mscCandidateActive = false;
                        }
                    } else if (dt == 5u) {
                        parsedEndpoints++;
                        if (dlen >= 7u) {
                            const uint8_t endpointAddress = desc[offset + 2u];
                            const uint8_t attributesField = desc[offset + 3u];
                            const uint16_t maxPacket = static_cast<uint16_t>(desc[offset + 4u] | (static_cast<uint16_t>(desc[offset + 5u]) << 8u));
                            const uint8_t interval = desc[offset + 6u];
                            const bool isIn = (endpointAddress & 0x80u) != 0;
                            const bool isInterrupt = (attributesField & 0x3u) == 0x3u;
                            const bool isBulk = (attributesField & 0x3u) == 0x2u;
                            if (isIn && isInterrupt) {
                                if (!foundInterruptInEndpoint) {
                                    interruptInEndpointAddress = endpointAddress;
                                    interruptInEndpointMaxPacketSize = maxPacket;
                                    interruptInEndpointInterval = interval;
                                    foundInterruptInEndpoint = true;
                                }

                                const bool isBootKeyboard = haveCurrentInterfaceDescriptor &&
                                    currentInterfaceClass == 0x03u &&
                                    currentInterfaceSubclass == 0x01u &&
                                    currentInterfaceProtocol == 0x01u;
                                const bool isBootMouse = haveCurrentInterfaceDescriptor &&
                                    currentInterfaceClass == 0x03u &&
                                    currentInterfaceSubclass == 0x01u &&
                                    currentInterfaceProtocol == 0x02u;
                                if (!foundKeyboardInterruptInEndpoint && isBootKeyboard) {
                                    keyboardEndpointAddress = endpointAddress;
                                    keyboardEndpointMaxPacketSize = maxPacket;
                                    keyboardEndpointInterval = interval;
                                    foundKeyboardInterruptInEndpoint = true;
                                }
                                if (!foundMouseInterruptInEndpoint && isBootMouse) {
                                    mouseEndpointAddress = endpointAddress;
                                    mouseEndpointMaxPacketSize = maxPacket;
                                    mouseEndpointInterval = interval;
                                    foundMouseInterruptInEndpoint = true;
                                }
                            }

                            if (!foundMscBulkPair && mscCandidateActive && isBulk) {
                                if (isIn && mscCandidateBulkInEndpoint == 0u) {
                                    mscCandidateBulkInEndpoint = endpointAddress;
                                    mscCandidateBulkInMaxPacket = maxPacket;
                                }
                                if (!isIn && mscCandidateBulkOutEndpoint == 0u) {
                                    mscCandidateBulkOutEndpoint = endpointAddress;
                                    mscCandidateBulkOutMaxPacket = maxPacket;
                                }
                                if (mscCandidateBulkInEndpoint != 0u && mscCandidateBulkOutEndpoint != 0u) {
                                    foundMscBulkPair = true;
                                    mscInterfaceNumber = mscCandidateInterface;
                                    mscBulkInEndpointAddress = mscCandidateBulkInEndpoint;
                                    mscBulkOutEndpointAddress = mscCandidateBulkOutEndpoint;
                                    mscBulkInMaxPacketSize = mscCandidateBulkInMaxPacket;
                                    mscBulkOutMaxPacketSize = mscCandidateBulkOutMaxPacket;
                                }
                            }
                        }
                    } else if (dt == 0x21u) {
                        parsedHid++;
                        if (haveCurrentInterface && dlen >= 9u) {
                            const uint8_t descriptorCount = desc[offset + 5u];
                            uint32_t subOffset = offset + 6u;
                            for (uint8_t d = 0; d < descriptorCount; d++) {
                                if (subOffset + 3u > (offset + static_cast<uint32_t>(dlen))) {
                                    break;
                                }
                                const uint8_t subType = desc[subOffset + 0u];
                                const uint16_t subLength = static_cast<uint16_t>(
                                    desc[subOffset + 1u] | (static_cast<uint16_t>(desc[subOffset + 2u]) << 8u));
                                if (!foundHidReportDescriptorInfo && subType == 0x22u && subLength > 0u) {
                                    foundHidReportDescriptorInfo = true;
                                    hidReportInterfaceNumber = currentInterfaceNumber;
                                    hidReportDescriptorLength = subLength;
                                }
                                subOffset += 3u;
                            }
                        }
                    }

                    offset += static_cast<uint32_t>(dlen);
                }

                if (!foundMscBulkPair && mscCandidateActive &&
                    mscCandidateBulkInEndpoint != 0u && mscCandidateBulkOutEndpoint != 0u) {
                    foundMscBulkPair = true;
                    mscInterfaceNumber = mscCandidateInterface;
                    mscBulkInEndpointAddress = mscCandidateBulkInEndpoint;
                    mscBulkOutEndpointAddress = mscCandidateBulkOutEndpoint;
                    mscBulkInMaxPacketSize = mscCandidateBulkInMaxPacket;
                    mscBulkOutMaxPacketSize = mscCandidateBulkOutMaxPacket;
                }

                char line[96] = {};
                size_t pos = 0;
                AppendString(line, sizeof(line), pos, "USB CFG TOTLEN ");
                AppendUInt(line, sizeof(line), pos, totalLength);
                AppendString(line, sizeof(line), pos, " CFGVAL ");
                AppendUInt(line, sizeof(line), pos, configValue);
                AppendString(line, sizeof(line), pos, " ATTR ");
                AppendHex(line, sizeof(line), pos, attributes);
                PushLog(line);

                pos = 0;
                line[0] = '\0';
                AppendString(line, sizeof(line), pos, "USB CFG IFH ");
                AppendUInt(line, sizeof(line), pos, interfaceCountHeader);
                AppendString(line, sizeof(line), pos, " IF ");
                AppendUInt(line, sizeof(line), pos, parsedInterfaces);
                AppendString(line, sizeof(line), pos, " EP ");
                AppendUInt(line, sizeof(line), pos, parsedEndpoints);
                AppendString(line, sizeof(line), pos, " HID ");
                AppendUInt(line, sizeof(line), pos, parsedHid);
                PushLog(line);

                pos = 0;
                line[0] = '\0';
                AppendString(line, sizeof(line), pos, "USB CFG MAXPWR ");
                AppendUInt(line, sizeof(line), pos, static_cast<uint64_t>(maxPowerUnits) * 2ull);
                AppendString(line, sizeof(line), pos, "mA PARSED ");
                AppendUInt(line, sizeof(line), pos, offset);
                if (totalLength > MaxConfigBytes) {
                    AppendString(line, sizeof(line), pos, " TRUNC");
                }
                PushLog(line);

                GHaveLastUsbMscBulkPair = false;
                GHaveLastUsbMscInterface = false;
                if (foundMouseInterruptInEndpoint) {
                    GLastUsbMouseInterruptInEndpointAddress = mouseEndpointAddress;
                    GLastUsbMouseInterruptInMaxPacketSize = mouseEndpointMaxPacketSize;
                    GLastUsbMouseInterruptInInterval = mouseEndpointInterval;
                    GHaveLastUsbMouseInterruptInEndpoint = true;
                }

                if (foundKeyboardInterruptInEndpoint) {
                    GLastUsbInterruptInEndpointAddress = keyboardEndpointAddress;
                    GLastUsbInterruptInMaxPacketSize = keyboardEndpointMaxPacketSize;
                    GLastUsbInterruptInInterval = keyboardEndpointInterval;
                    GHaveLastUsbInterruptInEndpoint = true;
                    GLastUsbIntrinEndpointKind = EUsbIntrinEndpointKind::KeyboardBoot;

                    GLastUsbKeyboardInterruptInEndpointAddress = keyboardEndpointAddress;
                    GLastUsbKeyboardInterruptInMaxPacketSize = keyboardEndpointMaxPacketSize;
                    GLastUsbKeyboardInterruptInInterval = keyboardEndpointInterval;
                    GHaveLastUsbKeyboardInterruptInEndpoint = true;
                } else if (foundInterruptInEndpoint && !GHaveLastUsbKeyboardInterruptInEndpoint) {
                    GLastUsbInterruptInEndpointAddress = interruptInEndpointAddress;
                    GLastUsbInterruptInMaxPacketSize = interruptInEndpointMaxPacketSize;
                    GLastUsbInterruptInInterval = interruptInEndpointInterval;
                    GHaveLastUsbInterruptInEndpoint = true;
                    GLastUsbIntrinEndpointKind = EUsbIntrinEndpointKind::Generic;
                }

                if (foundMscBulkPair) {
                    GLastUsbMscInterfaceNumber = mscInterfaceNumber;
                    GHaveLastUsbMscInterface = true;
                    GLastUsbMscBulkInEndpointAddress = mscBulkInEndpointAddress;
                    GLastUsbMscBulkOutEndpointAddress = mscBulkOutEndpointAddress;
                    GLastUsbMscBulkInMaxPacketSize = mscBulkInMaxPacketSize;
                    GLastUsbMscBulkOutMaxPacketSize = mscBulkOutMaxPacketSize;
                    GHaveLastUsbMscBulkPair = true;
                }

                if (GHaveLastUsbInterruptInEndpoint) {
                    pos = 0;
                    line[0] = '\0';
                    AppendString(line, sizeof(line), pos, "USB EP INTIN ");
                    AppendHex(line, sizeof(line), pos, GLastUsbInterruptInEndpointAddress);
                    AppendString(line, sizeof(line), pos, " MPS ");
                    AppendUInt(line, sizeof(line), pos, GLastUsbInterruptInMaxPacketSize);
                    AppendString(line, sizeof(line), pos, " INTV ");
                    AppendUInt(line, sizeof(line), pos, GLastUsbInterruptInInterval);
                    if (GLastUsbIntrinEndpointKind == EUsbIntrinEndpointKind::KeyboardBoot) {
                        AppendString(line, sizeof(line), pos, " KBD");
                    }
                    PushLog(line);
                }

                if (GHaveLastUsbMouseInterruptInEndpoint) {
                    pos = 0;
                    line[0] = '\0';
                    AppendString(line, sizeof(line), pos, "USB EP INTIN MOUSE ");
                    AppendHex(line, sizeof(line), pos, GLastUsbMouseInterruptInEndpointAddress);
                    AppendString(line, sizeof(line), pos, " MPS ");
                    AppendUInt(line, sizeof(line), pos, GLastUsbMouseInterruptInMaxPacketSize);
                    AppendString(line, sizeof(line), pos, " INTV ");
                    AppendUInt(line, sizeof(line), pos, GLastUsbMouseInterruptInInterval);
                    PushLog(line);
                }

                if (GHaveLastUsbMscBulkPair) {
                    pos = 0;
                    line[0] = '\0';
                    AppendString(line, sizeof(line), pos, "USB MSC IF ");
                    AppendUInt(line, sizeof(line), pos, GLastUsbMscInterfaceNumber);
                    AppendString(line, sizeof(line), pos, " IN ");
                    AppendHex(line, sizeof(line), pos, GLastUsbMscBulkInEndpointAddress);
                    AppendString(line, sizeof(line), pos, " OUT ");
                    AppendHex(line, sizeof(line), pos, GLastUsbMscBulkOutEndpointAddress);
                    AppendString(line, sizeof(line), pos, " INMPS ");
                    AppendUInt(line, sizeof(line), pos, GLastUsbMscBulkInMaxPacketSize);
                    AppendString(line, sizeof(line), pos, " OUTMPS ");
                    AppendUInt(line, sizeof(line), pos, GLastUsbMscBulkOutMaxPacketSize);
                    PushLog(line);
                } else {
                    PushLog("USB MSC NONE");
                }

                if (foundHidReportDescriptorInfo) {
                    char hidLine[96] = {};
                    size_t hidPos = 0;
                    AppendString(hidLine, sizeof(hidLine), hidPos, "USB HID REPORT LEN ");
                    AppendUInt(hidLine, sizeof(hidLine), hidPos, hidReportDescriptorLength);
                    AppendString(hidLine, sizeof(hidLine), hidPos, " IF ");
                    AppendUInt(hidLine, sizeof(hidLine), hidPos, hidReportInterfaceNumber);
                    AppendString(hidLine, sizeof(hidLine), hidPos, " SKIP");
                    PushLog(hidLine);
                }
            }
getcfg_parse_done:
            ;
        }
    }

    {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        (void)WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls);
    }

    bool cleanupOk = true;
    if (haveDescriptorBuffer && !FDmaMemoryManager::FreeBuffer(descriptorBuffer)) {
        cleanupOk = false;
    }
    if (haveEp0Ring && !FDmaMemoryManager::FreeBuffer(ep0Ring)) {
        cleanupOk = false;
    }
    if (haveErst && !FDmaMemoryManager::FreeBuffer(erst)) {
        cleanupOk = false;
    }
    if (haveEvtRing && !FDmaMemoryManager::FreeBuffer(evtRing)) {
        cleanupOk = false;
    }
    if (haveCmdRing && !FDmaMemoryManager::FreeBuffer(cmdRing)) {
        cleanupOk = false;
    }
    if (haveInputContext && !FDmaMemoryManager::FreeBuffer(inputContext)) {
        cleanupOk = false;
    }
    if (haveOutputContext && !FDmaMemoryManager::FreeBuffer(outputContext)) {
        cleanupOk = false;
    }
    if (haveDcbaa && !FDmaMemoryManager::FreeBuffer(dcbaa)) {
        cleanupOk = false;
    }
    if (!FPinnedMappingManager::UnmapPhysicalRange(mmio)) {
        cleanupOk = false;
    }

    if (!cleanupOk) {
        PushLog("XHCI GETCFG CLEANUP FAIL");
        if (passed) {
            failureStage = "CLEANUP";
        }
    }

    if (!passed) {
        char stageLine[96] = {};
        size_t stagePos = 0;
        AppendString(stageLine, sizeof(stageLine), stagePos, "XHCI GETCFG FAIL STAGE ");
        AppendString(stageLine, sizeof(stageLine), stagePos, failureStage);
        PushLog(stageLine);
    }

    PushLog(passed ? "XHCI GETCFG PASS" : "XHCI GETCFG FAIL");
}

static void RunXhciSetConfiguration() {
    PushLog("XHCI SETCFG START");

    const uint8_t requestedConfigValue = GHaveLastUsbConfigValue ? GLastUsbConfigValue : 1u;
    if (!GHaveLastUsbConfigValue) {
        PushLog("XHCI SETCFG CFG DEFAULT 1");
    }

    FXhciControllerInfo info{};
    if (!FXhciPciDiscovery::DiscoverFirst(info) || !info.Found) {
        PushLog("XHCI SETCFG NONE");
        return;
    }

    FPinnedMapping mmio{};
    if (!FPinnedMappingManager::MapPhysicalRange(info.MmioBase,
                                                 info.MmioSize,
                                                 FVirtualMemoryManager::FlagsDeviceRWUCNX,
                                                 mmio)) {
        PushLog("XHCI SETCFG MAP FAIL");
        return;
    }

    FXhciMmioRegisters regs;
    if (!regs.Initialize(mmio.VirtualAddress)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog("XHCI SETCFG REGS FAIL");
        return;
    }

    const uint64_t mappedVirtualBase = mmio.VirtualAddress - (info.MmioBase - (info.MmioBase & ~(FVirtualMemoryManager::PageSize - 1ull)));
    const uint64_t mappedSizeBytes = mmio.PageCount * FVirtualMemoryManager::PageSize;
    if (!IsXhciRegisterWindowSafe(regs, mappedVirtualBase, mappedSizeBytes)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog("XHCI SETCFG MMIO WINDOW FAIL");
        return;
    }

    constexpr uint32_t UsbCmdRunStop = 1u << 0;
    constexpr uint32_t UsbCmdHostControllerReset = 1u << 1;
    constexpr uint32_t UsbCmdInterruptEnable = 1u << 2;
    constexpr uint32_t UsbStsHostControllerHalted = 1u << 0;
    constexpr uint32_t UsbStsControllerNotReady = 1u << 11;
    constexpr uint64_t MaxPolls = 3000000ull;

    constexpr uint32_t TrbTypeLink = 6u;
    constexpr uint32_t TrbTypeEnableSlot = 9u;
    constexpr uint32_t TrbTypeAddressDevice = 11u;
    constexpr uint32_t TrbTypeSetupStage = 2u;
    constexpr uint32_t TrbTypeStatusStage = 4u;

    constexpr uint32_t CcSuccess = 1u;
    constexpr uint32_t EndpointIdControl = 1u;

    FDmaBuffer dcbaa{};
    FDmaBuffer outputContext{};
    FDmaBuffer inputContext{};
    FDmaBuffer cmdRing{};
    FDmaBuffer evtRing{};
    FDmaBuffer erst{};
    FDmaBuffer ep0Ring{};

    bool haveDcbaa = false;
    bool haveOutputContext = false;
    bool haveInputContext = false;
    bool haveCmdRing = false;
    bool haveEvtRing = false;
    bool haveErst = false;
    bool haveEp0Ring = false;

    bool passed = true;
    uint64_t polls = 0;

    if (!WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog("XHCI SETCFG CNR TIMEOUT");
        passed = false;
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls)) {
            PushLog("XHCI SETCFG HALT TIMEOUT");
            passed = false;
        }
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdHostControllerReset);
        if (!WaitForUsbCmdBit(regs, UsbCmdHostControllerReset, false, MaxPolls, polls)) {
            PushLog("XHCI SETCFG RESET TIMEOUT");
            passed = false;
        }
    }

    if (passed && !WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog("XHCI SETCFG READY TIMEOUT");
        passed = false;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, dcbaa)) {
        PushLog("XHCI SETCFG DCBAA FAIL");
        passed = false;
    } else if (passed) {
        haveDcbaa = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, outputContext)) {
        PushLog("XHCI SETCFG OUTCTX FAIL");
        passed = false;
    } else if (passed) {
        haveOutputContext = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, inputContext)) {
        PushLog("XHCI SETCFG INCTX FAIL");
        passed = false;
    } else if (passed) {
        haveInputContext = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, cmdRing)) {
        PushLog("XHCI SETCFG CR FAIL");
        passed = false;
    } else if (passed) {
        haveCmdRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, evtRing)) {
        PushLog("XHCI SETCFG ER FAIL");
        passed = false;
    } else if (passed) {
        haveEvtRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, erst)) {
        PushLog("XHCI SETCFG ERST FAIL");
        passed = false;
    } else if (passed) {
        haveErst = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, ep0Ring)) {
        PushLog("XHCI SETCFG EP0RING FAIL");
        passed = false;
    } else if (passed) {
        haveEp0Ring = true;
    }

    uint8_t connectedPort = 0;
    uint32_t connectedPortSc = 0;
    if (passed) {
        const uint32_t hcs1 = regs.ReadHcsParams1();
        const uint32_t maxPorts = (hcs1 >> 24u) & 0xFFu;
        for (uint8_t port = 1; port <= maxPorts; port++) {
            const uint32_t portSc = regs.ReadPortSc(port);
            if ((portSc & 0x1u) != 0) {
                connectedPort = port;
                connectedPortSc = portSc;
                break;
            }
        }

        if (connectedPort == 0) {
            PushLog("XHCI SETCFG NOPORT");
            passed = false;
        }
    }

    uint32_t slotId = 0;
    if (passed) {
        Fortress::Runtime::Memset(reinterpret_cast<void *>(dcbaa.VirtualAddress), 0, static_cast<Fortress::Core::usize>(dcbaa.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(outputContext.VirtualAddress), 0, static_cast<Fortress::Core::usize>(outputContext.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(inputContext.VirtualAddress), 0, static_cast<Fortress::Core::usize>(inputContext.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(cmdRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(cmdRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(evtRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(evtRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(erst.VirtualAddress), 0, static_cast<Fortress::Core::usize>(erst.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(ep0Ring.VirtualAddress), 0, static_cast<Fortress::Core::usize>(ep0Ring.SizeBytes));

        volatile uint64_t *dcbaaEntries = reinterpret_cast<volatile uint64_t *>(dcbaa.VirtualAddress);
        dcbaaEntries[0] = 0;

        volatile uint32_t *cmdWords = reinterpret_cast<volatile uint32_t *>(cmdRing.VirtualAddress);
        cmdWords[0] = 0;
        cmdWords[1] = 0;
        cmdWords[2] = 0;
        cmdWords[3] = (TrbTypeEnableSlot << 10u) | 1u;

        constexpr uint32_t CmdRingTrbCount = 256u;
        volatile uint32_t *cmdLinkWords = cmdWords + ((CmdRingTrbCount - 1u) * 4u);
        WriteLe64(cmdLinkWords, cmdRing.PhysicalAddress);
        cmdLinkWords[2] = 0;
        cmdLinkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

        volatile uint32_t *erstWords = reinterpret_cast<volatile uint32_t *>(erst.VirtualAddress);
        WriteLe64(erstWords, evtRing.PhysicalAddress);
        erstWords[2] = 256u;
        erstWords[3] = 0;

        regs.WriteConfig(8u);
        regs.WriteDcbaap(dcbaa.PhysicalAddress & ~0x3Full);
        regs.WriteCrcr((cmdRing.PhysicalAddress & ~0x3Full) | 1ull);
        regs.WriteInterrupterIman(0, 0u);
        regs.WriteInterrupterImod(0, 0u);
        regs.WriteInterrupterErstsz(0, 1u);
        regs.WriteInterrupterErstba(0, erst.PhysicalAddress & ~0x3Full);
        regs.WriteInterrupterErdp(0, evtRing.PhysicalAddress & ~0xFull);

        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdRunStop | UsbCmdInterruptEnable);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, false, MaxPolls, polls)) {
            PushLog("XHCI SETCFG RUN TIMEOUT");
            passed = false;
        }

        if (passed) {
            regs.RingDoorbell(0, 0, 0);
            uint32_t completionCode = 0;
            uint32_t eventIndex = 0;
            if (!WaitForCommandCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                          256u,
                                          cmdRing.PhysicalAddress,
                                          MaxPolls,
                                          completionCode,
                                          slotId,
                                          eventIndex)) {
                PushLog("XHCI SETCFG ES NOEVENT");
                passed = false;
            } else if (completionCode != CcSuccess || slotId == 0) {
                PushLog("XHCI SETCFG ES FAIL");
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }
        }

        if (passed) {
            dcbaaEntries[slotId] = outputContext.PhysicalAddress & ~0x3Full;

            const uint32_t hcc1 = regs.ReadHccParams1();
            const uint32_t contextSize = ((hcc1 & (1u << 2u)) != 0) ? 64u : 32u;
            const uint32_t portSpeed = (connectedPortSc >> 10u) & 0xFu;

            uint32_t maxPacketSize = 8u;
            if (portSpeed == 3u) {
                maxPacketSize = 64u;
            } else if (portSpeed >= 4u) {
                maxPacketSize = 512u;
            }

            volatile uint32_t *inputWords = reinterpret_cast<volatile uint32_t *>(inputContext.VirtualAddress);
            inputWords[1] = 0x3u;

            volatile uint8_t *inputBytes = reinterpret_cast<volatile uint8_t *>(inputContext.VirtualAddress);
            volatile uint32_t *slotCtx = reinterpret_cast<volatile uint32_t *>(inputBytes + contextSize);
            volatile uint32_t *ep0Ctx = reinterpret_cast<volatile uint32_t *>(inputBytes + (2u * contextSize));

            slotCtx[0] = (portSpeed << 20u) | (1u << 27u);
            slotCtx[1] = static_cast<uint32_t>(connectedPort) << 16u;

            ep0Ctx[1] = 3u | (4u << 3u) | (maxPacketSize << 16u);
            WriteLe64(ep0Ctx + 2, (ep0Ring.PhysicalAddress & ~0xFull) | 1ull);

            volatile uint32_t *addrTrb = cmdWords + 4u;
            WriteLe64(addrTrb, inputContext.PhysicalAddress & ~0xFull);
            addrTrb[2] = 0;
            addrTrb[3] = (TrbTypeAddressDevice << 10u) | (static_cast<uint32_t>(slotId) << 24u) | 1u;

            regs.RingDoorbell(0, 0, 0);

            uint32_t completionCode = 0;
            uint32_t returnedSlotId = 0;
            uint32_t eventIndex = 0;
            if (!WaitForCommandCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                          256u,
                                          cmdRing.PhysicalAddress + 16ull,
                                          MaxPolls,
                                          completionCode,
                                          returnedSlotId,
                                          eventIndex)) {
                PushLog("XHCI SETCFG AD NOEVENT");
                passed = false;
            } else if (completionCode != CcSuccess || returnedSlotId != slotId) {
                PushLog("XHCI SETCFG AD FAIL");
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }
        }

        if (passed) {
            volatile uint32_t *ep0Words = reinterpret_cast<volatile uint32_t *>(ep0Ring.VirtualAddress);

            ep0Words[0] = static_cast<uint32_t>(requestedConfigValue) << 16u;
            ep0Words[0] |= 0x00000900u; // bmRequestType=0x00, bRequest=0x09
            ep0Words[1] = 0; // wIndex=0x0000, wLength=0
            ep0Words[2] = 8u;
            ep0Words[3] = (TrbTypeSetupStage << 10u) | (1u << 6u) | (1u << 4u) | 1u;

            ep0Words[4] = 0;
            ep0Words[5] = 0;
            ep0Words[6] = 0;
            ep0Words[7] = (TrbTypeStatusStage << 10u) | (1u << 5u) | 1u;

            constexpr uint32_t Ep0TrbCount = 256u;
            volatile uint32_t *ep0LinkWords = ep0Words + ((Ep0TrbCount - 1u) * 4u);
            WriteLe64(ep0LinkWords, ep0Ring.PhysicalAddress);
            ep0LinkWords[2] = 0;
            ep0LinkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

            regs.RingDoorbell(static_cast<uint8_t>(slotId), 1u, 0u);

            uint32_t completionCode = 0;
            uint32_t endpointId = 0;
            uint32_t eventIndex = 0;
            if (!WaitForTransferCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                           256u,
                                           ep0Ring.PhysicalAddress,
                                           ep0Ring.PhysicalAddress + 16ull,
                                           ep0Ring.PhysicalAddress + 32ull,
                                           slotId,
                                           EndpointIdControl,
                                           MaxPolls,
                                           completionCode,
                                           endpointId,
                                           eventIndex)) {
                PushLog("XHCI SETCFG TD NOEVENT");
                LogEventRingDebug(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress), 256u, 8u);
                passed = false;
            } else if (completionCode != CcSuccess || endpointId != EndpointIdControl) {
                PushLog("XHCI SETCFG TD FAIL");
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);

                char line[96] = {};
                size_t pos = 0;
                AppendString(line, sizeof(line), pos, "XHCI SETCFG VALUE ");
                AppendUInt(line, sizeof(line), pos, requestedConfigValue);
                PushLog(line);
            }
        }
    }

    {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        (void)WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls);
    }

    bool cleanupOk = true;
    if (haveEp0Ring && !FDmaMemoryManager::FreeBuffer(ep0Ring)) {
        cleanupOk = false;
    }
    if (haveErst && !FDmaMemoryManager::FreeBuffer(erst)) {
        cleanupOk = false;
    }
    if (haveEvtRing && !FDmaMemoryManager::FreeBuffer(evtRing)) {
        cleanupOk = false;
    }
    if (haveCmdRing && !FDmaMemoryManager::FreeBuffer(cmdRing)) {
        cleanupOk = false;
    }
    if (haveInputContext && !FDmaMemoryManager::FreeBuffer(inputContext)) {
        cleanupOk = false;
    }
    if (haveOutputContext && !FDmaMemoryManager::FreeBuffer(outputContext)) {
        cleanupOk = false;
    }
    if (haveDcbaa && !FDmaMemoryManager::FreeBuffer(dcbaa)) {
        cleanupOk = false;
    }
    if (!FPinnedMappingManager::UnmapPhysicalRange(mmio)) {
        cleanupOk = false;
    }

    if (!cleanupOk) {
        PushLog("XHCI SETCFG CLEANUP FAIL");
    }

    PushLog(passed ? "XHCI SETCFG PASS" : "XHCI SETCFG FAIL");
}

static void RunXhciConfigureInterruptEndpoint() {
    PushLog("XHCI EPCONF START");

    const uint8_t requestedConfigValue = GHaveLastUsbConfigValue ? GLastUsbConfigValue : 1u;
    if (!GHaveLastUsbConfigValue) {
        PushLog("XHCI EPCONF CFG DEFAULT 1");
    }

    uint8_t endpointAddress = GLastUsbInterruptInEndpointAddress;
    uint16_t endpointMaxPacketSize = GLastUsbInterruptInMaxPacketSize;
    uint8_t endpointInterval = GLastUsbInterruptInInterval;
    if (GHaveLastUsbKeyboardInterruptInEndpoint) {
        endpointAddress = GLastUsbKeyboardInterruptInEndpointAddress;
        endpointMaxPacketSize = GLastUsbKeyboardInterruptInMaxPacketSize;
        endpointInterval = GLastUsbKeyboardInterruptInInterval;
    }
    if (!GHaveLastUsbInterruptInEndpoint) {
        endpointAddress = 0x81u;
        endpointMaxPacketSize = 8u;
        endpointInterval = 10u;
        PushLog("XHCI EPCONF EP DEFAULT 0x81");
    }

    const uint8_t endpointNumber = endpointAddress & 0xFu;
    const bool endpointIn = (endpointAddress & 0x80u) != 0;
    const uint32_t endpointId = static_cast<uint32_t>(endpointNumber) * 2u + (endpointIn ? 1u : 0u);
    const uint32_t endpointType = endpointIn ? 7u : 3u;
    const uint32_t intervalField = endpointInterval == 0 ? 0u : static_cast<uint32_t>(endpointInterval - 1u);

    FXhciControllerInfo info{};
    if (!FXhciPciDiscovery::DiscoverFirst(info) || !info.Found) {
        PushLog("XHCI EPCONF NONE");
        return;
    }

    FPinnedMapping mmio{};
    if (!FPinnedMappingManager::MapPhysicalRange(info.MmioBase,
                                                 info.MmioSize,
                                                 FVirtualMemoryManager::FlagsDeviceRWUCNX,
                                                 mmio)) {
        PushLog("XHCI EPCONF MAP FAIL");
        return;
    }

    FXhciMmioRegisters regs;
    if (!regs.Initialize(mmio.VirtualAddress)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog("XHCI EPCONF REGS FAIL");
        return;
    }

    const uint64_t mappedVirtualBase = mmio.VirtualAddress - (info.MmioBase - (info.MmioBase & ~(FVirtualMemoryManager::PageSize - 1ull)));
    const uint64_t mappedSizeBytes = mmio.PageCount * FVirtualMemoryManager::PageSize;
    if (!IsXhciRegisterWindowSafe(regs, mappedVirtualBase, mappedSizeBytes)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog("XHCI EPCONF MMIO WINDOW FAIL");
        return;
    }

    constexpr uint32_t UsbCmdRunStop = 1u << 0;
    constexpr uint32_t UsbCmdHostControllerReset = 1u << 1;
    constexpr uint32_t UsbCmdInterruptEnable = 1u << 2;
    constexpr uint32_t UsbStsHostControllerHalted = 1u << 0;
    constexpr uint32_t UsbStsControllerNotReady = 1u << 11;
    constexpr uint64_t MaxPolls = 3000000ull;

    constexpr uint32_t TrbTypeLink = 6u;
    constexpr uint32_t TrbTypeEnableSlot = 9u;
    constexpr uint32_t TrbTypeAddressDevice = 11u;
    constexpr uint32_t TrbTypeConfigureEndpoint = 12u;
    constexpr uint32_t TrbTypeSetupStage = 2u;
    constexpr uint32_t TrbTypeStatusStage = 4u;

    constexpr uint32_t CcSuccess = 1u;
    constexpr uint32_t EndpointIdControl = 1u;

    FDmaBuffer dcbaa{};
    FDmaBuffer outputContext{};
    FDmaBuffer inputContext{};
    FDmaBuffer cmdRing{};
    FDmaBuffer evtRing{};
    FDmaBuffer erst{};
    FDmaBuffer ep0Ring{};
    FDmaBuffer endpointRing{};

    bool haveDcbaa = false;
    bool haveOutputContext = false;
    bool haveInputContext = false;
    bool haveCmdRing = false;
    bool haveEvtRing = false;
    bool haveErst = false;
    bool haveEp0Ring = false;
    bool haveEndpointRing = false;

    bool passed = true;
    uint64_t polls = 0;

    if (!WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog("XHCI EPCONF CNR TIMEOUT");
        passed = false;
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls)) {
            PushLog("XHCI EPCONF HALT TIMEOUT");
            passed = false;
        }
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdHostControllerReset);
        if (!WaitForUsbCmdBit(regs, UsbCmdHostControllerReset, false, MaxPolls, polls)) {
            PushLog("XHCI EPCONF RESET TIMEOUT");
            passed = false;
        }
    }

    if (passed && !WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog("XHCI EPCONF READY TIMEOUT");
        passed = false;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, dcbaa)) {
        PushLog("XHCI EPCONF DCBAA FAIL");
        passed = false;
    } else if (passed) {
        haveDcbaa = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, outputContext)) {
        PushLog("XHCI EPCONF OUTCTX FAIL");
        passed = false;
    } else if (passed) {
        haveOutputContext = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, inputContext)) {
        PushLog("XHCI EPCONF INCTX FAIL");
        passed = false;
    } else if (passed) {
        haveInputContext = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, cmdRing)) {
        PushLog("XHCI EPCONF CR FAIL");
        passed = false;
    } else if (passed) {
        haveCmdRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, evtRing)) {
        PushLog("XHCI EPCONF ER FAIL");
        passed = false;
    } else if (passed) {
        haveEvtRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, erst)) {
        PushLog("XHCI EPCONF ERST FAIL");
        passed = false;
    } else if (passed) {
        haveErst = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, ep0Ring)) {
        PushLog("XHCI EPCONF EP0RING FAIL");
        passed = false;
    } else if (passed) {
        haveEp0Ring = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, endpointRing)) {
        PushLog("XHCI EPCONF EPRING FAIL");
        passed = false;
    } else if (passed) {
        haveEndpointRing = true;
    }

    uint8_t connectedPort = 0;
    uint32_t connectedPortSc = 0;
    if (passed) {
        const uint32_t hcs1 = regs.ReadHcsParams1();
        const uint32_t maxPorts = (hcs1 >> 24u) & 0xFFu;
        for (uint8_t port = 1; port <= maxPorts; port++) {
            const uint32_t portSc = regs.ReadPortSc(port);
            if ((portSc & 0x1u) != 0) {
                connectedPort = port;
                connectedPortSc = portSc;
                break;
            }
        }

        if (connectedPort == 0) {
            PushLog("XHCI EPCONF NOPORT");
            passed = false;
        }
    }

    uint32_t slotId = 0;
    if (passed) {
        Fortress::Runtime::Memset(reinterpret_cast<void *>(dcbaa.VirtualAddress), 0, static_cast<Fortress::Core::usize>(dcbaa.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(outputContext.VirtualAddress), 0, static_cast<Fortress::Core::usize>(outputContext.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(inputContext.VirtualAddress), 0, static_cast<Fortress::Core::usize>(inputContext.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(cmdRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(cmdRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(evtRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(evtRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(erst.VirtualAddress), 0, static_cast<Fortress::Core::usize>(erst.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(ep0Ring.VirtualAddress), 0, static_cast<Fortress::Core::usize>(ep0Ring.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(endpointRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(endpointRing.SizeBytes));

        volatile uint64_t *dcbaaEntries = reinterpret_cast<volatile uint64_t *>(dcbaa.VirtualAddress);
        dcbaaEntries[0] = 0;

        volatile uint32_t *cmdWords = reinterpret_cast<volatile uint32_t *>(cmdRing.VirtualAddress);
        cmdWords[0] = 0;
        cmdWords[1] = 0;
        cmdWords[2] = 0;
        cmdWords[3] = (TrbTypeEnableSlot << 10u) | 1u;

        constexpr uint32_t CmdRingTrbCount = 256u;
        volatile uint32_t *cmdLinkWords = cmdWords + ((CmdRingTrbCount - 1u) * 4u);
        WriteLe64(cmdLinkWords, cmdRing.PhysicalAddress);
        cmdLinkWords[2] = 0;
        cmdLinkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

        constexpr uint32_t EpTrbCount = 256u;
        volatile uint32_t *ep0LinkWords = reinterpret_cast<volatile uint32_t *>(ep0Ring.VirtualAddress) + ((EpTrbCount - 1u) * 4u);
        WriteLe64(ep0LinkWords, ep0Ring.PhysicalAddress);
        ep0LinkWords[2] = 0;
        ep0LinkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

        volatile uint32_t *epLinkWords = reinterpret_cast<volatile uint32_t *>(endpointRing.VirtualAddress) + ((EpTrbCount - 1u) * 4u);
        WriteLe64(epLinkWords, endpointRing.PhysicalAddress);
        epLinkWords[2] = 0;
        epLinkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

        volatile uint32_t *erstWords = reinterpret_cast<volatile uint32_t *>(erst.VirtualAddress);
        WriteLe64(erstWords, evtRing.PhysicalAddress);
        erstWords[2] = 256u;
        erstWords[3] = 0;

        regs.WriteConfig(8u);
        regs.WriteDcbaap(dcbaa.PhysicalAddress & ~0x3Full);
        regs.WriteCrcr((cmdRing.PhysicalAddress & ~0x3Full) | 1ull);
        regs.WriteInterrupterIman(0, 0u);
        regs.WriteInterrupterImod(0, 0u);
        regs.WriteInterrupterErstsz(0, 1u);
        regs.WriteInterrupterErstba(0, erst.PhysicalAddress & ~0x3Full);
        regs.WriteInterrupterErdp(0, evtRing.PhysicalAddress & ~0xFull);

        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdRunStop | UsbCmdInterruptEnable);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, false, MaxPolls, polls)) {
            PushLog("XHCI EPCONF RUN TIMEOUT");
            passed = false;
        }

        if (passed) {
            regs.RingDoorbell(0, 0, 0);
            uint32_t completionCode = 0;
            uint32_t eventIndex = 0;
            if (!WaitForCommandCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                          256u,
                                          cmdRing.PhysicalAddress,
                                          MaxPolls,
                                          completionCode,
                                          slotId,
                                          eventIndex)) {
                PushLog("XHCI EPCONF ES NOEVENT");
                passed = false;
            } else if (completionCode != CcSuccess || slotId == 0) {
                PushLog("XHCI EPCONF ES FAIL");
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }
        }

        if (passed) {
            dcbaaEntries[slotId] = outputContext.PhysicalAddress & ~0x3Full;

            const uint32_t hcc1 = regs.ReadHccParams1();
            const uint32_t contextSize = ((hcc1 & (1u << 2u)) != 0) ? 64u : 32u;
            const uint32_t portSpeed = (connectedPortSc >> 10u) & 0xFu;

            uint32_t maxPacketSize0 = 8u;
            if (portSpeed == 3u) {
                maxPacketSize0 = 64u;
            } else if (portSpeed >= 4u) {
                maxPacketSize0 = 512u;
            }

            volatile uint32_t *inputWords = reinterpret_cast<volatile uint32_t *>(inputContext.VirtualAddress);
            inputWords[1] = 0x3u;

            volatile uint8_t *inputBytes = reinterpret_cast<volatile uint8_t *>(inputContext.VirtualAddress);
            volatile uint32_t *slotCtx = reinterpret_cast<volatile uint32_t *>(inputBytes + contextSize);
            volatile uint32_t *ep0Ctx = reinterpret_cast<volatile uint32_t *>(inputBytes + (2u * contextSize));

            slotCtx[0] = (portSpeed << 20u) | (1u << 27u);
            slotCtx[1] = static_cast<uint32_t>(connectedPort) << 16u;

            ep0Ctx[1] = 3u | (4u << 3u) | (maxPacketSize0 << 16u);
            WriteLe64(ep0Ctx + 2, (ep0Ring.PhysicalAddress & ~0xFull) | 1ull);

            volatile uint32_t *addrTrb = cmdWords + 4u;
            WriteLe64(addrTrb, inputContext.PhysicalAddress & ~0xFull);
            addrTrb[2] = 0;
            addrTrb[3] = (TrbTypeAddressDevice << 10u) | (static_cast<uint32_t>(slotId) << 24u) | 1u;

            regs.RingDoorbell(0, 0, 0);

            uint32_t completionCode = 0;
            uint32_t returnedSlotId = 0;
            uint32_t eventIndex = 0;
            if (!WaitForCommandCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                          256u,
                                          cmdRing.PhysicalAddress + 16ull,
                                          MaxPolls,
                                          completionCode,
                                          returnedSlotId,
                                          eventIndex)) {
                PushLog("XHCI EPCONF AD NOEVENT");
                passed = false;
            } else if (completionCode != CcSuccess || returnedSlotId != slotId) {
                PushLog("XHCI EPCONF AD FAIL");
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }
        }

        if (passed) {
            volatile uint32_t *ep0Words = reinterpret_cast<volatile uint32_t *>(ep0Ring.VirtualAddress);

            ep0Words[0] = static_cast<uint32_t>(requestedConfigValue) << 16u;
            ep0Words[0] |= 0x00000900u; // bmRequestType=0x00, bRequest=0x09
            ep0Words[1] = 0; // wIndex=0x0000, wLength=0
            ep0Words[2] = 8u;
            ep0Words[3] = (TrbTypeSetupStage << 10u) | (1u << 6u) | (1u << 4u) | 1u;

            ep0Words[4] = 0;
            ep0Words[5] = 0;
            ep0Words[6] = 0;
            ep0Words[7] = (TrbTypeStatusStage << 10u) | (1u << 5u) | 1u;

            regs.RingDoorbell(static_cast<uint8_t>(slotId), 1u, 0u);

            uint32_t completionCode = 0;
            uint32_t endpointIdReturned = 0;
            uint32_t eventIndex = 0;
            if (!WaitForTransferCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                           256u,
                                           ep0Ring.PhysicalAddress,
                                           ep0Ring.PhysicalAddress + 16ull,
                                           ep0Ring.PhysicalAddress + 32ull,
                                           slotId,
                                           EndpointIdControl,
                                           MaxPolls,
                                           completionCode,
                                           endpointIdReturned,
                                           eventIndex)) {
                PushLog("XHCI EPCONF SETCFG NOEVENT");
                LogEventRingDebug(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress), 256u, 8u);
                passed = false;
            } else if (completionCode != CcSuccess || endpointIdReturned != EndpointIdControl) {
                PushLog("XHCI EPCONF SETCFG FAIL");
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }
        }

        if (passed) {
            const uint32_t hcc1 = regs.ReadHccParams1();
            const uint32_t contextSize = ((hcc1 & (1u << 2u)) != 0) ? 64u : 32u;
            volatile uint32_t *inputWords = reinterpret_cast<volatile uint32_t *>(inputContext.VirtualAddress);
            Fortress::Runtime::Memset(reinterpret_cast<void *>(inputContext.VirtualAddress), 0, static_cast<Fortress::Core::usize>(inputContext.SizeBytes));

            // Start from the controller-managed output context state so route/speed/state are preserved.
            uint8_t *inputBytesRaw = reinterpret_cast<uint8_t *>(inputContext.VirtualAddress);
            uint8_t *outputBytesRaw = reinterpret_cast<uint8_t *>(outputContext.VirtualAddress);
            const uint64_t maxOutputCopy = inputContext.SizeBytes > contextSize ? (inputContext.SizeBytes - contextSize) : 0;
            const uint64_t copyBytes = outputContext.SizeBytes < maxOutputCopy ? outputContext.SizeBytes : maxOutputCopy;
            for (uint64_t i = 0; i < copyBytes; i++) {
                inputBytesRaw[contextSize + i] = outputBytesRaw[i];
            }

            const uint32_t contextEntries = endpointId > 1u ? endpointId : 1u;
            inputWords[0] = 0;
            inputWords[1] = (1u << 0u) | (1u << endpointId);

            volatile uint8_t *inputBytes = reinterpret_cast<volatile uint8_t *>(inputContext.VirtualAddress);
            volatile uint32_t *slotCtx = reinterpret_cast<volatile uint32_t *>(inputBytes + contextSize);
            const uint32_t existingContextEntries = (slotCtx[0] >> 27u) & 0x1Fu;
            const uint32_t newContextEntries = existingContextEntries > contextEntries ? existingContextEntries : contextEntries;
            slotCtx[0] = (slotCtx[0] & ~(0x1Fu << 27u)) | (newContextEntries << 27u);

            volatile uint32_t *targetEpCtx = reinterpret_cast<volatile uint32_t *>(inputBytes + ((1u + endpointId) * contextSize));
            targetEpCtx[0] = (intervalField << 16u);
            targetEpCtx[1] = (3u << 1u) | (endpointType << 3u) | (static_cast<uint32_t>(endpointMaxPacketSize) << 16u);
            WriteLe64(targetEpCtx + 2, (endpointRing.PhysicalAddress & ~0xFull) | 1ull);
            targetEpCtx[4] = endpointMaxPacketSize | (static_cast<uint32_t>(endpointMaxPacketSize) << 16u);

            volatile uint32_t *cmdWords = reinterpret_cast<volatile uint32_t *>(cmdRing.VirtualAddress);
            volatile uint32_t *cfgEpTrb = cmdWords + 8u;
            WriteLe64(cfgEpTrb, inputContext.PhysicalAddress & ~0xFull);
            cfgEpTrb[2] = 0;
            cfgEpTrb[3] = (TrbTypeConfigureEndpoint << 10u) | (static_cast<uint32_t>(slotId) << 24u) | 1u;

            regs.RingDoorbell(0, 0, 0);

            uint32_t completionCode = 0;
            uint32_t returnedSlotId = 0;
            uint32_t eventIndex = 0;
            if (!WaitForCommandCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                          256u,
                                          cmdRing.PhysicalAddress + 32ull,
                                          MaxPolls,
                                          completionCode,
                                          returnedSlotId,
                                          eventIndex)) {
                PushLog("XHCI EPCONF CFE NOEVENT");
                passed = false;
            } else if (completionCode != CcSuccess || returnedSlotId != slotId) {
                PushLog("XHCI EPCONF CFE FAIL");
                char line[96] = {};
                size_t pos = 0;
                AppendString(line, sizeof(line), pos, "XHCI EPCONF CFE CC ");
                AppendUInt(line, sizeof(line), pos, completionCode);
                AppendString(line, sizeof(line), pos, " SLOT ");
                AppendUInt(line, sizeof(line), pos, returnedSlotId);
                PushLog(line);
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);

                char line[96] = {};
                size_t pos = 0;
                AppendString(line, sizeof(line), pos, "XHCI EPCONF EP ");
                AppendHex(line, sizeof(line), pos, endpointAddress);
                AppendString(line, sizeof(line), pos, " ID ");
                AppendUInt(line, sizeof(line), pos, endpointId);
                AppendString(line, sizeof(line), pos, " MPS ");
                AppendUInt(line, sizeof(line), pos, endpointMaxPacketSize);
                PushLog(line);
            }
        }
    }

    {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        (void)WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls);
    }

    bool cleanupOk = true;
    if (haveEndpointRing && !FDmaMemoryManager::FreeBuffer(endpointRing)) {
        cleanupOk = false;
    }
    if (haveEp0Ring && !FDmaMemoryManager::FreeBuffer(ep0Ring)) {
        cleanupOk = false;
    }
    if (haveErst && !FDmaMemoryManager::FreeBuffer(erst)) {
        cleanupOk = false;
    }
    if (haveEvtRing && !FDmaMemoryManager::FreeBuffer(evtRing)) {
        cleanupOk = false;
    }
    if (haveCmdRing && !FDmaMemoryManager::FreeBuffer(cmdRing)) {
        cleanupOk = false;
    }
    if (haveInputContext && !FDmaMemoryManager::FreeBuffer(inputContext)) {
        cleanupOk = false;
    }
    if (haveOutputContext && !FDmaMemoryManager::FreeBuffer(outputContext)) {
        cleanupOk = false;
    }
    if (haveDcbaa && !FDmaMemoryManager::FreeBuffer(dcbaa)) {
        cleanupOk = false;
    }
    if (!FPinnedMappingManager::UnmapPhysicalRange(mmio)) {
        cleanupOk = false;
    }

    if (!cleanupOk) {
        PushLog("XHCI EPCONF CLEANUP FAIL");
    }

    PushLog(passed ? "XHCI EPCONF PASS" : "XHCI EPCONF FAIL");
}

static void RunXhciInterruptIn(bool loopMode, uint64_t maxPollsOverride, bool quietBackgroundMode);

static void RunXhciInterruptInCommand(bool loopMode) {
    RunXhciInterruptIn(loopMode, 0ull, false);
}

static void RunXhciInterruptIn(bool loopMode, uint64_t maxPollsOverride = 0ull, bool quietBackgroundMode = false) {
    if (!quietBackgroundMode) {
        PushLog(loopMode ? "XHCI INTRINLOOP START" : "XHCI INTRIN START");
    }

    // Keep cursor mapping stable on hardware by avoiding implicit activation of
    // sampled auto-ranges that may represent only a local subset of movement.

    const uint8_t requestedConfigValue = GHaveLastUsbConfigValue ? GLastUsbConfigValue : 1u;
    if (!GHaveLastUsbConfigValue && !quietBackgroundMode) {
        PushLog(loopMode ? "XHCI INTRINLOOP CFG DEFAULT 1" : "XHCI INTRIN CFG DEFAULT 1");
    }

    const bool preferKeyboardEndpoint = quietBackgroundMode;
    uint8_t endpointAddress = GLastUsbInterruptInEndpointAddress;
    uint16_t endpointMaxPacketSize = GLastUsbInterruptInMaxPacketSize;
    uint8_t endpointInterval = GLastUsbInterruptInInterval;
    EUsbIntrinEndpointKind endpointKind = GLastUsbIntrinEndpointKind;
    if (preferKeyboardEndpoint && GHaveLastUsbKeyboardInterruptInEndpoint) {
        endpointAddress = GLastUsbKeyboardInterruptInEndpointAddress;
        endpointMaxPacketSize = GLastUsbKeyboardInterruptInMaxPacketSize;
        endpointInterval = GLastUsbKeyboardInterruptInInterval;
        endpointKind = EUsbIntrinEndpointKind::KeyboardBoot;
    } else if (!preferKeyboardEndpoint && GHaveLastUsbMouseInterruptInEndpoint) {
        endpointAddress = GLastUsbMouseInterruptInEndpointAddress;
        endpointMaxPacketSize = GLastUsbMouseInterruptInMaxPacketSize;
        endpointInterval = GLastUsbMouseInterruptInInterval;
        endpointKind = EUsbIntrinEndpointKind::MouseBoot;
    }
    if (!GHaveLastUsbInterruptInEndpoint) {
        endpointAddress = 0x81u;
        endpointMaxPacketSize = 8u;
        endpointInterval = 4u;
        endpointKind = EUsbIntrinEndpointKind::Generic;
        if (!quietBackgroundMode) {
            PushLog(loopMode ? "XHCI INTRINLOOP EP DEFAULT 0x81" : "XHCI INTRIN EP DEFAULT 0x81");
        }
    }

    if (endpointMaxPacketSize == 0) {
        endpointMaxPacketSize = 8u;
    }

    if (!quietBackgroundMode) {
        char endpointLine[96] = {};
        size_t endpointPos = 0;
        AppendString(endpointLine, sizeof(endpointLine), endpointPos, loopMode ? "XHCI INTRINLOOP EPSEL " : "XHCI INTRIN EPSEL ");
        if (endpointKind == EUsbIntrinEndpointKind::KeyboardBoot) {
            AppendString(endpointLine, sizeof(endpointLine), endpointPos, "KBD ");
        } else if (endpointKind == EUsbIntrinEndpointKind::MouseBoot) {
            AppendString(endpointLine, sizeof(endpointLine), endpointPos, "MOUSE ");
        } else {
            AppendString(endpointLine, sizeof(endpointLine), endpointPos, "GEN ");
        }
        AppendHex(endpointLine, sizeof(endpointLine), endpointPos, endpointAddress);
        AppendString(endpointLine, sizeof(endpointLine), endpointPos, " MPS ");
        AppendUInt(endpointLine, sizeof(endpointLine), endpointPos, endpointMaxPacketSize);
        PushLog(endpointLine);
    }

    const uint8_t endpointNumber = endpointAddress & 0xFu;
    const bool endpointIn = (endpointAddress & 0x80u) != 0;
    const uint32_t endpointId = static_cast<uint32_t>(endpointNumber) * 2u + (endpointIn ? 1u : 0u);
    const uint32_t endpointType = endpointIn ? 7u : 3u;
    const uint32_t intervalField = endpointInterval == 0 ? 0u : static_cast<uint32_t>(endpointInterval - 1u);

    FXhciControllerInfo info{};
    if (!FXhciPciDiscovery::DiscoverFirst(info) || !info.Found) {
        PushLog(loopMode ? "XHCI INTRINLOOP NONE" : "XHCI INTRIN NONE");
        return;
    }

    FPinnedMapping mmio{};
    if (!FPinnedMappingManager::MapPhysicalRange(info.MmioBase,
                                                 info.MmioSize,
                                                 FVirtualMemoryManager::FlagsDeviceRWUCNX,
                                                 mmio)) {
        PushLog(loopMode ? "XHCI INTRINLOOP MAP FAIL" : "XHCI INTRIN MAP FAIL");
        return;
    }

    FXhciMmioRegisters regs;
    if (!regs.Initialize(mmio.VirtualAddress)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog(loopMode ? "XHCI INTRINLOOP REGS FAIL" : "XHCI INTRIN REGS FAIL");
        return;
    }

    const uint64_t mappedVirtualBase = mmio.VirtualAddress - (info.MmioBase - (info.MmioBase & ~(FVirtualMemoryManager::PageSize - 1ull)));
    const uint64_t mappedSizeBytes = mmio.PageCount * FVirtualMemoryManager::PageSize;
    if (!IsXhciRegisterWindowSafe(regs, mappedVirtualBase, mappedSizeBytes)) {
        (void)FPinnedMappingManager::UnmapPhysicalRange(mmio);
        PushLog(loopMode ? "XHCI INTRINLOOP MMIO WINDOW FAIL" : "XHCI INTRIN MMIO WINDOW FAIL");
        return;
    }

    constexpr uint32_t UsbCmdRunStop = 1u << 0;
    constexpr uint32_t UsbCmdHostControllerReset = 1u << 1;
    constexpr uint32_t UsbCmdInterruptEnable = 1u << 2;
    constexpr uint32_t UsbStsHostControllerHalted = 1u << 0;
    constexpr uint32_t UsbStsControllerNotReady = 1u << 11;
    const uint64_t MaxPolls = (maxPollsOverride == 0ull) ? 3000000ull : maxPollsOverride;

    constexpr uint32_t TrbTypeNormal = 1u;
    constexpr uint32_t TrbTypeLink = 6u;
    constexpr uint32_t TrbTypeEnableSlot = 9u;
    constexpr uint32_t TrbTypeAddressDevice = 11u;
    constexpr uint32_t TrbTypeConfigureEndpoint = 12u;
    constexpr uint32_t TrbTypeSetupStage = 2u;
    constexpr uint32_t TrbTypeDataStage = 3u;
    constexpr uint32_t TrbTypeStatusStage = 4u;

    constexpr uint32_t CcSuccess = 1u;
    constexpr uint32_t CcShortPacket = 13u;
    constexpr uint32_t EndpointIdControl = 1u;

    FDmaBuffer dcbaa{};
    FDmaBuffer outputContext{};
    FDmaBuffer inputContext{};
    FDmaBuffer cmdRing{};
    FDmaBuffer evtRing{};
    FDmaBuffer erst{};
    FDmaBuffer ep0Ring{};
    FDmaBuffer endpointRing{};
    FDmaBuffer transferBuffer{};

    bool haveDcbaa = false;
    bool haveOutputContext = false;
    bool haveInputContext = false;
    bool haveCmdRing = false;
    bool haveEvtRing = false;
    bool haveErst = false;
    bool haveEp0Ring = false;
    bool haveEndpointRing = false;
    bool haveTransferBuffer = false;

    bool passed = true;
    uint64_t polls = 0;

    if (!WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog(loopMode ? "XHCI INTRINLOOP CNR TIMEOUT" : "XHCI INTRIN CNR TIMEOUT");
        passed = false;
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls)) {
            PushLog(loopMode ? "XHCI INTRINLOOP HALT TIMEOUT" : "XHCI INTRIN HALT TIMEOUT");
            passed = false;
        }
    }

    if (passed) {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdHostControllerReset);
        if (!WaitForUsbCmdBit(regs, UsbCmdHostControllerReset, false, MaxPolls, polls)) {
            PushLog(loopMode ? "XHCI INTRINLOOP RESET TIMEOUT" : "XHCI INTRIN RESET TIMEOUT");
            passed = false;
        }
    }

    if (passed && !WaitForUsbStsBit(regs, UsbStsControllerNotReady, false, MaxPolls, polls)) {
        PushLog(loopMode ? "XHCI INTRINLOOP READY TIMEOUT" : "XHCI INTRIN READY TIMEOUT");
        passed = false;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, dcbaa)) {
        PushLog(loopMode ? "XHCI INTRINLOOP DCBAA FAIL" : "XHCI INTRIN DCBAA FAIL");
        passed = false;
    } else if (passed) {
        haveDcbaa = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, outputContext)) {
        PushLog(loopMode ? "XHCI INTRINLOOP OUTCTX FAIL" : "XHCI INTRIN OUTCTX FAIL");
        passed = false;
    } else if (passed) {
        haveOutputContext = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, inputContext)) {
        PushLog(loopMode ? "XHCI INTRINLOOP INCTX FAIL" : "XHCI INTRIN INCTX FAIL");
        passed = false;
    } else if (passed) {
        haveInputContext = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, cmdRing)) {
        PushLog(loopMode ? "XHCI INTRINLOOP CR FAIL" : "XHCI INTRIN CR FAIL");
        passed = false;
    } else if (passed) {
        haveCmdRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, evtRing)) {
        PushLog(loopMode ? "XHCI INTRINLOOP ER FAIL" : "XHCI INTRIN ER FAIL");
        passed = false;
    } else if (passed) {
        haveEvtRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, erst)) {
        PushLog(loopMode ? "XHCI INTRINLOOP ERST FAIL" : "XHCI INTRIN ERST FAIL");
        passed = false;
    } else if (passed) {
        haveErst = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, ep0Ring)) {
        PushLog(loopMode ? "XHCI INTRINLOOP EP0RING FAIL" : "XHCI INTRIN EP0RING FAIL");
        passed = false;
    } else if (passed) {
        haveEp0Ring = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, endpointRing)) {
        PushLog(loopMode ? "XHCI INTRINLOOP EPRING FAIL" : "XHCI INTRIN EPRING FAIL");
        passed = false;
    } else if (passed) {
        haveEndpointRing = true;
    }

    if (passed && !FDmaMemoryManager::AllocateBuffer(4096, 64, true, transferBuffer)) {
        PushLog(loopMode ? "XHCI INTRINLOOP TBUF FAIL" : "XHCI INTRIN TBUF FAIL");
        passed = false;
    } else if (passed) {
        haveTransferBuffer = true;
    }

    uint8_t connectedPort = 0;
    uint32_t connectedPortSc = 0;
    if (passed) {
        const uint32_t hcs1 = regs.ReadHcsParams1();
        const uint32_t maxPorts = (hcs1 >> 24u) & 0xFFu;
        for (uint8_t port = 1; port <= maxPorts; port++) {
            const uint32_t portSc = regs.ReadPortSc(port);
            if ((portSc & 0x1u) != 0) {
                connectedPort = port;
                connectedPortSc = portSc;
                break;
            }
        }

        if (connectedPort == 0) {
            PushLog(loopMode ? "XHCI INTRINLOOP NOPORT" : "XHCI INTRIN NOPORT");
            passed = false;
        }
    }

    uint32_t slotId = 0;
    if (passed) {
        Fortress::Runtime::Memset(reinterpret_cast<void *>(dcbaa.VirtualAddress), 0, static_cast<Fortress::Core::usize>(dcbaa.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(outputContext.VirtualAddress), 0, static_cast<Fortress::Core::usize>(outputContext.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(inputContext.VirtualAddress), 0, static_cast<Fortress::Core::usize>(inputContext.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(cmdRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(cmdRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(evtRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(evtRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(erst.VirtualAddress), 0, static_cast<Fortress::Core::usize>(erst.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(ep0Ring.VirtualAddress), 0, static_cast<Fortress::Core::usize>(ep0Ring.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(endpointRing.VirtualAddress), 0, static_cast<Fortress::Core::usize>(endpointRing.SizeBytes));
        Fortress::Runtime::Memset(reinterpret_cast<void *>(transferBuffer.VirtualAddress), 0, static_cast<Fortress::Core::usize>(transferBuffer.SizeBytes));

        volatile uint64_t *dcbaaEntries = reinterpret_cast<volatile uint64_t *>(dcbaa.VirtualAddress);
        dcbaaEntries[0] = 0;

        volatile uint32_t *cmdWords = reinterpret_cast<volatile uint32_t *>(cmdRing.VirtualAddress);
        cmdWords[0] = 0;
        cmdWords[1] = 0;
        cmdWords[2] = 0;
        cmdWords[3] = (TrbTypeEnableSlot << 10u) | 1u;

        constexpr uint32_t CmdRingTrbCount = 256u;
        volatile uint32_t *cmdLinkWords = cmdWords + ((CmdRingTrbCount - 1u) * 4u);
        WriteLe64(cmdLinkWords, cmdRing.PhysicalAddress);
        cmdLinkWords[2] = 0;
        cmdLinkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

        constexpr uint32_t EpTrbCount = 256u;
        volatile uint32_t *ep0LinkWords = reinterpret_cast<volatile uint32_t *>(ep0Ring.VirtualAddress) + ((EpTrbCount - 1u) * 4u);
        WriteLe64(ep0LinkWords, ep0Ring.PhysicalAddress);
        ep0LinkWords[2] = 0;
        ep0LinkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

        volatile uint32_t *epLinkWords = reinterpret_cast<volatile uint32_t *>(endpointRing.VirtualAddress) + ((EpTrbCount - 1u) * 4u);
        WriteLe64(epLinkWords, endpointRing.PhysicalAddress);
        epLinkWords[2] = 0;
        epLinkWords[3] = (TrbTypeLink << 10u) | (1u << 1u) | 1u;

        volatile uint32_t *erstWords = reinterpret_cast<volatile uint32_t *>(erst.VirtualAddress);
        WriteLe64(erstWords, evtRing.PhysicalAddress);
        erstWords[2] = 256u;
        erstWords[3] = 0;

        regs.WriteConfig(8u);
        regs.WriteDcbaap(dcbaa.PhysicalAddress & ~0x3Full);
        regs.WriteCrcr((cmdRing.PhysicalAddress & ~0x3Full) | 1ull);
        regs.WriteInterrupterIman(0, 0u);
        regs.WriteInterrupterImod(0, 0u);
        regs.WriteInterrupterErstsz(0, 1u);
        regs.WriteInterrupterErstba(0, erst.PhysicalAddress & ~0x3Full);
        regs.WriteInterrupterErdp(0, evtRing.PhysicalAddress & ~0xFull);

        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd | UsbCmdRunStop | UsbCmdInterruptEnable);
        if (!WaitForUsbStsBit(regs, UsbStsHostControllerHalted, false, MaxPolls, polls)) {
            PushLog(loopMode ? "XHCI INTRINLOOP RUN TIMEOUT" : "XHCI INTRIN RUN TIMEOUT");
            passed = false;
        }

        if (passed) {
            regs.RingDoorbell(0, 0, 0);
            uint32_t completionCode = 0;
            uint32_t eventIndex = 0;
            if (!WaitForCommandCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                          256u,
                                          cmdRing.PhysicalAddress,
                                          MaxPolls,
                                          completionCode,
                                          slotId,
                                          eventIndex)) {
                PushLog(loopMode ? "XHCI INTRINLOOP ES NOEVENT" : "XHCI INTRIN ES NOEVENT");
                passed = false;
            } else if (completionCode != CcSuccess || slotId == 0) {
                PushLog(loopMode ? "XHCI INTRINLOOP ES FAIL" : "XHCI INTRIN ES FAIL");
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }
        }

        if (passed) {
            dcbaaEntries[slotId] = outputContext.PhysicalAddress & ~0x3Full;

            const uint32_t hcc1 = regs.ReadHccParams1();
            const uint32_t contextSize = ((hcc1 & (1u << 2u)) != 0) ? 64u : 32u;
            const uint32_t portSpeed = (connectedPortSc >> 10u) & 0xFu;

            uint32_t maxPacketSize0 = 8u;
            if (portSpeed == 3u) {
                maxPacketSize0 = 64u;
            } else if (portSpeed >= 4u) {
                maxPacketSize0 = 512u;
            }

            volatile uint32_t *inputWords = reinterpret_cast<volatile uint32_t *>(inputContext.VirtualAddress);
            inputWords[1] = 0x3u;

            volatile uint8_t *inputBytes = reinterpret_cast<volatile uint8_t *>(inputContext.VirtualAddress);
            volatile uint32_t *slotCtx = reinterpret_cast<volatile uint32_t *>(inputBytes + contextSize);
            volatile uint32_t *ep0Ctx = reinterpret_cast<volatile uint32_t *>(inputBytes + (2u * contextSize));

            slotCtx[0] = (portSpeed << 20u) | (1u << 27u);
            slotCtx[1] = static_cast<uint32_t>(connectedPort) << 16u;

            ep0Ctx[1] = 3u | (4u << 3u) | (maxPacketSize0 << 16u);
            WriteLe64(ep0Ctx + 2, (ep0Ring.PhysicalAddress & ~0xFull) | 1ull);

            volatile uint32_t *addrTrb = cmdWords + 4u;
            WriteLe64(addrTrb, inputContext.PhysicalAddress & ~0xFull);
            addrTrb[2] = 0;
            addrTrb[3] = (TrbTypeAddressDevice << 10u) | (static_cast<uint32_t>(slotId) << 24u) | 1u;

            regs.RingDoorbell(0, 0, 0);

            uint32_t completionCode = 0;
            uint32_t returnedSlotId = 0;
            uint32_t eventIndex = 0;
            if (!WaitForCommandCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                          256u,
                                          cmdRing.PhysicalAddress + 16ull,
                                          MaxPolls,
                                          completionCode,
                                          returnedSlotId,
                                          eventIndex)) {
                PushLog(loopMode ? "XHCI INTRINLOOP AD NOEVENT" : "XHCI INTRIN AD NOEVENT");
                passed = false;
            } else if (completionCode != CcSuccess || returnedSlotId != slotId) {
                PushLog(loopMode ? "XHCI INTRINLOOP AD FAIL" : "XHCI INTRIN AD FAIL");
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }
        }

        if (passed) {
            volatile uint32_t *ep0Words = reinterpret_cast<volatile uint32_t *>(ep0Ring.VirtualAddress);

            ep0Words[0] = static_cast<uint32_t>(requestedConfigValue) << 16u;
            ep0Words[0] |= 0x00000900u;
            ep0Words[1] = 0;
            ep0Words[2] = 8u;
            ep0Words[3] = (TrbTypeSetupStage << 10u) | (1u << 6u) | (1u << 4u) | 1u;

            ep0Words[4] = 0;
            ep0Words[5] = 0;
            ep0Words[6] = 0;
            ep0Words[7] = (TrbTypeStatusStage << 10u) | (1u << 5u) | 1u;

            regs.RingDoorbell(static_cast<uint8_t>(slotId), 1u, 0u);

            uint32_t completionCode = 0;
            uint32_t endpointIdReturned = 0;
            uint32_t eventIndex = 0;
            if (!WaitForTransferCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                           256u,
                                           ep0Ring.PhysicalAddress,
                                           ep0Ring.PhysicalAddress + 16ull,
                                           ep0Ring.PhysicalAddress + 32ull,
                                           slotId,
                                           EndpointIdControl,
                                           MaxPolls,
                                           completionCode,
                                           endpointIdReturned,
                                           eventIndex)) {
                PushLog(loopMode ? "XHCI INTRINLOOP SETCFG NOEVENT" : "XHCI INTRIN SETCFG NOEVENT");
                LogEventRingDebug(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress), 256u, 8u);
                passed = false;
            } else if (completionCode != CcSuccess || endpointIdReturned != EndpointIdControl) {
                PushLog(loopMode ? "XHCI INTRINLOOP SETCFG FAIL" : "XHCI INTRIN SETCFG FAIL");
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }
        }

        if (passed && !GHaveHidLogicalAxisRange && !GHidLogicalRangeProbeAttempted) {
            constexpr uint32_t RequestLength = 512u;
            constexpr uint64_t HidRangeProbePollBudget = 12000ull;
            constexpr uint8_t MaxInterfaceProbe = 8u;
            GHidLogicalRangeProbeAttempted = true;
            bool anyDescriptorTransfer = false;
            uint8_t probeInterfaces[MaxInterfaceProbe] = {};
            uint8_t probeCount = 0;
            if (GHaveHidLogicalRangeInterfaceHint && GHidLogicalRangeInterfaceHint < MaxInterfaceProbe) {
                probeInterfaces[probeCount++] = GHidLogicalRangeInterfaceHint;
            }
            for (uint8_t iface = 0; iface < MaxInterfaceProbe; iface++) {
                if (probeCount > 0 && probeInterfaces[0] == iface) {
                    continue;
                }
                probeInterfaces[probeCount++] = iface;
            }

            for (uint8_t p = 0; p < probeCount && !GHaveHidLogicalAxisRange; p++) {
                const uint8_t interfaceNumber = probeInterfaces[p];
                Fortress::Runtime::Memset(reinterpret_cast<void *>(transferBuffer.VirtualAddress),
                                          0,
                                          static_cast<Fortress::Core::usize>(transferBuffer.SizeBytes));

                volatile uint32_t *ep0Words = reinterpret_cast<volatile uint32_t *>(ep0Ring.VirtualAddress);
                ep0Words[0] = 0x22000681u; // bmRequestType=0x81, bRequest=0x06, wValue=0x2200 (report)
                ep0Words[1] = static_cast<uint32_t>(interfaceNumber) | ((RequestLength & 0xFFFFu) << 16u);
                ep0Words[2] = 8u;
                ep0Words[3] = (TrbTypeSetupStage << 10u) | (1u << 6u) | (1u << 4u) | (3u << 16u) | 1u;

                ep0Words[4] = static_cast<uint32_t>(transferBuffer.PhysicalAddress & 0xFFFFFFFFull);
                ep0Words[5] = static_cast<uint32_t>((transferBuffer.PhysicalAddress >> 32u) & 0xFFFFFFFFull);
                ep0Words[6] = RequestLength;
                ep0Words[7] = (TrbTypeDataStage << 10u) | (1u << 4u) | (1u << 16u) | 1u;

                ep0Words[8] = 0;
                ep0Words[9] = 0;
                ep0Words[10] = 0;
                ep0Words[11] = (TrbTypeStatusStage << 10u) | (1u << 5u) | 1u;

                regs.RingDoorbell(static_cast<uint8_t>(slotId), 1u, 0u);

                uint32_t hidCompletionCode = 0;
                uint32_t hidEndpointId = 0;
                uint32_t hidEventIndex = 0;
                if (!WaitForTransferCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                               256u,
                                               ep0Ring.PhysicalAddress,
                                               ep0Ring.PhysicalAddress + 16ull,
                                               ep0Ring.PhysicalAddress + 32ull,
                                               slotId,
                                               EndpointIdControl,
                                               HidRangeProbePollBudget,
                                               hidCompletionCode,
                                               hidEndpointId,
                                               hidEventIndex)) {
                    continue;
                }

                AckConsumedEvent(regs, evtRing.PhysicalAddress, hidEventIndex);
                if ((hidCompletionCode == CcSuccess || hidCompletionCode == CcShortPacket) &&
                    hidEndpointId == EndpointIdControl) {
                    anyDescriptorTransfer = true;
                    const bool hadRange = GHaveHidLogicalAxisRange;
                    TryUpdateHidAxisLogicalRange(
                        reinterpret_cast<volatile uint8_t *>(transferBuffer.VirtualAddress),
                        RequestLength,
                        loopMode ? "XHCI INTRINLOOP HIDRANGE" : "XHCI INTRIN HIDRANGE");
                    if (!hadRange && GHaveHidLogicalAxisRange) {
                        GHaveHidLogicalRangeInterfaceHint = true;
                        GHidLogicalRangeInterfaceHint = interfaceNumber;

                        char line[96] = {};
                        size_t pos = 0;
                        AppendString(line,
                                     sizeof(line),
                                     pos,
                                     loopMode ? "XHCI INTRINLOOP HIDRANGE IF " : "XHCI INTRIN HIDRANGE IF ");
                        AppendUInt(line, sizeof(line), pos, interfaceNumber);
                        PushLog(line);
                    }
                }
            }

            if (!GHaveHidLogicalAxisRange) {
                PushLog(anyDescriptorTransfer
                            ? (loopMode ? "XHCI INTRINLOOP HIDRANGE NOPARSE" : "XHCI INTRIN HIDRANGE NOPARSE")
                            : (loopMode ? "XHCI INTRINLOOP HIDRANGE NOXFER" : "XHCI INTRIN HIDRANGE NOXFER"));
            }
        }

        if (passed) {
            const uint32_t hcc1 = regs.ReadHccParams1();
            const uint32_t contextSize = ((hcc1 & (1u << 2u)) != 0) ? 64u : 32u;
            volatile uint32_t *inputWords = reinterpret_cast<volatile uint32_t *>(inputContext.VirtualAddress);
            Fortress::Runtime::Memset(reinterpret_cast<void *>(inputContext.VirtualAddress), 0, static_cast<Fortress::Core::usize>(inputContext.SizeBytes));

            uint8_t *inputBytesRaw = reinterpret_cast<uint8_t *>(inputContext.VirtualAddress);
            uint8_t *outputBytesRaw = reinterpret_cast<uint8_t *>(outputContext.VirtualAddress);
            const uint64_t maxOutputCopy = inputContext.SizeBytes > contextSize ? (inputContext.SizeBytes - contextSize) : 0;
            const uint64_t copyBytes = outputContext.SizeBytes < maxOutputCopy ? outputContext.SizeBytes : maxOutputCopy;
            for (uint64_t i = 0; i < copyBytes; i++) {
                inputBytesRaw[contextSize + i] = outputBytesRaw[i];
            }

            const uint32_t contextEntries = endpointId > 1u ? endpointId : 1u;
            inputWords[0] = 0;
            inputWords[1] = (1u << 0u) | (1u << endpointId);

            volatile uint8_t *inputBytes = reinterpret_cast<volatile uint8_t *>(inputContext.VirtualAddress);
            volatile uint32_t *slotCtx = reinterpret_cast<volatile uint32_t *>(inputBytes + contextSize);
            const uint32_t existingContextEntries = (slotCtx[0] >> 27u) & 0x1Fu;
            const uint32_t newContextEntries = existingContextEntries > contextEntries ? existingContextEntries : contextEntries;
            slotCtx[0] = (slotCtx[0] & ~(0x1Fu << 27u)) | (newContextEntries << 27u);

            volatile uint32_t *targetEpCtx = reinterpret_cast<volatile uint32_t *>(inputBytes + ((1u + endpointId) * contextSize));
            targetEpCtx[0] = (intervalField << 16u);
            targetEpCtx[1] = (3u << 1u) | (endpointType << 3u) | (static_cast<uint32_t>(endpointMaxPacketSize) << 16u);
            WriteLe64(targetEpCtx + 2, (endpointRing.PhysicalAddress & ~0xFull) | 1ull);
            targetEpCtx[4] = endpointMaxPacketSize | (static_cast<uint32_t>(endpointMaxPacketSize) << 16u);

            volatile uint32_t *cmdWords = reinterpret_cast<volatile uint32_t *>(cmdRing.VirtualAddress);
            volatile uint32_t *cfgEpTrb = cmdWords + 8u;
            WriteLe64(cfgEpTrb, inputContext.PhysicalAddress & ~0xFull);
            cfgEpTrb[2] = 0;
            cfgEpTrb[3] = (TrbTypeConfigureEndpoint << 10u) | (static_cast<uint32_t>(slotId) << 24u) | 1u;

            regs.RingDoorbell(0, 0, 0);

            uint32_t completionCode = 0;
            uint32_t returnedSlotId = 0;
            uint32_t eventIndex = 0;
            if (!WaitForCommandCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                          256u,
                                          cmdRing.PhysicalAddress + 32ull,
                                          MaxPolls,
                                          completionCode,
                                          returnedSlotId,
                                          eventIndex)) {
                PushLog(loopMode ? "XHCI INTRINLOOP CFE NOEVENT" : "XHCI INTRIN CFE NOEVENT");
                passed = false;
            } else if (completionCode != CcSuccess || returnedSlotId != slotId) {
                PushLog(loopMode ? "XHCI INTRINLOOP CFE FAIL" : "XHCI INTRIN CFE FAIL");
                passed = false;
            } else {
                AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
            }
        }

        if (passed) {
            constexpr uint32_t LoopIterations = 64u;
            uint8_t lastBytes[8] = {};
            bool haveLastBytes = false;
            uint32_t changedReports = 0;
            uint32_t unrelatedEventsDrained = 0;

            const uint32_t iterations = loopMode ? LoopIterations : 1u;
            for (uint32_t iter = 0; iter < iterations && passed; iter++) {
                volatile uint32_t *epWords = reinterpret_cast<volatile uint32_t *>(endpointRing.VirtualAddress);
                const uint32_t trbOffset = (iter % 128u) * 4u;
                volatile uint32_t *trbWords = epWords + trbOffset;
                trbWords[0] = static_cast<uint32_t>(transferBuffer.PhysicalAddress & 0xFFFFFFFFull);
                trbWords[1] = static_cast<uint32_t>((transferBuffer.PhysicalAddress >> 32u) & 0xFFFFFFFFull);
                trbWords[2] = endpointMaxPacketSize;
                trbWords[3] = (TrbTypeNormal << 10u) | (1u << 5u) | 1u;

                regs.RingDoorbell(static_cast<uint8_t>(slotId), static_cast<uint8_t>(endpointId), 0u);

                bool transferMatched = false;
                uint32_t completionCode = 0;
                uint32_t endpointIdReturned = 0;
                for (uint32_t drainAttempt = 0; drainAttempt < (loopMode ? 8u : 12u); drainAttempt++) {
                    uint32_t eventIndex = 0;
                    const uint64_t pollBudget = loopMode ? 50000ull : 250000ull;
                    if (!WaitForTransferCompletion(reinterpret_cast<volatile uint32_t *>(evtRing.VirtualAddress),
                                                   256u,
                                                   endpointRing.PhysicalAddress + static_cast<uint64_t>(trbOffset) * 4ull,
                                                   endpointRing.PhysicalAddress,
                                                   endpointRing.PhysicalAddress + 16ull,
                                                   slotId,
                                                   0u,
                                                   pollBudget,
                                                   completionCode,
                                                   endpointIdReturned,
                                                   eventIndex)) {
                        break;
                    }

                    AckConsumedEvent(regs, evtRing.PhysicalAddress, eventIndex);
                    if (endpointIdReturned != endpointId) {
                        unrelatedEventsDrained++;
                        continue;
                    }
                    transferMatched = true;
                    break;
                }

                if (!transferMatched) {
                    if (loopMode) {
                        YieldLongOperationFrame();
                        continue;
                    }
                    if (!quietBackgroundMode) {
                        PushLog("XHCI INTRIN IDLE");
                    }
                    if (!quietBackgroundMode && unrelatedEventsDrained > 0u) {
                        char line[96] = {};
                        size_t pos = 0;
                        AppendString(line, sizeof(line), pos, "XHCI INTRIN DRAINED ");
                        AppendUInt(line, sizeof(line), pos, unrelatedEventsDrained);
                        PushLog(line);
                    }
                    break;
                }

                if (completionCode != CcSuccess && completionCode != CcShortPacket) {
                    PushLog(loopMode ? "XHCI INTRINLOOP TD FAIL" : "XHCI INTRIN TD FAIL");
                    char line[96] = {};
                    size_t pos = 0;
                    AppendString(line, sizeof(line), pos, loopMode ? "XHCI INTRINLOOP TD CC " : "XHCI INTRIN TD CC ");
                    AppendUInt(line, sizeof(line), pos, completionCode);
                    AppendString(line, sizeof(line), pos, " EPID ");
                    AppendUInt(line, sizeof(line), pos, endpointIdReturned);
                    PushLog(line);
                    passed = false;
                    break;
                }

                volatile uint8_t *data = reinterpret_cast<volatile uint8_t *>(transferBuffer.VirtualAddress);
                const uint32_t bytesToLog = endpointMaxPacketSize < 8u ? endpointMaxPacketSize : 8u;
                uint8_t reportBytes[8] = {};
                for (uint32_t i = 0; i < bytesToLog; i++) {
                    reportBytes[i] = data[i];
                }

                bool changed = !haveLastBytes;
                if (!changed) {
                    for (uint32_t i = 0; i < bytesToLog; i++) {
                        if (lastBytes[i] != reportBytes[i]) {
                            changed = true;
                            break;
                        }
                    }
                }

                bool backgroundChanged = true;
                if (quietBackgroundMode && !loopMode && GXhciIntrinBackgroundHaveLastReport &&
                    GXhciIntrinBackgroundLastReportLength == bytesToLog) {
                    backgroundChanged = false;
                    for (uint32_t i = 0; i < bytesToLog; i++) {
                        if (GXhciIntrinBackgroundLastReport[i] != reportBytes[i]) {
                            backgroundChanged = true;
                            break;
                        }
                    }
                }

                const bool shouldLogTransfer = loopMode ? (changed || iter == 0u)
                                                         : (!quietBackgroundMode || backgroundChanged);

                if (shouldLogTransfer) {
                    char line[96] = {};
                    size_t pos = 0;
                    if ((!quietBackgroundMode && !loopMode) || IsHidVerboseMode()) {
                        AppendString(line, sizeof(line), pos, loopMode ? "XHCI INTRINLOOP TD CC " : "XHCI INTRIN TD CC ");
                        AppendUInt(line, sizeof(line), pos, completionCode);
                        PushLog(line);

                        pos = 0;
                        line[0] = '\0';
                    }

                    AppendString(line, sizeof(line), pos, loopMode ? "XHCI INTRINLOOP DATA" : "XHCI INTRIN DATA");
                    for (uint32_t i = 0; i < bytesToLog; i++) {
                        AppendString(line, sizeof(line), pos, " ");
                        AppendHex(line, sizeof(line), pos, reportBytes[i]);
                    }
                    PushLog(line);

                    if (endpointKind == EUsbIntrinEndpointKind::KeyboardBoot) {
                        ProcessUsbBootKeyboardReport(loopMode ? "XHCI INTRINLOOP KBD"
                                                              : (quietBackgroundMode ? "XHCI INTRIN BG KBD" : "XHCI INTRIN KBD"),
                                                     reportBytes,
                                                     bytesToLog);
                    } else {
                        LogHidReportDecode(loopMode ? "XHCI INTRINLOOP HID"
                                                    : (quietBackgroundMode ? "XHCI INTRIN BG HID" : "XHCI INTRIN HID"),
                                           reportBytes,
                                           bytesToLog,
                                           lastBytes,
                                           haveLastBytes,
                                           loopMode && IsHidEdgeMode());
                    }
                }

                if (quietBackgroundMode && !loopMode && bytesToLog <= sizeof(GXhciIntrinBackgroundLastReport)) {
                    for (uint32_t i = 0; i < bytesToLog; i++) {
                        GXhciIntrinBackgroundLastReport[i] = reportBytes[i];
                    }
                    GXhciIntrinBackgroundLastReportLength = bytesToLog;
                    GXhciIntrinBackgroundHaveLastReport = true;
                }

                if (changed) {
                    changedReports++;
                    for (uint32_t i = 0; i < bytesToLog; i++) {
                        lastBytes[i] = reportBytes[i];
                    }
                    haveLastBytes = true;
                }

                if (loopMode) {
                    YieldLongOperationFrame();
                }
            }

            if (loopMode && passed) {
                char line[96] = {};
                size_t pos = 0;
                AppendString(line, sizeof(line), pos, "XHCI INTRINLOOP CHANGED ");
                AppendUInt(line, sizeof(line), pos, changedReports);
                AppendString(line, sizeof(line), pos, " OF ");
                AppendUInt(line, sizeof(line), pos, LoopIterations);
                PushLog(line);

                if (unrelatedEventsDrained > 0u) {
                    pos = 0;
                    line[0] = '\0';
                    AppendString(line, sizeof(line), pos, "XHCI INTRINLOOP DRAINED ");
                    AppendUInt(line, sizeof(line), pos, unrelatedEventsDrained);
                    PushLog(line);
                }
            }
        }
    }

    {
        uint32_t cmd = regs.ReadUsbCmd();
        regs.WriteUsbCmd(cmd & ~UsbCmdRunStop);
        (void)WaitForUsbStsBit(regs, UsbStsHostControllerHalted, true, MaxPolls, polls);
    }

    bool cleanupOk = true;
    if (haveTransferBuffer && !FDmaMemoryManager::FreeBuffer(transferBuffer)) {
        cleanupOk = false;
    }
    if (haveEndpointRing && !FDmaMemoryManager::FreeBuffer(endpointRing)) {
        cleanupOk = false;
    }
    if (haveEp0Ring && !FDmaMemoryManager::FreeBuffer(ep0Ring)) {
        cleanupOk = false;
    }
    if (haveErst && !FDmaMemoryManager::FreeBuffer(erst)) {
        cleanupOk = false;
    }
    if (haveEvtRing && !FDmaMemoryManager::FreeBuffer(evtRing)) {
        cleanupOk = false;
    }
    if (haveCmdRing && !FDmaMemoryManager::FreeBuffer(cmdRing)) {
        cleanupOk = false;
    }
    if (haveInputContext && !FDmaMemoryManager::FreeBuffer(inputContext)) {
        cleanupOk = false;
    }
    if (haveOutputContext && !FDmaMemoryManager::FreeBuffer(outputContext)) {
        cleanupOk = false;
    }
    if (haveDcbaa && !FDmaMemoryManager::FreeBuffer(dcbaa)) {
        cleanupOk = false;
    }
    if (!FPinnedMappingManager::UnmapPhysicalRange(mmio)) {
        cleanupOk = false;
    }

    if (!cleanupOk) {
        PushLog(loopMode ? "XHCI INTRINLOOP CLEANUP FAIL" : "XHCI INTRIN CLEANUP FAIL");
    }

    if (loopMode && passed && !GHaveHidLogicalAxisRange && GHidAutoLogicalRangeReady) {
        PushLog("XHCI INTRINLOOP HIDRANGE AUTO READY");
    }

    if (loopMode) {
        PushLog(passed ? "XHCI INTRINLOOP PASS" : "XHCI INTRINLOOP FAIL");
    } else if (!quietBackgroundMode || !passed) {
        PushLog(passed ? "XHCI INTRIN PASS" : "XHCI INTRIN FAIL");
    }
}

static void RunXhciRegisterAutoSnapshot() {
    FXhciControllerInfo info{};
    if (!FXhciPciDiscovery::DiscoverFirst(info) || !info.Found) {
        PushLog("XHCIREGS NONE");
        return;
    }

    FPinnedMapping mapping{};
    if (!FPinnedMappingManager::MapPhysicalRange(info.MmioBase,
                                                 info.MmioSize,
                                                 FVirtualMemoryManager::FlagsDeviceRWUCNX,
                                                 mapping)) {
        PushLog("XHCIREGS MAP FAIL");
        return;
    }

    LogXhciRegisterSnapshot(mapping.VirtualAddress);

    if (!FPinnedMappingManager::UnmapPhysicalRange(mapping)) {
        PushLog("XHCIREGS UNMAP FAIL");
        return;
    }
}

static void RunXhciRegisterSnapshotFromAddress(uint64_t address) {
    if (address == 0) {
        PushLog("XHCIREGS ARG INVALID");
        return;
    }

    if (IsHigherHalfCanonicalAddress(address)) {
        if (IsVirtualRangeMapped(address, 0x80)) {
            LogXhciRegisterSnapshot(address);
        } else {
            PushLog("XHCIREGS VA UNMAPPED");
            PushLog("USE XHCIREGS OR XHCIPROBE");
        }
        return;
    }

    if (IsVirtualRangeMapped(address, 0x80)) {
        LogXhciRegisterSnapshot(address);
        return;
    }

    if (LogXhciRegisterSnapshotFromPhysical(address)) {
        return;
    }

    PushLog("XHCIREGS ARG INVALID");
    PushLog("TRY XHCIREGS OR XHCIPROBE");
}

static void CopyString(char *dst, size_t dstSize, const char *src) {
    if (dst == nullptr || dstSize == 0) {
        return;
    }

    size_t i = 0;
    for (; i + 1 < dstSize && src != nullptr && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static void PushLog(const char *line) {
    SerialMirrorLine(line);

    if (!GBootLogFrozen) {
        if (GBootLogCount < GMaxBootLogLines) {
            CopyString(GBootLogLines[GBootLogCount], sizeof(GBootLogLines[GBootLogCount]), line);
            GBootLogCount++;
        } else {
            for (size_t i = 1; i < GMaxBootLogLines; i++) {
                CopyString(GBootLogLines[i - 1], sizeof(GBootLogLines[i - 1]), GBootLogLines[i]);
            }
            CopyString(GBootLogLines[GMaxBootLogLines - 1], sizeof(GBootLogLines[GMaxBootLogLines - 1]), line);
        }
    }

    if (GLogCount < GMaxLogLines) {
        CopyString(GLogLines[GLogCount], sizeof(GLogLines[GLogCount]), line);
        GLogCount++;
        return;
    }

    for (size_t i = 1; i < GMaxLogLines; i++) {
        CopyString(GLogLines[i - 1], sizeof(GLogLines[i - 1]), GLogLines[i]);
    }
    CopyString(GLogLines[GMaxLogLines - 1], sizeof(GLogLines[GMaxLogLines - 1]), line);
}

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

static void AppendSInt(char *dst, size_t dstSize, size_t &offset, int64_t value) {
    if (value < 0) {
        AppendChar(dst, dstSize, offset, '-');
        value = -value;
    }
    AppendUInt(dst, dstSize, offset, static_cast<uint64_t>(value));
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

static uint32_t ComputeCommandHash(const char *text) {
    if (text == nullptr) {
        return 0u;
    }

    uint32_t hash = 2166136261u;
    for (size_t i = 0; text[i] != '\0'; i++) {
        hash ^= static_cast<uint8_t>(text[i]);
        hash *= 16777619u;
    }
    return hash;
}

static void PublishCommandSubmittedEvent() {
    if (GCommandLength == 0) {
        return;
    }

    const FKernelEvent commandEvent{
        .EventId = FKernelRuntimeIds::EventCommandSubmitted,
        .TopicId = FKernelRuntimeIds::TopicCommand,
        .SourceServiceId = FKernelRuntimeIds::ServiceCommandConsole,
        .Arg0 = static_cast<uint32_t>(GCommandLength),
        .Arg1 = ComputeCommandHash(GCommandBuffer),
        .Arg2 = static_cast<uint32_t>(static_cast<uint8_t>(GCommandBuffer[0])),
    };
    (void)FEventManager::Publish(commandEvent);
}

static void PublishWireframeSetEvent(bool enabled) {
    const FKernelEvent wireframeEvent{
        .EventId = FKernelRuntimeIds::EventRenderWireframeSet,
        .TopicId = FKernelRuntimeIds::TopicCommand,
        .SourceServiceId = FKernelRuntimeIds::ServiceCommandConsole,
        .Arg0 = enabled ? 1u : 0u,
        .Arg1 = 0u,
        .Arg2 = 0u,
    };
    (void)FEventManager::Publish(wireframeEvent);
}

static void PublishScenePauseSetEvent(bool paused) {
    const FKernelEvent pauseEvent{
        .EventId = FKernelRuntimeIds::EventScenePauseSet,
        .TopicId = FKernelRuntimeIds::TopicCommand,
        .SourceServiceId = FKernelRuntimeIds::ServiceCommandConsole,
        .Arg0 = paused ? 1u : 0u,
        .Arg1 = 0u,
        .Arg2 = 0u,
    };
    (void)FEventManager::Publish(pauseEvent);
}

static void PublishCursorOverlaySetEvent(bool enabled) {
    const FKernelEvent cursorOverlayEvent{
        .EventId = FKernelRuntimeIds::EventCursorOverlaySet,
        .TopicId = FKernelRuntimeIds::TopicCommand,
        .SourceServiceId = FKernelRuntimeIds::ServiceCommandConsole,
        .Arg0 = enabled ? 1u : 0u,
        .Arg1 = 0u,
        .Arg2 = 0u,
    };
    (void)FEventManager::Publish(cursorOverlayEvent);
}

static void PublishDesktopSurfaceOverlaySetEvent(bool enabled) {
    const FKernelEvent desktopSurfaceOverlayEvent{
        .EventId = FKernelRuntimeIds::EventDesktopSurfaceOverlaySet,
        .TopicId = FKernelRuntimeIds::TopicCommand,
        .SourceServiceId = FKernelRuntimeIds::ServiceCommandConsole,
        .Arg0 = enabled ? 1u : 0u,
        .Arg1 = 0u,
        .Arg2 = 0u,
    };
    (void)FEventManager::Publish(desktopSurfaceOverlayEvent);
}

static void PublishInputKeyPressedEvent(char ascii, Fortress::Core::uint32 commandLength) {
    const FKernelEvent keyEvent{
        .EventId = FKernelRuntimeIds::EventInputKeyPressed,
        .TopicId = FKernelRuntimeIds::TopicInput,
        .SourceServiceId = FKernelRuntimeIds::ServiceKeyboardInput,
        .Arg0 = static_cast<Fortress::Core::uint32>(static_cast<uint8_t>(ascii)),
        .Arg1 = commandLength,
        .Arg2 = 0u,
    };
    (void)FEventManager::Publish(keyEvent);
}

static void RunVfsStat() {
    FVirtualFileSystemStats stats{};
    FVirtualFileSystem::GetStats(stats);

    char line[96] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "VFS STAT MOUNTS ");
    AppendUInt(line, sizeof(line), pos, stats.MountCount);
    AppendString(line, sizeof(line), pos, " CAP ");
    AppendUInt(line, sizeof(line), pos, stats.Capacity);
    PushLog(line);
}

static bool EnsureServicePortAccess(uint16_t portId, uint32_t serviceId, const char *denyContext) {
    if (FPortManager::CanServiceAccessPort(portId, serviceId)) {
        return true;
    }

    FPortLease lease{};
    char line[144] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "PORT DENY ");
    AppendString(line, sizeof(line), pos, denyContext != nullptr ? denyContext : "ACCESS");
    AppendString(line, sizeof(line), pos, " P ");
    AppendUInt(line, sizeof(line), pos, portId);
    AppendString(line, sizeof(line), pos, " SID ");
    AppendUInt(line, sizeof(line), pos, serviceId);
    if (FPortManager::GetPortLease(portId, lease)) {
        AppendString(line, sizeof(line), pos, " OWNER ");
        AppendUInt(line, sizeof(line), pos, lease.ServiceId);
        AppendString(line, sizeof(line), pos, " L ");
        AppendUInt(line, sizeof(line), pos, lease.LeaseId);
    } else {
        AppendString(line, sizeof(line), pos, " (NO LEASE; TRY PORTOPEN ");
        AppendUInt(line, sizeof(line), pos, portId);
        AppendString(line, sizeof(line), pos, ")");
    }
    PushLog(line);
    return false;
}

static void RunPortList() {
    FPortRecordSnapshot ports[16] = {};
    uint32_t count = 0u;
    FPortManager::GetPorts(ports, 16u, count);

    char header[64] = {};
    size_t headerPos = 0;
    AppendString(header, sizeof(header), headerPos, "PORTLIST N ");
    AppendUInt(header, sizeof(header), headerPos, count);
    PushLog(header);

    for (uint32_t i = 0u; i < count; i++) {
        char line[160] = {};
        size_t pos = 0;
        AppendString(line, sizeof(line), pos, "PORT ");
        AppendUInt(line, sizeof(line), pos, ports[i].PortId);
        AppendString(line, sizeof(line), pos, " NAME ");
        AppendString(line, sizeof(line), pos, ports[i].Name != nullptr ? ports[i].Name : "<null>");
        AppendString(line, sizeof(line), pos, " OPEN ");
        AppendUInt(line, sizeof(line), pos, ports[i].Lease.InUse ? 1u : 0u);
        if (ports[i].Lease.InUse) {
            AppendString(line, sizeof(line), pos, " SID ");
            AppendUInt(line, sizeof(line), pos, ports[i].Lease.ServiceId);
            AppendString(line, sizeof(line), pos, " L ");
            AppendUInt(line, sizeof(line), pos, ports[i].Lease.LeaseId);
        }
        PushLog(line);
    }
}

static void RunPortOpen(uint32_t portIdRaw) {
    const uint16_t portId = static_cast<uint16_t>(portIdRaw);
    FPortLease existingLease{};
    if (FPortManager::GetPortLease(portId, existingLease)) {
        char line[112] = {};
        size_t pos = 0;
        AppendString(line, sizeof(line), pos, "PORTOPEN IN USE P ");
        AppendUInt(line, sizeof(line), pos, portId);
        AppendString(line, sizeof(line), pos, " SID ");
        AppendUInt(line, sizeof(line), pos, existingLease.ServiceId);
        AppendString(line, sizeof(line), pos, " L ");
        AppendUInt(line, sizeof(line), pos, existingLease.LeaseId);
        PushLog(line);
        return;
    }

    uint32_t leaseId = 0u;
    if (!FPortManager::OpenLease(portId, FKernelRuntimeIds::ServiceCommandConsole, "CommandConsole", leaseId)) {
        PushLog("PORTOPEN FAIL");
        return;
    }

    if (!CacheCommandPortLease(portId, leaseId)) {
        PushLog("PORTOPEN CACHE FULL");
        return;
    }

    char line[96] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "PORTOPEN OK P ");
    AppendUInt(line, sizeof(line), pos, portId);
    AppendString(line, sizeof(line), pos, " L ");
    AppendUInt(line, sizeof(line), pos, leaseId);
    PushLog(line);
}

static void RunPortClose(uint32_t portIdRaw) {
    const uint16_t portId = static_cast<uint16_t>(portIdRaw);

    uint32_t leaseId = 0u;
    if (!GetCachedCommandPortLease(portId, leaseId)) {
        FPortLease liveLease{};
        if (!FPortManager::GetPortLease(portId, liveLease) ||
            liveLease.ServiceId != FKernelRuntimeIds::ServiceCommandConsole) {
            PushLog("PORTCLOSE NO LEASE");
            return;
        }
        leaseId = liveLease.LeaseId;
    }

    if (!FPortManager::CloseLease(portId, FKernelRuntimeIds::ServiceCommandConsole, leaseId)) {
        PushLog("PORTCLOSE FAIL");
        return;
    }

    DropCachedCommandPortLease(portId);
    char line[96] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "PORTCLOSE OK P ");
    AppendUInt(line, sizeof(line), pos, portId);
    AppendString(line, sizeof(line), pos, " L ");
    AppendUInt(line, sizeof(line), pos, leaseId);
    PushLog(line);
}

static void RunPortLease(uint32_t portIdRaw) {
    const uint16_t portId = static_cast<uint16_t>(portIdRaw);

    const char *portName = nullptr;
    const bool haveName = FPortManager::GetPortName(portId, portName);
    if (!haveName) {
        PushLog("PORTLEASE PORT UNKNOWN");
        return;
    }

    FPortLease lease{};
    if (!FPortManager::GetPortLease(portId, lease)) {
        char line[128] = {};
        size_t pos = 0;
        AppendString(line, sizeof(line), pos, "PORTLEASE P ");
        AppendUInt(line, sizeof(line), pos, portId);
        AppendString(line, sizeof(line), pos, " NAME ");
        AppendString(line, sizeof(line), pos, portName != nullptr ? portName : "<null>");
        AppendString(line, sizeof(line), pos, " OPEN 0");
        PushLog(line);
        return;
    }

    char line[160] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "PORTLEASE P ");
    AppendUInt(line, sizeof(line), pos, portId);
    AppendString(line, sizeof(line), pos, " NAME ");
    AppendString(line, sizeof(line), pos, portName != nullptr ? portName : "<null>");
    AppendString(line, sizeof(line), pos, " OPEN 1 SID ");
    AppendUInt(line, sizeof(line), pos, lease.ServiceId);
    AppendString(line, sizeof(line), pos, " L ");
    AppendUInt(line, sizeof(line), pos, lease.LeaseId);
    PushLog(line);
}

static void RunVfsResolve(const char *absolutePath) {
    if (absolutePath != nullptr && StartsWith(absolutePath, "/boot") &&
        !EnsureServicePortAccess(FKernelRuntimeIds::PortBootVolume,
                                 FKernelRuntimeIds::ServiceVirtualFileSystem,
                                 "VFSRESOLVE")) {
        return;
    }

    if (absolutePath == nullptr || absolutePath[0] != '/') {
        PushLog("VFS RESOLVE ARG INVALID");
        return;
    }

    FVirtualFileSystemRoute route{};
    if (!FVirtualFileSystem::ResolvePath(absolutePath, route) || !route.Found) {
        PushLog("VFS RESOLVE MISS");
        return;
    }

    char line[160] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "VFS RESOLVE ");
    AppendString(line, sizeof(line), pos, absolutePath);
    AppendString(line, sizeof(line), pos, " -> ");
    AppendString(line, sizeof(line), pos, route.DriverName);
    AppendString(line, sizeof(line), pos, ":");
    AppendString(line, sizeof(line), pos, route.RelativePath);
    AppendString(line, sizeof(line), pos, " RO ");
    AppendUInt(line, sizeof(line), pos, route.ReadOnly ? 1u : 0u);
    PushLog(line);
}

static void RunServiceDbStats() {
    FServiceRegistryDatabaseAdapterStats stats{};
    FServiceRegistryDatabaseAdapter::GetStats(stats);

    char line[112] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "SRDB REC ");
    AppendUInt(line, sizeof(line), pos, stats.RecordCount);
    AppendString(line, sizeof(line), pos, " REF ");
    AppendUInt(line, sizeof(line), pos, stats.RefreshCount);
    AppendString(line, sizeof(line), pos, " MISS ");
    AppendUInt(line, sizeof(line), pos, stats.FailedLookupCount);
    PushLog(line);
}

static void RunServiceDbFind(uint32_t serviceId) {
    FServiceRegistryDatabaseRecord record{};
    if (!FServiceRegistryDatabaseAdapter::FindRecordByServiceId(serviceId, record)) {
        PushLog("SRDB FIND MISS");
        return;
    }

    char line[128] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "SRDB SID ");
    AppendUInt(line, sizeof(line), pos, record.ServiceId);
    AppendString(line, sizeof(line), pos, " CH ");
    AppendUInt(line, sizeof(line), pos, record.EndpointChannelId);
    AppendString(line, sizeof(line), pos, " NAME ");
    AppendString(line, sizeof(line), pos, record.Name != nullptr ? record.Name : "<null>");
    PushLog(line);
}

static void RunPortPolicyStats() {
    FPortManagerStats stats{};
    FPortManager::GetStats(stats);

    char line[128] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "PORT REG ");
    AppendUInt(line, sizeof(line), pos, stats.RegisteredPortCount);
    AppendString(line, sizeof(line), pos, " OPEN ");
    AppendUInt(line, sizeof(line), pos, stats.ActiveLeaseCount);
    AppendString(line, sizeof(line), pos, " DENY ");
    AppendUInt(line, sizeof(line), pos, stats.DeniedAccessCount);
    AppendString(line, sizeof(line), pos, " O ");
    AppendUInt(line, sizeof(line), pos, stats.LeaseOpenCount);
    AppendString(line, sizeof(line), pos, " C ");
    AppendUInt(line, sizeof(line), pos, stats.LeaseCloseCount);
    PushLog(line);
}

static void LogPortAuditEntry(const char *prefix, const FPortAuditEntry &entry) {
    char line[144] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, prefix);
    AppendString(line, sizeof(line), pos, " P ");
    AppendUInt(line, sizeof(line), pos, entry.PortId);
    AppendString(line, sizeof(line), pos, " S ");
    AppendUInt(line, sizeof(line), pos, entry.ServiceId);
    AppendString(line, sizeof(line), pos, " L ");
    AppendUInt(line, sizeof(line), pos, entry.LeaseId);
    AppendString(line, sizeof(line), pos, " A ");
    AppendUInt(line, sizeof(line), pos, static_cast<uint64_t>(entry.Action));
    AppendString(line, sizeof(line), pos, " R ");
    AppendUInt(line, sizeof(line), pos, static_cast<uint64_t>(entry.Result));
    PushLog(line);
}

static void RunPortAuditLast() {
    FPortAuditEntry entry{};
    if (!FPortManager::GetLastAuditEntry(entry)) {
        PushLog("PORTAUDIT NONE");
        return;
    }

    LogPortAuditEntry("PORTAUDIT LAST", entry);
}

static void RunPortAuditDenied() {
    FPortAuditEntry entry{};
    if (!FPortManager::GetLastDeniedAuditEntry(entry)) {
        PushLog("PORTAUDIT DENY NONE");
        return;
    }

    LogPortAuditEntry("PORTAUDIT DENY", entry);
}

static void RunPortPolicyCheck(uint32_t portId, uint32_t serviceId) {
    const bool allowed = FPortManager::CanServiceAccessPort(static_cast<uint16_t>(portId), serviceId);
    char line[96] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "PORTCHECK P ");
    AppendUInt(line, sizeof(line), pos, portId);
    AppendString(line, sizeof(line), pos, " S ");
    AppendUInt(line, sizeof(line), pos, serviceId);
    AppendString(line, sizeof(line), pos, " -> ");
    AppendString(line, sizeof(line), pos, allowed ? "ALLOW" : "DENY");
    PushLog(line);
}

static void RunDesktopZList() {
    if (GBoundDesktopCompositor == nullptr || !GBoundDesktopCompositor->IsReady()) {
        PushLog("DSKZLIST COMPOSITOR UNBOUND");
        return;
    }

    FDesktopSurfaceId surfaceIds[32] = {};
    uint32_t count = 0u;
    GBoundDesktopCompositor->GetSurfacesInZOrder(surfaceIds, 32u, count);

    char line[160] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "DSK ZLIST N ");
    AppendUInt(line, sizeof(line), pos, count);
    for (uint32_t i = 0u; i < count; i++) {
        AppendString(line, sizeof(line), pos, " ");
        AppendUInt(line, sizeof(line), pos, surfaceIds[i]);
    }
    PushLog(line);
}

static void RunDesktopChildren(uint32_t parentSurfaceId) {
    if (GBoundDesktopCompositor == nullptr || !GBoundDesktopCompositor->IsReady()) {
        PushLog("DSKCHILDREN COMPOSITOR UNBOUND");
        return;
    }

    FDesktopSurfaceId childIds[32] = {};
    uint32_t count = 0u;
    GBoundDesktopCompositor->GetChildSurfaceIds(static_cast<FDesktopSurfaceId>(parentSurfaceId),
                                                childIds,
                                                32u,
                                                count);

    char line[160] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "DSK CHILDREN P ");
    AppendUInt(line, sizeof(line), pos, parentSurfaceId);
    AppendString(line, sizeof(line), pos, " N ");
    AppendUInt(line, sizeof(line), pos, count);
    PushLog(line);
}

static void RunXhciMscStatus() {
    if (!GHaveLastUsbMscBulkPair) {
        PushLog("XHCI MSC NONE");
        return;
    }

    char line[160] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "XHCI MSC IF ");
    AppendUInt(line, sizeof(line), pos, GLastUsbMscInterfaceNumber);
    AppendString(line, sizeof(line), pos, " IN ");
    AppendHex(line, sizeof(line), pos, GLastUsbMscBulkInEndpointAddress);
    AppendString(line, sizeof(line), pos, " OUT ");
    AppendHex(line, sizeof(line), pos, GLastUsbMscBulkOutEndpointAddress);
    AppendString(line, sizeof(line), pos, " INMPS ");
    AppendUInt(line, sizeof(line), pos, GLastUsbMscBulkInMaxPacketSize);
    AppendString(line, sizeof(line), pos, " OUTMPS ");
    AppendUInt(line, sizeof(line), pos, GLastUsbMscBulkOutMaxPacketSize);
    PushLog(line);
}

struct FLogExportCursor {
    uint8_t Section = 0u;
    size_t LineIndex = 0u;
    size_t CharIndex = 0u;
    bool EmitNewline = false;
};

static bool NextLogExportByte(FLogExportCursor &cursor, uint8_t &outByte) {
    for (;;) {
        if (cursor.Section >= 2u) {
            return false;
        }

        const bool bootSection = cursor.Section == 0u;
        const size_t lineCount = bootSection ? GBootLogCount : GLogCount;
        const char (*lines)[96] = bootSection ? GBootLogLines : GLogLines;

        if (cursor.LineIndex >= lineCount) {
            cursor.Section++;
            cursor.LineIndex = 0u;
            cursor.CharIndex = 0u;
            cursor.EmitNewline = false;
            continue;
        }

        const char *line = lines[cursor.LineIndex];
        if (!cursor.EmitNewline) {
            const char c = line[cursor.CharIndex];
            if (c != '\0') {
                outByte = static_cast<uint8_t>(c);
                cursor.CharIndex++;
                return true;
            }
            cursor.EmitNewline = true;
        }

        outByte = static_cast<uint8_t>('\n');
        cursor.LineIndex++;
        cursor.CharIndex = 0u;
        cursor.EmitNewline = false;
        return true;
    }
}

static void RunLogSave(const char *args) {
    if (args == nullptr || args[0] == '\0') {
        PushLog("LOGSAVE USAGE /MOUNT START_BLOCK BLOCK_COUNT");
        return;
    }

    const char *cursor = args;
    char mountPathToken[48] = {};
    char startBlockToken[24] = {};
    char blockCountToken[24] = {};
    if (!ReadToken(cursor, mountPathToken, sizeof(mountPathToken)) ||
        !ReadToken(cursor, startBlockToken, sizeof(startBlockToken)) ||
        !ReadToken(cursor, blockCountToken, sizeof(blockCountToken))) {
        PushLog("LOGSAVE USAGE /MOUNT START_BLOCK BLOCK_COUNT");
        return;
    }

    char extraToken[8] = {};
    if (ReadToken(cursor, extraToken, sizeof(extraToken))) {
        PushLog("LOGSAVE TOO MANY ARGS");
        return;
    }

    if (mountPathToken[0] != '/') {
        PushLog("LOGSAVE MOUNT INVALID");
        return;
    }

    uint64_t startBlock = 0u;
    uint64_t blockCount = 0u;
    if (!ParseUInt(startBlockToken, startBlock) || !ParseUInt(blockCountToken, blockCount)) {
        PushLog("LOGSAVE ARG INVALID");
        return;
    }

    if (blockCount == 0u || blockCount > 4096u) {
        PushLog("LOGSAVE BLOCK_COUNT RANGE 1..4096");
        return;
    }

    size_t mountLen = 0u;
    while (mountPathToken[mountLen] != '\0') {
        mountLen++;
    }
    while (mountLen > 1u && mountPathToken[mountLen - 1u] == '/') {
        mountPathToken[mountLen - 1u] = '\0';
        mountLen--;
    }

    if (StartsWith(mountPathToken, "/boot") &&
        !EnsureServicePortAccess(FKernelRuntimeIds::PortBootVolume,
                                 FKernelRuntimeIds::ServiceVirtualFileSystem,
                                 "LOGSAVE")) {
        return;
    }

    char probePath[96] = {};
    size_t probePos = 0u;
    AppendString(probePath, sizeof(probePath), probePos, mountPathToken);
    AppendString(probePath, sizeof(probePath), probePos, "/blk/0");

    FVirtualFileSystemRoute route{};
    if (!FVirtualFileSystem::ResolvePath(probePath, route) || !route.Found || route.Device == nullptr) {
        PushLog("LOGSAVE ROUTE MISS");
        return;
    }
    if (route.ReadOnly) {
        PushLog("LOGSAVE ROUTE READONLY");
        return;
    }

    const uint32_t blockSize = route.Device->GetBlockSizeBytes();
    if (blockSize == 0u || blockSize > 4096u) {
        PushLog("LOGSAVE BLOCKSIZE UNSUPPORTED");
        return;
    }

    uint8_t blockBuffer[4096] = {};
    FLogExportCursor exportCursor{};
    uint64_t totalBytesWritten = 0u;
    uint64_t blocksWritten = 0u;

    for (uint64_t blockOffset = 0u; blockOffset < blockCount; blockOffset++) {
        Fortress::Runtime::Memset(blockBuffer, 0, static_cast<Fortress::Core::usize>(blockSize));

        uint32_t bytesFilled = 0u;
        uint8_t nextByte = 0u;
        while (bytesFilled < blockSize && NextLogExportByte(exportCursor, nextByte)) {
            blockBuffer[bytesFilled++] = nextByte;
            totalBytesWritten++;
        }

        if (bytesFilled == 0u && blockOffset > 0u) {
            break;
        }

        char blockPath[96] = {};
        size_t blockPos = 0u;
        AppendString(blockPath, sizeof(blockPath), blockPos, mountPathToken);
        AppendString(blockPath, sizeof(blockPath), blockPos, "/blk/");
        AppendUInt(blockPath, sizeof(blockPath), blockPos, startBlock + blockOffset);

        uint32_t writtenBytes = 0u;
        if (!FVirtualFileSystem::WriteFile(blockPath, blockBuffer, blockSize, writtenBytes) || writtenBytes != blockSize) {
            PushLog("LOGSAVE WRITE FAIL");
            return;
        }

        blocksWritten++;

        if (bytesFilled < blockSize) {
            break;
        }
    }

    if (blocksWritten == 0u) {
        PushLog("LOGSAVE NO DATA");
        return;
    }

    char line[160] = {};
    size_t pos = 0u;
    AppendString(line, sizeof(line), pos, "LOGSAVE OK MOUNT ");
    AppendString(line, sizeof(line), pos, mountPathToken);
    AppendString(line, sizeof(line), pos, " START ");
    AppendUInt(line, sizeof(line), pos, startBlock);
    AppendString(line, sizeof(line), pos, " BLKS ");
    AppendUInt(line, sizeof(line), pos, blocksWritten);
    AppendString(line, sizeof(line), pos, " BYTES ");
    AppendUInt(line, sizeof(line), pos, totalBytesWritten);
    PushLog(line);
}

static void RunLogSaveBoot(const char *args) {
    if (args == nullptr || args[0] == '\0') {
        PushLog("LOGSAVEBOOT USAGE START_BLOCK BLOCK_COUNT");
        return;
    }

    const char *cursor = args;
    char startBlockToken[24] = {};
    char blockCountToken[24] = {};
    if (!ReadToken(cursor, startBlockToken, sizeof(startBlockToken)) ||
        !ReadToken(cursor, blockCountToken, sizeof(blockCountToken))) {
        PushLog("LOGSAVEBOOT USAGE START_BLOCK BLOCK_COUNT");
        return;
    }

    char extraToken[8] = {};
    if (ReadToken(cursor, extraToken, sizeof(extraToken))) {
        PushLog("LOGSAVEBOOT TOO MANY ARGS");
        return;
    }

    char mappedArgs[96] = {};
    size_t pos = 0u;
    AppendString(mappedArgs, sizeof(mappedArgs), pos, "/boot ");
    AppendString(mappedArgs, sizeof(mappedArgs), pos, startBlockToken);
    AppendString(mappedArgs, sizeof(mappedArgs), pos, " ");
    AppendString(mappedArgs, sizeof(mappedArgs), pos, blockCountToken);
    RunLogSave(mappedArgs);
}

static void RunEventBurst(uint32_t count) {
    if (count == 0u) {
        PushLog("EVENTBURST COUNT INVALID");
        return;
    }

    FServiceRegistrationInfo inputService{};
    if (!FServiceRegistry::FindServiceById(FKernelRuntimeIds::ServiceKeyboardInput, inputService)) {
        PushLog("EVENTBURST INPUT SERVICE MISSING");
        return;
    }

    uint32_t publishedCount = 0u;
    uint32_t droppedCount = 0u;
    for (uint32_t i = 0; i < count; i++) {
        const FKernelEvent burstEvent{
            .EventId = FKernelRuntimeIds::EventInputKeyPressed,
            .TopicId = FKernelRuntimeIds::TopicInput,
            .SourceServiceId = FKernelRuntimeIds::ServiceKeyboardInput,
            .Arg0 = static_cast<uint32_t>('A' + (i % 26u)),
            .Arg1 = i,
            .Arg2 = 1u,
        };

        if (FEventManager::Publish(burstEvent)) {
            publishedCount++;
        } else {
            droppedCount++;
        }

        if ((i % 1024u) == 0u) {
            YieldLongOperationFrame();
        }
    }

    char line[96] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "EVENTBURST REQ ");
    AppendUInt(line, sizeof(line), pos, count);
    AppendString(line, sizeof(line), pos, " CH ");
    AppendUInt(line, sizeof(line), pos, inputService.EndpointChannelId);
    AppendString(line, sizeof(line), pos, " PUB ");
    AppendUInt(line, sizeof(line), pos, publishedCount);
    AppendString(line, sizeof(line), pos, " DROP ");
    AppendUInt(line, sizeof(line), pos, droppedCount);
    PushLog(line);

    FMessageBusStats busStats{};
    FMessageBus::GetStats(busStats);
    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "EVENTBURST BUS Q ");
    AppendUInt(line, sizeof(line), pos, busStats.QueueDepth);
    AppendString(line, sizeof(line), pos, " DROP ");
    AppendUInt(line, sizeof(line), pos, busStats.DroppedCount);
    PushLog(line);

    FEventManagerStats eventStats{};
    FEventManager::GetStats(eventStats);
    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "EVENTBURST EVT PUB ");
    AppendUInt(line, sizeof(line), pos, eventStats.PublishedCount);
    AppendString(line, sizeof(line), pos, " DSP ");
    AppendUInt(line, sizeof(line), pos, eventStats.DispatchedCount);
    AppendString(line, sizeof(line), pos, " DROP ");
    AppendUInt(line, sizeof(line), pos, eventStats.DroppedCount);
    PushLog(line);
}

// UI/windowing command family.
static bool TryProcessDesktopSurfaceCommand() {
    FKernelDesktopCursorCommandContext context{
        .CommandBuffer = GCommandBuffer,
        .CommandLength = &GCommandLength,
        .DesktopCompositor = GBoundDesktopCompositor,
        .DesktopInputRouter = GBoundDesktopInputRouter,
        .CursorOverlayEnabled = &GCursorOverlayEnabled,
        .DesktopSurfaceOverlayEnabled = &GDesktopSurfaceOverlayEnabled,
        .CursorInvertX = &GCursorInvertX,
        .CursorInvertY = &GCursorInvertY,
        .CursorSensitivityPercent = &GCursorSensitivityPercent,
        .GetCursorLatencyModeValueFn = nullptr,
        .SetCursorLatencyModeValueFn = nullptr,
        .GetCursorLatencyModeNameFn = GetCursorLatencyModeName,
        .PushLogFn = PushLog,
        .AppendCharFn = AppendChar,
        .AppendStringFn = AppendString,
        .AppendUIntFn = AppendUInt,
        .StrEqFn = StrEq,
        .StartsWithFn = StartsWith,
        .ParseUIntFn = ParseUInt,
        .ReadTokenFn = ReadToken,
        .MatchAnyExactFn = MatchAnyExact,
        .MatchAnyPrefixFn = MatchAnyPrefix,
        .AliasArgAfterPrefixFn = AliasArgAfterPrefix,
        .PublishCursorOverlaySetEventFn = PublishCursorOverlaySetEvent,
        .PublishDesktopSurfaceOverlaySetEventFn = PublishDesktopSurfaceOverlaySetEvent,
    };
    return Fortress::Kernel::TryProcessDesktopSurfaceCommand(context);
}

// UI cursor/overlay command family.
static bool TryProcessCursorCommand() {
    auto getLatency = []() -> uint8_t {
        return GCursorLatencyMode == ECursorLatencyMode::Responsive ? 1u : 0u;
    };
    auto setLatency = [](uint8_t value) {
        GCursorLatencyMode = (value == 1u) ? ECursorLatencyMode::Responsive : ECursorLatencyMode::Smooth;
    };

    FKernelDesktopCursorCommandContext context{
        .CommandBuffer = GCommandBuffer,
        .CommandLength = &GCommandLength,
        .DesktopCompositor = GBoundDesktopCompositor,
        .DesktopInputRouter = GBoundDesktopInputRouter,
        .CursorOverlayEnabled = &GCursorOverlayEnabled,
        .DesktopSurfaceOverlayEnabled = &GDesktopSurfaceOverlayEnabled,
        .CursorInvertX = &GCursorInvertX,
        .CursorInvertY = &GCursorInvertY,
        .CursorSensitivityPercent = &GCursorSensitivityPercent,
        .GetCursorLatencyModeValueFn = getLatency,
        .SetCursorLatencyModeValueFn = setLatency,
        .GetCursorLatencyModeNameFn = GetCursorLatencyModeName,
        .PushLogFn = PushLog,
        .AppendCharFn = AppendChar,
        .AppendStringFn = AppendString,
        .AppendUIntFn = AppendUInt,
        .StrEqFn = StrEq,
        .StartsWithFn = StartsWith,
        .ParseUIntFn = ParseUInt,
        .ReadTokenFn = ReadToken,
        .MatchAnyExactFn = MatchAnyExact,
        .MatchAnyPrefixFn = MatchAnyPrefix,
        .AliasArgAfterPrefixFn = AliasArgAfterPrefix,
        .PublishCursorOverlaySetEventFn = PublishCursorOverlaySetEvent,
        .PublishDesktopSurfaceOverlaySetEventFn = PublishDesktopSurfaceOverlaySetEvent,
    };
    return Fortress::Kernel::TryProcessCursorCommand(context);
}

// Memory mapping and pinning command family.
static bool TryProcessMemoryMapCommand() {
    FKernelMemoryMapCommandContext context{
        .CommandBuffer = GCommandBuffer,
        .CommandLength = &GCommandLength,
        .PushLogFn = PushLog,
        .ReserveVirtualRangeFn = ReserveVirtualRange,
        .ReleaseVirtualRangeFn = ReleaseVirtualRange,
    };
    return Fortress::Kernel::TryProcessMemoryMapCommand(context);
}

static void SetHudLogShowTail() {
    GHudLogViewMode = FKernelCommandConsole::EHudLogViewMode::ShowLog;
    GHudLogDetailMode = FKernelCommandConsole::EHudLogDetailMode::Tail;
    PushLog("HUD LOG SHOW TAIL");
}

static void SetHudLogShowFull() {
    GHudLogViewMode = FKernelCommandConsole::EHudLogViewMode::ShowLog;
    GHudLogDetailMode = FKernelCommandConsole::EHudLogDetailMode::Full;
    PushLog("HUD LOG SHOW FULL");
}

static void SetHudLogShowErrors() {
    GHudLogViewMode = FKernelCommandConsole::EHudLogViewMode::ShowLog;
    GHudLogDetailMode = FKernelCommandConsole::EHudLogDetailMode::Errors;
    PushLog("HUD LOG SHOW ERRORS");
}

static void SetHudLogShowWarn() {
    GHudLogViewMode = FKernelCommandConsole::EHudLogViewMode::ShowLog;
    GHudLogDetailMode = FKernelCommandConsole::EHudLogDetailMode::Warn;
    PushLog("HUD LOG SHOW WARN");
}

static void SetHudLogShowAllIssues() {
    GHudLogViewMode = FKernelCommandConsole::EHudLogViewMode::ShowLog;
    GHudLogDetailMode = FKernelCommandConsole::EHudLogDetailMode::AllIssues;
    PushLog("HUD LOG SHOW ALLISSUES");
}

static void SetHudLogBoot() {
    GHudLogViewMode = FKernelCommandConsole::EHudLogViewMode::BootLog;
    GHudLogDetailMode = FKernelCommandConsole::EHudLogDetailMode::Full;
    PushLog("HUD LOG BOOT");
}

static void SetHudLogHidden() {
    GTerminalModeEnabled = false;
    GHudLogViewMode = FKernelCommandConsole::EHudLogViewMode::Hidden;
    PushLog("HUD LOG HIDE");
}

static void RunTerminalModeQuery() {
    PushLog(GTerminalModeEnabled ? "TERMINAL ON" : "TERMINAL OFF");
}

static void SetTerminalModeOn() {
    GTerminalModeEnabled = true;
    GHudLogViewMode = FKernelCommandConsole::EHudLogViewMode::ShowLog;
    GHudLogDetailMode = FKernelCommandConsole::EHudLogDetailMode::Tail;
    PushLog("TERMINAL ON");
}

static void SetTerminalModeOff() {
    GTerminalModeEnabled = false;
    PushLog("TERMINAL OFF");
}

static void RunParallelHudQuery() {
    PushLog(GHudParallelStatsEnabled ? "PARALLELHUD ON" : "PARALLELHUD OFF");
}

static void SetParallelHudOn() {
    GHudParallelStatsEnabled = true;
    PushLog("PARALLELHUD ON");
}

static void SetParallelHudOff() {
    GHudParallelStatsEnabled = false;
    PushLog("PARALLELHUD OFF");
}

static void RunKbdLayoutQuery() {
    char line[64] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "KBD LAYOUT ");
    AppendString(line, sizeof(line), pos, FKeyboardManager::GetLayoutName());
    PushLog(line);
}

static void RunKbdLayoutSetUs() {
    FKeyboardManager::SetLayout(EKeyboardLayout::UsQwerty);
    PushLog("KBD LAYOUT US-QWERTY");
}

static void RunKbdLayoutSetDvorak() {
    FKeyboardManager::SetLayout(EKeyboardLayout::UsDvorak);
    PushLog("KBD LAYOUT US-DVORAK");
}

static void RunKbdModsQuery() {
    char line[64] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "KBD MODS SHIFT ");
    AppendUInt(line, sizeof(line), pos, (FKeyboardManager::GetModifierFlags() & KeyboardModifierShift) != 0u ? 1u : 0u);
    PushLog(line);
}

static void RunTextShaperQuery() {
    FTextShaperConfig config{};
    FTextRenderer::GetShaperConfig(config);
    char line[80] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "TEXT SHAPER ");
    AppendString(line, sizeof(line), pos, FTextRenderer::GetShaperModeName());
    AppendString(line, sizeof(line), pos, " WRAP ");
    AppendUInt(line, sizeof(line), pos, static_cast<uint64_t>(config.WrapWidthPixels));
    PushLog(line);
}

static void RunTextShaperSetBasic() {
    FTextShaperConfig config{};
    FTextRenderer::GetShaperConfig(config);
    config.Mode = ETextShaperMode::Basic;
    FTextRenderer::SetShaperConfig(config);
    PushLog("TEXT SHAPER BASIC");
}

static bool RunTextShaperSetWrap(const char *args) {
    FTextShaperConfig config{};
    FTextRenderer::GetShaperConfig(config);
    config.Mode = ETextShaperMode::Wrap;

    if (args != nullptr && args[0] != '\0') {
        uint64_t wrapWidth = 0u;
        if (!ParseUInt(args, wrapWidth) || wrapWidth > 16384u) {
            PushLog("TEXTSHAPER WRAP RANGE 0..16384");
            GCommandLength = 0;
            GCommandBuffer[0] = '\0';
            return false;
        }
        config.WrapWidthPixels = static_cast<Fortress::Core::int32>(wrapWidth);
    }

    FTextRenderer::SetShaperConfig(config);
    char line[80] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "TEXT SHAPER WRAP ");
    AppendUInt(line, sizeof(line), pos, static_cast<uint64_t>(config.WrapWidthPixels));
    PushLog(line);
    return true;
}

static void RunFontCacheQuery() {
    if (GBoundVideoConsole == nullptr) {
        PushLog("FONT CACHE CONSOLE UNBOUND");
        GCommandLength = 0;
        GCommandBuffer[0] = '\0';
        return;
    }

    Fortress::Video::FFontCacheStats stats{};
    GBoundVideoConsole->GetFontCacheStats(stats);
    char line[96] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "FONT CACHE E ");
    AppendUInt(line, sizeof(line), pos, stats.EntryCount);
    AppendString(line, sizeof(line), pos, " C ");
    AppendUInt(line, sizeof(line), pos, stats.Capacity);
    AppendString(line, sizeof(line), pos, " H ");
    AppendUInt(line, sizeof(line), pos, stats.HitCount);
    AppendString(line, sizeof(line), pos, " M ");
    AppendUInt(line, sizeof(line), pos, stats.MissCount);
    AppendString(line, sizeof(line), pos, " EV ");
    AppendUInt(line, sizeof(line), pos, stats.EvictionCount);
    PushLog(line);
}

static void RunFontCacheReset() {
    if (GBoundVideoConsole == nullptr) {
        PushLog("FONT CACHE CONSOLE UNBOUND");
        GCommandLength = 0;
        GCommandBuffer[0] = '\0';
        return;
    }

    GBoundVideoConsole->ResetFontCache();
    PushLog("FONT CACHE RESET");
}

static void RunUtilityHelp() {
    PushLog("CMDS: HELP SHUTDOWN|POWEROFF|HALT|OK SHOWLOG [TAIL|FULL|ERRORS|WARN|ALLISSUES] BOOTLOG HIDELOG TERMINAL [ON|OFF|STATUS] STATS EVENTHEALTH EVENTBURST VFSSTAT VFSRESOLVE VFSBOOT VFSBOOTBLK VFSBOOT0 LOGSAVE LOGSAVEBOOT SRDBSTAT SRDBFIND PORTSTAT PORTLIST PORTAUDIT [LAST|DENIED] PORTCHECK PORTOPEN PORTCLOSE PORTLEASE DSKZLIST DSKCHILDREN KBDLAYOUT KBDMODS TEXTSHAPER FONTCACHE DESKTOPSTAT DESKTOPRAISE DESKTOPFOCUS DESKTOPCAPTURE DESKTOPINPUT DESKTOPLIST DESKTOPDIRTY [N|ALL] DESKTOPINSPECT DESKTOPHIDE DESKTOPSHOW DESKTOPDAMAGE DESKTOPCREATE DESKTOPCLOSE DESKTOPMOVE DESKTOPRESIZE WINDOWSTAT WINDOWRAISE WINDOWFOCUS WINDOWCAPTURE WINDOWINPUT WINDOWLIST WINDOWDIRTY [N|ALL] WINDOWINSPECT WINDOWHIDE WINDOWSHOW WINDOWDAMAGE WINDOWCREATE WINDOWCLOSE WINDOWMOVE WINDOWRESIZE WIRE PAUSE RESUME PARALLEL PARALLELTEST CURSOR DSKSURFOVERLAY WINDOWOVERLAY");
    PushLog("KBD: KBDLAYOUT [US|DVORAK] KBDMODS");
    PushLog("TEXT: TEXTSHAPER [BASIC|WRAP [PX]]");
    PushLog("VFS: VFSRESOLVE /ABS/PATH | VFSBOOT NAME | VFSBOOTBLK INDEX | VFSBOOT0");
    PushLog("LOG: LOGSAVE /MOUNT START COUNT | LOGSAVEBOOT START COUNT");
    PushLog("FONT: FONTCACHE [RESET]");
    PushLog("DSKSURF: DSKSURFSTAT DSKSURFRAISE [ID] DSKSURFFOCUS [ID|NEXT]");
    PushLog("DSKSURF: DSKSURFCAPTURE [ID|OFF] DSKSURFINPUT (RX/DROP/PTR/CLK)");
    PushLog("DSKSURF: DSKSURFLIST DSKSURFDIRTY [N|ALL] DSKSURFINSPECT [ID] (default: focused surface)");
    PushLog("DSKSURF: DSKSURFHIDE ID DSKSURFSHOW ID DSKSURFDAMAGE ID");
    PushLog("DSKSURF: DSKSURFCREATE X Y W H Z DSKSURFCLOSE ID");
    PushLog("DSKSURF: DSKSURFMOVE ID X Y DSKSURFRESIZE ID W H");
    PushLog("DSKSURF: DSKSURFOVERLAY [ON|OFF]");
    PushLog("WINDOW: WINDOW* aliases map to desktop surface commands");
    PushLog("CURSOR: CURSOR [ON|OFF] INVERTX INVERTY SENS N LATENCY [SMOOTH|RESPONSIVE]");
    PushLog("CPU: PARALLEL [ON|OFF] PARALLEL DRAIN [ON|OFF] PARALLELTEST [N] PARALLELCANARY [N] PARALLELPROBE (DRAIN=>AP_PROBE)");
    PushLog("CPU: PARALLELHUD [ON|OFF] (HUD PARALLEL STATS SECTION)");
    PushLog("EVENT: EVENTHEALTH EVENTBURST [N] (N: 1..200000, DEFAULT 2048)");
    PushLog("HUD: SHOWLOG BOOTLOG HIDELOG TERMINAL [ON|OFF|STATUS] LAYERS");
    PushLog("VFS: VFSSTAT VFSRESOLVE /ABS/PATH");
    PushLog("VFS: LOGSAVE /MOUNT START_BLOCK BLOCK_COUNT (writes boot+runtime logs to /MOUNT/blk/N)");
    PushLog("PMM: PALLOC [N] PFREE PRESERVE LOW");
    PushLog("VMM: VMMAP VA PA [N] [rw|rwnx|rx|dev]");
    PushLog("VMM: VMALLOC N [rw|rwnx|rx|dev]");
    PushLog("VMM: VMFREE [VA] VMUNMAP VA [N]");
    PushLog("VMM: VMUNMAP VA [N] VMTRANSLATE VA");
    PushLog("DMA: DMAALLOC BYTES [ALIGN] [LOW4G]");
    PushLog("DMA: DMAFREE [VA]");
    PushLog("PIN: MMPIN PA BYTES [rw|rwnx|rx|dev]");
    PushLog("PIN: MMUNPIN [VA]");
    PushLog("XHCI: XHCIREGS [CAPVA|PABASE]");
    PushLog("XHCI: XHCIPROBE XHCIINIT XHCIRINGTEST XHCIENUM XHCIADDRDEV XHCIGETDESC XHCIGETCFG XHCISETCFG XHCIEPCONF XHCIINTRIN XHCIINTRINLOOP XHCIINTRINBG XHCIINTRINSTOP XHCIINTRINSTATUS XHCIMSCSTATUS");
    PushLog("XHCI: XHCIHID [COMPACT|CORE|VERBOSE]");
    PushLog("HEAP: KALLOC BYTES KFREE VA");
    PushLog("MEM: MEMTEST XHCIMEMTEST MMIOTEST");
}

static void RunUtilityEventHealth() {
    FMessageBusStats busStats{};
    FMessageBus::GetStats(busStats);
    char line[96] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "EVENT HEALTH BUS Q ");
    AppendUInt(line, sizeof(line), pos, busStats.QueueDepth);
    AppendString(line, sizeof(line), pos, " PUB ");
    AppendUInt(line, sizeof(line), pos, busStats.PublishedCount);
    AppendString(line, sizeof(line), pos, " DROP ");
    AppendUInt(line, sizeof(line), pos, busStats.DroppedCount);
    PushLog(line);

    FEventManagerStats eventStats{};
    FEventManager::GetStats(eventStats);
    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "EVENT HEALTH EVT SUB ");
    AppendUInt(line, sizeof(line), pos, eventStats.SubscriptionCount);
    AppendString(line, sizeof(line), pos, " PUB ");
    AppendUInt(line, sizeof(line), pos, eventStats.PublishedCount);
    AppendString(line, sizeof(line), pos, " DSP ");
    AppendUInt(line, sizeof(line), pos, eventStats.DispatchedCount);
    AppendString(line, sizeof(line), pos, " DROP ");
    AppendUInt(line, sizeof(line), pos, eventStats.DroppedCount);
    PushLog(line);

    FKernelSchedulerEventStats schedulerStats{};
    FKernelSchedulerEventPlane::GetStats(schedulerStats);
    FKernelCommandControlStats commandStats{};
    FKernelCommandControlPlane::GetStats(commandStats);
    FKernelInputEventStats inputStats{};
    FKernelInputEventPlane::GetStats(inputStats);

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "EVENT HEALTH HRX ");
    AppendUInt(line, sizeof(line), pos, schedulerStats.HeartbeatEventsReceived);
    AppendString(line, sizeof(line), pos, " CRX ");
    AppendUInt(line, sizeof(line), pos, commandStats.CommandEventsReceived);
    AppendString(line, sizeof(line), pos, " IRX ");
    AppendUInt(line, sizeof(line), pos, inputStats.InputEventsReceived);
    PushLog(line);
}

static void RunUtilityStats() {
    FMessageBusStats messageBusStats{};
    FMessageBus::GetStats(messageBusStats);
    char msgLine[96] = {};
    size_t msgPos = 0;
    AppendString(msgLine, sizeof(msgLine), msgPos, "MSG PUB ");
    AppendUInt(msgLine, sizeof(msgLine), msgPos, messageBusStats.PublishedCount);
    AppendString(msgLine, sizeof(msgLine), msgPos, " CONS ");
    AppendUInt(msgLine, sizeof(msgLine), msgPos, messageBusStats.ConsumedCount);
    AppendString(msgLine, sizeof(msgLine), msgPos, " Q ");
    AppendUInt(msgLine, sizeof(msgLine), msgPos, messageBusStats.QueueDepth);
    PushLog(msgLine);

    FEventManagerStats eventStats{};
    FEventManager::GetStats(eventStats);
    char eventLine[96] = {};
    size_t eventPos = 0;
    AppendString(eventLine, sizeof(eventLine), eventPos, "EVENT PUB ");
    AppendUInt(eventLine, sizeof(eventLine), eventPos, eventStats.PublishedCount);
    AppendString(eventLine, sizeof(eventLine), eventPos, " DSP ");
    AppendUInt(eventLine, sizeof(eventLine), eventPos, eventStats.DispatchedCount);
    AppendString(eventLine, sizeof(eventLine), eventPos, " DROP ");
    AppendUInt(eventLine, sizeof(eventLine), eventPos, eventStats.DroppedCount);
    PushLog(eventLine);

    FServiceRegistryStats serviceStats{};
    FServiceRegistry::GetStats(serviceStats);
    char serviceLine[64] = {};
    size_t servicePos = 0;
    AppendString(serviceLine, sizeof(serviceLine), servicePos, "SERVICES ");
    AppendUInt(serviceLine, sizeof(serviceLine), servicePos, serviceStats.ServiceCount);
    PushLog(serviceLine);

    PushLog("STATS REFRESHED");
}

static void RunUtilityRenderLayers() {
    char line[128] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "LAYERS HUD ");
    AppendString(line,
                 sizeof(line),
                 pos,
                 GHudLogViewMode == FKernelCommandConsole::EHudLogViewMode::Hidden ? "OFF" : "ON");
    AppendString(line, sizeof(line), pos, " CURSOR ");
    AppendUInt(line, sizeof(line), pos, GCursorOverlayEnabled ? 1u : 0u);
    AppendString(line, sizeof(line), pos, " DSKSURF ");
    AppendUInt(line, sizeof(line), pos, GDesktopSurfaceOverlayEnabled ? 1u : 0u);
    PushLog(line);

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "LAYERS EVT CURSOR ");
    AppendUInt(line,
               sizeof(line),
               pos,
               FKernelCommandControlPlane::IsCursorOverlayEnabled() ? 1u : 0u);
    AppendString(line, sizeof(line), pos, " DSKSURF ");
    AppendUInt(line,
               sizeof(line),
               pos,
               FKernelCommandControlPlane::IsDesktopSurfaceOverlayEnabled() ? 1u : 0u);
    PushLog(line);

    PushLog("LAYERS HUDPATH PIPELINE_ONLY");
}

static void ResetCommandInputBuffer();

// Utility/status/help command family.
static bool TryProcessUtilityCommand() {
    FKernelUtilityCommandContext utilityContext{
        .CommandBuffer = GCommandBuffer,
        .CommandLength = &GCommandLength,
        .BootLogFrozen = &GBootLogFrozen,
        .PushLog = PushLog,
        .StrEqFn = StrEq,
        .StartsWithFn = StartsWith,
        .ReadTokenFn = ReadToken,
        .ParseUIntFn = ParseUInt,
        .RunEventBurstFn = RunEventBurst,
        .RunVfsStatFn = RunVfsStat,
        .RunVfsResolveFn = RunVfsResolve,
        .RunServiceDbStatsFn = RunServiceDbStats,
        .RunServiceDbFindFn = RunServiceDbFind,
        .RunPortPolicyStatsFn = RunPortPolicyStats,
        .RunPortAuditLastFn = RunPortAuditLast,
        .RunPortAuditDeniedFn = RunPortAuditDenied,
        .RunPortPolicyCheckFn = RunPortPolicyCheck,
        .RunPortListFn = RunPortList,
        .RunPortOpenFn = RunPortOpen,
        .RunPortCloseFn = RunPortClose,
        .RunPortLeaseFn = RunPortLease,
        .RunDesktopZListFn = RunDesktopZList,
        .RunDesktopChildrenFn = RunDesktopChildren,
        .SetHudLogShowTailFn = SetHudLogShowTail,
        .SetHudLogShowFullFn = SetHudLogShowFull,
        .SetHudLogShowErrorsFn = SetHudLogShowErrors,
        .SetHudLogShowWarnFn = SetHudLogShowWarn,
        .SetHudLogShowAllIssuesFn = SetHudLogShowAllIssues,
        .SetHudLogBootFn = SetHudLogBoot,
        .SetHudLogHiddenFn = SetHudLogHidden,
        .RunTerminalModeQueryFn = RunTerminalModeQuery,
        .SetTerminalModeOnFn = SetTerminalModeOn,
        .SetTerminalModeOffFn = SetTerminalModeOff,
        .RunParallelHudQueryFn = RunParallelHudQuery,
        .SetParallelHudOnFn = SetParallelHudOn,
        .SetParallelHudOffFn = SetParallelHudOff,
        .RunKbdLayoutQueryFn = RunKbdLayoutQuery,
        .RunKbdLayoutSetUsFn = RunKbdLayoutSetUs,
        .RunKbdLayoutSetDvorakFn = RunKbdLayoutSetDvorak,
        .RunKbdModsQueryFn = RunKbdModsQuery,
        .RunTextShaperQueryFn = RunTextShaperQuery,
        .RunTextShaperSetBasicFn = RunTextShaperSetBasic,
        .RunTextShaperSetWrapFn = RunTextShaperSetWrap,
        .RunFontCacheQueryFn = RunFontCacheQuery,
        .RunFontCacheResetFn = RunFontCacheReset,
        .RunHelpFn = RunUtilityHelp,
        .RunEventHealthFn = RunUtilityEventHealth,
        .RunStatsFn = RunUtilityStats,
        .RunRenderLayersFn = RunUtilityRenderLayers,
        .ClearCommandInputFn = ResetCommandInputBuffer,
    };
    return Fortress::Kernel::TryProcessUtilityCommand(utilityContext);
}

static uint8_t GetHidLogModeValue() {
    if (GHidLogMode == EHidLogMode::Compact) {
        return 0u;
    }
    if (GHidLogMode == EHidLogMode::Core) {
        return 1u;
    }
    return 2u;
}

static void SetHidLogModeValue(uint8_t value) {
    if (value == 0u) {
        GHidLogMode = EHidLogMode::Compact;
    } else if (value == 1u) {
        GHidLogMode = EHidLogMode::Core;
    } else {
        GHidLogMode = EHidLogMode::Verbose;
    }
}

// xHCI USB diagnostic/control command family.
static bool TryProcessXhciCommand() {
    FKernelXhciCommandContext xhciContext{
        .CommandBuffer = GCommandBuffer,
        .CommandLength = &GCommandLength,
        .IntrinLoopBackgroundEnabled = &GXhciIntrinLoopBackgroundEnabled,
        .IntrinBackgroundHaveLastReport = &GXhciIntrinBackgroundHaveLastReport,
        .IntrinBackgroundLastReportLength = &GXhciIntrinBackgroundLastReportLength,
        .IntrinLoopBackgroundTickCounter = &GXhciIntrinLoopBackgroundTickCounter,
        .GetHidLogModeValueFn = GetHidLogModeValue,
        .SetHidLogModeValueFn = SetHidLogModeValue,
        .PushLog = PushLog,
        .StrEqFn = StrEq,
        .StartsWithFn = StartsWith,
        .ReadTokenFn = ReadToken,
        .ParseU64AutoFn = ParseU64Auto,
        .RunXhciRegisterAutoSnapshotFn = RunXhciRegisterAutoSnapshot,
        .RunXhciRegisterSnapshotFromAddressFn = RunXhciRegisterSnapshotFromAddress,
        .RunXhciAutoProbeFn = RunXhciAutoProbe,
        .RunXhciInitFn = RunXhciInit,
        .RunXhciRingTestFn = RunXhciRingTest,
        .RunXhciEnumFn = RunXhciEnum,
        .RunXhciAddressDeviceFn = RunXhciAddressDevice,
        .RunXhciGetDescriptorFn = RunXhciGetDescriptor,
        .RunXhciGetConfigDescriptorFn = RunXhciGetConfigDescriptor,
        .RunXhciSetConfigurationFn = RunXhciSetConfiguration,
        .RunXhciConfigureInterruptEndpointFn = RunXhciConfigureInterruptEndpoint,
        .RunXhciInterruptInFn = RunXhciInterruptInCommand,
        .RunXhciMscStatusFn = RunXhciMscStatus,
        .ClearCommandInputFn = ResetCommandInputBuffer,
    };
    return Fortress::Kernel::TryProcessXhciCommand(xhciContext);
}

static void FreezeBootLogOnFirstCommand() {
    if (!GBootLogFrozen && GCommandLength > 0) {
        GBootLogFrozen = true;
    }
}

static void LogSubmittedCommandIfAny() {
    if (GCommandLength > 0) {
        char submittedLine[96] = {};
        size_t submittedPos = 0;
        AppendString(submittedLine, sizeof(submittedLine), submittedPos, "CMD> ");
        AppendString(submittedLine, sizeof(submittedLine), submittedPos, GCommandBuffer);
        PushLog(submittedLine);
    }
}

struct FParallelTestWorkItem {
    uint32_t Slot = 0u;
};

static FParallelTestWorkItem GParallelTestWorkItems[4096] = {};
static uint64_t GParallelTestRequestedCount = 0ull;
static uint64_t GParallelTestExecutedCount = 0ull;

struct FParallelCanaryWorkItem {
    uint32_t TargetCoreId = 0u;
};
static FParallelCanaryWorkItem GParallelCanaryWorkItems[4096] = {};
static uint64_t GLastParallelProbeTicks = 0ull;
static uint64_t GLastParallelProbeExecuted = 0ull;
static uint32_t GParallelProbeApDrainNoExecStreak = 0u;

static void RunParallelTestWork(void *context) {
    FParallelTestWorkItem *item = static_cast<FParallelTestWorkItem *>(context);
    if (item == nullptr) {
        return;
    }

    (void)item->Slot;
    (void)__atomic_add_fetch(&GParallelTestExecutedCount, 1ull, __ATOMIC_RELAXED);
}

static void RunParallelTest(uint32_t workItems) {
    if (workItems == 0u) {
        PushLog("PARALLELTEST RANGE 1..100000");
        return;
    }

    uint32_t boundedItems = workItems;
    if (boundedItems > static_cast<uint32_t>(sizeof(GParallelTestWorkItems) / sizeof(GParallelTestWorkItems[0]))) {
        boundedItems = static_cast<uint32_t>(sizeof(GParallelTestWorkItems) / sizeof(GParallelTestWorkItems[0]));
    }

    FKernelCoreDispatchStats beforeStats{};
    FKernelCoreDispatch::GetStats(beforeStats);

    GParallelTestRequestedCount = boundedItems;
    GParallelTestExecutedCount = 0ull;

    uint32_t enqueued = 0u;
    for (uint32_t i = 0u; i < boundedItems; i++) {
        GParallelTestWorkItems[i].Slot = i;
        if (FKernelCoreDispatch::DispatchRoundRobin(&RunParallelTestWork, &GParallelTestWorkItems[i])) {
            enqueued++;
        }
    }

    FKernelCoreDispatchStats afterStats{};
    FKernelCoreDispatch::GetStats(afterStats);

    char line[128] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "PARALLELTEST REQ ");
    AppendUInt(line, sizeof(line), pos, boundedItems);
    AppendString(line, sizeof(line), pos, " ENQ ");
    AppendUInt(line, sizeof(line), pos, enqueued);
    AppendString(line, sizeof(line), pos, " PEND ");
    AppendUInt(line, sizeof(line), pos, afterStats.PendingWorkItems);
    PushLog(line);

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "PARALLELTEST CDISP EXE ");
    AppendUInt(line, sizeof(line), pos, afterStats.ExecutedCount);
    AppendString(line, sizeof(line), pos, " PREV ");
    AppendUInt(line, sizeof(line), pos, beforeStats.ExecutedCount);
    PushLog(line);
}

static void RunParallelCanaryWork(void *context) {
    (void)context;
}

static void RunParallelCanary(uint32_t workItems) {
    FCpuCoreManagerStats coreStats{};
    FCpuCoreManager::GetStats(coreStats);
    if (!coreStats.ParallelWorkersSupported || coreStats.OnlineCoreCount <= 1u) {
        PushLog("PARALLELCANARY UNSUPPORTED");
        return;
    }

    if (!FKernelApWorker::IsEnabled()) {
        PushLog("PARALLELCANARY REQUIRES PARALLEL ON");
        return;
    }

    const bool drainEnabled = FKernelApWorker::IsDrainEnabled();
    const bool dispatchCallbacksEnabled = FKernelApWorker::AreDispatchCallbacksEnabled();
    if (drainEnabled && !dispatchCallbacksEnabled) {
        PushLog("PARALLELCANARY BLOCKED AP_PROBE_NO_DISPATCH");
        if (FKernelApWorker::SetDrainEnabled(false)) {
            PushLog("PARALLELCANARY AP_PARKED");
        } else {
            PushLog("PARALLELCANARY AP_PARK_FAIL");
        }
        return;
    }

    if (drainEnabled && dispatchCallbacksEnabled) {
        static constexpr uint32_t GParallelCanaryDrainExperimentalMax = 4u;
        if (workItems > GParallelCanaryDrainExperimentalMax) {
            PushLog("PARALLELCANARY BLOCKED AP_DRAIN_UNSAFE");
            return;
        }

        char experimentalLine[96] = {};
        size_t experimentalPos = 0;
        AppendString(experimentalLine, sizeof(experimentalLine), experimentalPos, "PARALLELCANARY AP_DRAIN MICRO ");
        AppendUInt(experimentalLine, sizeof(experimentalLine), experimentalPos, workItems);
        AppendString(experimentalLine, sizeof(experimentalLine), experimentalPos, " MAX ");
        AppendUInt(experimentalLine, sizeof(experimentalLine), experimentalPos, GParallelCanaryDrainExperimentalMax);
        PushLog(experimentalLine);

#if !defined(FORTRESS_EXPERIMENTAL_AP_DISPATCH_CANARY_ENQUEUE)
        PushLog("PARALLELCANARY BLOCKED AP_DRAIN_ENQUEUE_UNSTABLE");
        return;
#endif
    }

    uint32_t boundedItems = workItems;
    const uint32_t canaryCapacity = static_cast<uint32_t>(sizeof(GParallelCanaryWorkItems) / sizeof(GParallelCanaryWorkItems[0]));
    if (boundedItems > canaryCapacity) {
        boundedItems = canaryCapacity;
    }

    const uint32_t onlineCoreCount = coreStats.OnlineCoreCount;
    const uint32_t bspCoreId = coreStats.BootstrapCoreId;

    if (drainEnabled && dispatchCallbacksEnabled) {
        char beginLine[96] = {};
        size_t beginPos = 0;
        AppendString(beginLine, sizeof(beginLine), beginPos, "PARALLELCANARY ENQ BEGIN N ");
        AppendUInt(beginLine, sizeof(beginLine), beginPos, boundedItems);
        PushLog(beginLine);
    }

    FKernelCoreDispatchStats beforeDispatchStats{};
    FKernelCoreDispatch::GetStats(beforeDispatchStats);
    const uint64_t beforeBspExecuted = FKernelCoreDispatch::GetExecutedCountForCore(bspCoreId);

    uint32_t enqueued = 0u;
    for (uint32_t i = 0u; i < boundedItems; i++) {
        uint32_t targetCore = i % onlineCoreCount;
        if (targetCore == bspCoreId) {
            targetCore = (targetCore + 1u) % onlineCoreCount;
        }

        GParallelCanaryWorkItems[i].TargetCoreId = targetCore;
        if (drainEnabled && dispatchCallbacksEnabled) {
            char itemLine[96] = {};
            size_t itemPos = 0;
            AppendString(itemLine, sizeof(itemLine), itemPos, "PARALLELCANARY ENQ ITEM ");
            AppendUInt(itemLine, sizeof(itemLine), itemPos, i);
            AppendString(itemLine, sizeof(itemLine), itemPos, " CORE ");
            AppendUInt(itemLine, sizeof(itemLine), itemPos, targetCore);
            PushLog(itemLine);
        }
        if (FKernelCoreDispatch::DispatchToCore(targetCore, &RunParallelCanaryWork, &GParallelCanaryWorkItems[i])) {
            enqueued++;
        }
    }

    if (drainEnabled && dispatchCallbacksEnabled) {
        char doneLine[96] = {};
        size_t donePos = 0;
        AppendString(doneLine, sizeof(doneLine), donePos, "PARALLELCANARY ENQ DONE ");
        AppendUInt(doneLine, sizeof(doneLine), donePos, enqueued);
        PushLog(doneLine);
    }

    FKernelCoreDispatchStats dispatchStats{};
    FKernelCoreDispatch::GetStats(dispatchStats);
    const uint64_t afterBspExecuted = FKernelCoreDispatch::GetExecutedCountForCore(bspCoreId);

    char line[128] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "PARALLELCANARY REQ ");
    AppendUInt(line, sizeof(line), pos, boundedItems);
    AppendString(line, sizeof(line), pos, " ENQ ");
    AppendUInt(line, sizeof(line), pos, enqueued);
    AppendString(line, sizeof(line), pos, " AP ");
    AppendUInt(line, sizeof(line), pos, onlineCoreCount - 1u);
    PushLog(line);

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "PARALLELCANARY MODE ");
    AppendString(line, sizeof(line), pos, FKernelApWorker::IsDrainEnabled() ? "AP_DRAIN" : "AP_PARKED");
    AppendString(line, sizeof(line), pos, " PEND ");
    AppendUInt(line, sizeof(line), pos, dispatchStats.PendingWorkItems);
    PushLog(line);

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "PARALLELCANARY EXE DELTA ");
    AppendUInt(line, sizeof(line), pos, dispatchStats.ExecutedCount - beforeDispatchStats.ExecutedCount);
    AppendString(line, sizeof(line), pos, " BSPDELTA ");
    AppendUInt(line, sizeof(line), pos, afterBspExecuted - beforeBspExecuted);
    PushLog(line);
}
static void RunParallelProbe() {
    FKernelApWorkerStats apStats{};
    FKernelApWorker::GetStats(apStats);

    FKernelCoreDispatchStats dispatchStats{};
    FKernelCoreDispatch::GetStats(dispatchStats);

    const uint64_t tickDelta = apStats.ProbeTicksTotal - GLastParallelProbeTicks;
    const uint64_t exeDelta = dispatchStats.ExecutedCount - GLastParallelProbeExecuted;
    GLastParallelProbeTicks = apStats.ProbeTicksTotal;
    GLastParallelProbeExecuted = dispatchStats.ExecutedCount;
    const bool apDrainActive = FKernelApWorker::IsDrainEnabled() && FKernelApWorker::AreDispatchCallbacksEnabled();

    char line[160] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "PARALLELPROBE MODE ");
    if (!FKernelApWorker::IsDrainEnabled()) {
        AppendString(line, sizeof(line), pos, "AP_PARKED");
    } else if (FKernelApWorker::AreDispatchCallbacksEnabled()) {
        AppendString(line, sizeof(line), pos, "AP_DRAIN");
    } else {
        AppendString(line, sizeof(line), pos, "AP_PROBE");
    }
    AppendString(line, sizeof(line), pos, " PTICK ");
    AppendUInt(line, sizeof(line), pos, apStats.ProbeTicksTotal);
    AppendString(line, sizeof(line), pos, " DPT ");
    AppendUInt(line, sizeof(line), pos, tickDelta);
    AppendString(line, sizeof(line), pos, " DEXE ");
    AppendUInt(line, sizeof(line), pos, exeDelta);
    PushLog(line);

    if (apDrainActive) {
        if (exeDelta == 0ull && dispatchStats.PendingWorkItems > 0u) {
            GParallelProbeApDrainNoExecStreak++;
            if (GParallelProbeApDrainNoExecStreak == 3u ||
                (GParallelProbeApDrainNoExecStreak > 3u && (GParallelProbeApDrainNoExecStreak % 8u) == 0u)) {
                char stallLine[160] = {};
                size_t stallPos = 0;
                AppendString(stallLine, sizeof(stallLine), stallPos, "PARALLELPROBE WARN AP_DRAIN NOEXEC STREAK ");
                AppendUInt(stallLine, sizeof(stallLine), stallPos, GParallelProbeApDrainNoExecStreak);
                AppendString(stallLine, sizeof(stallLine), stallPos, " PEND ");
                AppendUInt(stallLine, sizeof(stallLine), stallPos, dispatchStats.PendingWorkItems);
                AppendString(stallLine, sizeof(stallLine), stallPos, " PTICK ");
                AppendUInt(stallLine, sizeof(stallLine), stallPos, apStats.ProbeTicksTotal);
                PushLog(stallLine);
            }
        } else {
            GParallelProbeApDrainNoExecStreak = 0u;
        }
    } else {
        GParallelProbeApDrainNoExecStreak = 0u;
    }

    pos = 0;
    line[0] = '\0';
    AppendString(line, sizeof(line), pos, "PARALLELPROBE AP START ");
    AppendUInt(line, sizeof(line), pos, apStats.StartedWorkers);
    AppendString(line, sizeof(line), pos, " ACTIVE ");
    AppendUInt(line, sizeof(line), pos, apStats.ActiveWorkers);
    AppendString(line, sizeof(line), pos, " LAST ");
    AppendUInt(line, sizeof(line), pos, apStats.LastProbeCoreId);
    AppendString(line, sizeof(line), pos, " PEND ");
    AppendUInt(line, sizeof(line), pos, dispatchStats.PendingWorkItems);
    PushLog(line);
}

static void ResetCommandInputBuffer() {
    GCommandLength = 0;
    GCommandBuffer[0] = '\0';
}

static void ProcessCommand() {
    GCommandBuffer[GCommandLength] = '\0';
    PublishCommandSubmittedEvent();

    FreezeBootLogOnFirstCommand();
    LogSubmittedCommandIfAny();

    auto isParallelSupported = []() -> bool {
        FCpuCoreManagerStats coreStats{};
        FCpuCoreManager::GetStats(coreStats);
        return coreStats.ParallelWorkersSupported;
    };

    auto isParallelEnabled = []() -> bool {
        return FKernelApWorker::IsEnabled();
    };

    auto setParallelEnabled = [](bool enabled) -> bool {
        return FKernelApWorker::Enable(enabled);
    };

    auto isParallelDrainEnabled = []() -> bool {
        return FKernelApWorker::IsDrainEnabled();
    };

    auto areParallelDispatchCallbacksEnabled = []() -> bool {
        return FKernelApWorker::AreDispatchCallbacksEnabled();
    };

    auto setParallelDrainEnabled = [](bool enabled) -> bool {
        return FKernelApWorker::SetDrainEnabled(enabled);
    };

    // Runtime/control and utility families.
    FKernelRuntimeControlCommandContext runtimeControlContext{
        .CommandBuffer = GCommandBuffer,
        .WireframeEnabled = &GWireframe,
        .ScenePaused = &GPaused,
        .PublishWireframeSetEvent = PublishWireframeSetEvent,
        .PublishScenePauseSetEvent = PublishScenePauseSetEvent,
        .PushLog = PushLog,
        .IsParallelWorkersSupported = isParallelSupported,
        .IsParallelWorkersEnabled = isParallelEnabled,
        .SetParallelWorkersEnabled = setParallelEnabled,
        .IsParallelDrainEnabled = isParallelDrainEnabled,
        .AreParallelDispatchCallbacksEnabled = areParallelDispatchCallbacksEnabled,
        .SetParallelDrainEnabled = setParallelDrainEnabled,
        .StartsWithFn = StartsWith,
        .ParseUIntFn = ParseUInt,
        .RunParallelTestFn = RunParallelTest,
        .RunParallelCanaryFn = RunParallelCanary,
        .RunParallelProbeFn = RunParallelProbe,
        .RequestPlatformShutdown = RequestPlatformShutdown,
        .HaltCpuForever = HaltCpuForever,
    };

    FKernelHeapCommandContext heapCommandContext{
        .CommandBuffer = GCommandBuffer,
        .CommandLength = &GCommandLength,
        .PushLog = PushLog,
        .ClearCommandInput = ResetCommandInputBuffer,
        .RunMemorySelfTest = RunMemorySelfTest,
        .RunXhciMemorySmokeTest = RunXhciMemorySmokeTest,
        .RunMmioSmokeTest = RunMmioSmokeTest,
    };

    if (TryProcessRuntimeControlCommand(runtimeControlContext)) {
    } else if (TryProcessUtilityCommand()) {
    } else if (StrEq(GCommandBuffer, "logsaveboot") || StartsWith(GCommandBuffer, "logsaveboot ")) {
        if (StrEq(GCommandBuffer, "logsaveboot")) {
            PushLog("LOGSAVEBOOT USAGE START_BLOCK BLOCK_COUNT");
        } else {
            RunLogSaveBoot(GCommandBuffer + 12);
        }
    } else if (StrEq(GCommandBuffer, "logsave") || StartsWith(GCommandBuffer, "logsave ")) {
        if (StrEq(GCommandBuffer, "logsave")) {
            PushLog("LOGSAVE USAGE /MOUNT START_BLOCK BLOCK_COUNT");
        } else {
            RunLogSave(GCommandBuffer + 8);
        }

    // UI/input families.
    } else if (TryProcessCursorCommand()) {
    } else if (TryProcessDesktopSurfaceCommand()) {

    // Device families.
    } else if (TryProcessXhciCommand()) {

    // Memory and allocator families.
    } else if (TryProcessMemoryMapCommand()) {
    } else if (TryProcessHeapCommand(heapCommandContext)) {
    } else if (GCommandLength > 0) {
        PushLog("UNKNOWN COMMAND");
    }

    ResetCommandInputBuffer();
}

static void TickBackgroundCommands() {
    if (!GXhciIntrinLoopBackgroundEnabled || GXhciIntrinLoopBackgroundRunning) {
        return;
    }

    const bool apDrainActive = FKernelApWorker::IsDrainEnabled() && FKernelApWorker::AreDispatchCallbacksEnabled();
    if (apDrainActive) {
        if (!GXhciIntrinBackgroundAutoPausedForApDrain) {
            GXhciIntrinBackgroundAutoPausedForApDrain = true;
            PushLog("XHCI INTRIN BG PAUSED AP_DRAIN");
        }
        return;
    }

    if (GXhciIntrinBackgroundAutoPausedForApDrain) {
        GXhciIntrinBackgroundAutoPausedForApDrain = false;
        GXhciIntrinLoopBackgroundTickCounter = 0u;
        PushLog("XHCI INTRIN BG RESUME");
    }

    if (GCommandLength > 0) {
        return;
    }

    GXhciIntrinLoopBackgroundTickCounter++;
    if (GXhciIntrinLoopBackgroundTickCounter < GXhciIntrinLoopBackgroundTickDivider) {
        return;
    }
    GXhciIntrinLoopBackgroundTickCounter = 0u;

    GXhciIntrinLoopBackgroundRunning = true;
    RunXhciInterruptIn(false, 250000ull, true);
    GXhciIntrinLoopBackgroundRunning = false;
}

void FKernelCommandConsole::Initialize() {
    GWireframe = false;
    GPaused = false;
    GCommandLength = 0;
    GCommandBuffer[0] = '\0';
    GLogCount = 0;
    GBootLogCount = 0;
    GBootLogFrozen = false;
    GHudLogViewMode = FKernelCommandConsole::EHudLogViewMode::Hidden;
    GHudLogDetailMode = FKernelCommandConsole::EHudLogDetailMode::Tail;
    GTerminalModeEnabled = false;
    GHudParallelStatsEnabled = false;
    GSerialMirrorReady = false;
    GSerialMirrorFaulted = false;
    ResetKernelCommandHeapState();
    ResetKernelCommandMemoryMapState();
    GVirtualFreeRangeCount = 0;
    GLastUsbConfigValue = 0;
    GHaveLastUsbConfigValue = false;
    GLastUsbMscInterfaceNumber = 0;
    GHaveLastUsbMscInterface = false;
    GLastUsbMscBulkInEndpointAddress = 0;
    GLastUsbMscBulkOutEndpointAddress = 0;
    GLastUsbMscBulkInMaxPacketSize = 0;
    GLastUsbMscBulkOutMaxPacketSize = 0;
    GHaveLastUsbMscBulkPair = false;
    GLastUsbInterruptInEndpointAddress = 0;
    GLastUsbInterruptInMaxPacketSize = 0;
    GLastUsbInterruptInInterval = 0;
    GHaveLastUsbInterruptInEndpoint = false;
    GLastUsbIntrinEndpointKind = EUsbIntrinEndpointKind::Generic;
    GLastUsbKeyboardInterruptInEndpointAddress = 0;
    GLastUsbKeyboardInterruptInMaxPacketSize = 0;
    GLastUsbKeyboardInterruptInInterval = 0;
    GHaveLastUsbKeyboardInterruptInEndpoint = false;
    GLastUsbMouseInterruptInEndpointAddress = 0;
    GLastUsbMouseInterruptInMaxPacketSize = 0;
    GLastUsbMouseInterruptInInterval = 0;
    GHaveLastUsbMouseInterruptInEndpoint = false;
    GUsbBootKeyboardLastModifiers = 0;
    for (uint32_t i = 0u; i < 6u; i++) {
        GUsbBootKeyboardLastKeys[i] = 0;
    }
    GHaveUsbBootKeyboardLastReport = false;
    GHidLogMode = EHidLogMode::Verbose;
    GHidSmoothedNormXFp = 0;
    GHidSmoothedNormYFp = 0;
    GHidSmoothedMotionXFp = 0;
    GHidSmoothedMotionYFp = 0;
    GHidSmoothingInitialized = false;
    GHaveHidAbsoluteCursor = false;
    GHidCursorNormX = 0;
    GHidCursorNormY = 0;
    GCursorOverlayEnabled = false;
    GDesktopSurfaceOverlayEnabled = false;
    GCursorInvertX = false;
    GCursorInvertY = false;
    GCursorSensitivityPercent = 100;
    GCursorLatencyMode = ECursorLatencyMode::Smooth;
    GHidButtonsDownMask = 0;
    GHidButtonPressEdgesMask = 0;
    GHaveHidLogicalAxisRange = false;
    GHidLogicalMinX = 0;
    GHidLogicalMaxX = 65535;
    GHidLogicalMinY = 0;
    GHidLogicalMaxY = 65535;
    GHidAutoLogicalRangeInitialized = false;
    GHidAutoLogicalRangeReady = false;
    GHidAutoLogicalRangeActive = false;
    GHidAutoLogicalRangeSamples = 0;
    GHidAutoLogicalMinX = 0;
    GHidAutoLogicalMaxX = 0;
    GHidAutoLogicalMinY = 0;
    GHidAutoLogicalMaxY = 0;
    GHidAutoExpandBlendSamplesRemaining = 0;
    GHidAutoExpandBlendStartNormX = 0;
    GHidAutoExpandBlendStartNormY = 0;
    GHidAutoExpandCooldownSamplesRemaining = 0;
    GHidAutoExpandBelowMinXStreak = 0;
    GHidAutoExpandAboveMaxXStreak = 0;
    GHidAutoExpandBelowMinYStreak = 0;
    GHidAutoExpandAboveMaxYStreak = 0;
    GHidLogicalRangeProbeAttempted = false;
    GHaveHidLogicalRangeInterfaceHint = false;
    GHidLogicalRangeInterfaceHint = 0;
    for (uint32_t i = 0u; i < GMaxCommandPortLeases; i++) {
        GCommandPortLeases[i] = FCachedCommandPortLease{};
    }
    GXhciIntrinLoopBackgroundEnabled = false;
    GXhciIntrinLoopBackgroundRunning = false;
    GXhciIntrinLoopBackgroundTickDivider = 120u;
    GXhciIntrinLoopBackgroundTickCounter = 0u;
    GXhciIntrinBackgroundAutoPausedForApDrain = false;
    GXhciIntrinBackgroundHaveLastReport = false;
    GXhciIntrinBackgroundLastReportLength = 0u;
    GNextVmallocVirtual = Fortress::Kernel::FKernelConfig::VmallocBase;
#if defined(FORTRESS_PARALLEL_PROBE_AUTORUN)
    GParallelProbeAutorunCommandIndex = 0u;
    GParallelProbeAutorunCooldownTicks = GParallelProbeAutorunInitialDelayTicks;
#endif
    GParallelProbeApDrainNoExecStreak = 0u;
    PushLog("TYPE HELP FOR COMMANDS");
}

void FKernelCommandConsole::PollInput() {
    FKeyboardInputEvent event{};
    while (FKeyboardManager::PollEvent(event)) {
        if (!event.Pressed || event.Ascii == 0) {
            continue;
        }

        PublishInputKeyPressedEvent(event.Ascii, static_cast<Fortress::Core::uint32>(GCommandLength));

        if (event.Ascii == '\n') {
            ProcessCommand();
            continue;
        }

        if (event.Ascii == '\b') {
            if (GCommandLength > 0) {
                GCommandLength--;
                GCommandBuffer[GCommandLength] = '\0';
            }
            continue;
        }

        if (GCommandLength + 1 < sizeof(GCommandBuffer)) {
            GCommandBuffer[GCommandLength++] = event.Ascii;
            GCommandBuffer[GCommandLength] = '\0';
        }
    }

#if defined(FORTRESS_PARALLEL_PROBE_AUTORUN)
    TickParallelProbeAutorun();
#endif

    TickBackgroundCommands();
}

void FKernelCommandConsole::SetLongOperationYieldCallback(FLongOperationYieldCallback callback) {
    GLongOperationYieldCallback = callback;
}

void FKernelCommandConsole::BindVideoConsole(Fortress::Video::FVideoConsole *console) {
    GBoundVideoConsole = console;
}

void FKernelCommandConsole::BindDesktopCompositor(Fortress::Kernel::FDesktopCompositor *compositor) {
    GBoundDesktopCompositor = compositor;
}

void FKernelCommandConsole::BindDesktopInputRouter(Fortress::Kernel::FDesktopInputRouter *router) {
    GBoundDesktopInputRouter = router;
}

void FKernelCommandConsole::PushSystemLog(const char *line) {
    if (line == nullptr || line[0] == '\0') {
        return;
    }

    PushLog(line);
}

bool FKernelCommandConsole::IsWireframeEnabled() {
    return GWireframe;
}

bool FKernelCommandConsole::IsPaused() {
    return GPaused;
}

const char *FKernelCommandConsole::GetCommandBuffer() {
    return GCommandBuffer;
}

FKernelCommandConsole::EHudLogViewMode FKernelCommandConsole::GetHudLogViewMode() {
    return GHudLogViewMode;
}

FKernelCommandConsole::EHudLogDetailMode FKernelCommandConsole::GetHudLogDetailMode() {
    return GHudLogDetailMode;
}

bool FKernelCommandConsole::IsTerminalModeEnabled() {
    return GTerminalModeEnabled;
}

bool FKernelCommandConsole::IsHudParallelStatsEnabled() {
    return GHudParallelStatsEnabled;
}

Fortress::Core::usize FKernelCommandConsole::GetLogCount() {
    return GLogCount;
}

const char *FKernelCommandConsole::GetLogLine(Fortress::Core::usize index) {
    if (index >= GLogCount) {
        return "";
    }
    return GLogLines[index];
}

Fortress::Core::usize FKernelCommandConsole::GetBootLogCount() {
    return GBootLogCount;
}

const char *FKernelCommandConsole::GetBootLogLine(Fortress::Core::usize index) {
    if (index >= GBootLogCount) {
        return "";
    }
    return GBootLogLines[index];
}

bool FKernelCommandConsole::GetHidCursorNormalized(Fortress::Core::int32 &outX, Fortress::Core::int32 &outY) {
    if (!GHaveHidAbsoluteCursor) {
        return false;
    }

    outX = GHidCursorNormX;
    outY = GHidCursorNormY;
    return true;
}

bool FKernelCommandConsole::IsCursorOverlayEnabled() {
    return GCursorOverlayEnabled;
}

bool FKernelCommandConsole::IsCursorInvertX() {
    return GCursorInvertX;
}

bool FKernelCommandConsole::IsCursorInvertY() {
    return GCursorInvertY;
}

Fortress::Core::uint32 FKernelCommandConsole::GetCursorSensitivityPercent() {
    return GCursorSensitivityPercent;
}

void FKernelCommandConsole::GetHidButtonsDown(bool &outLeft, bool &outRight, bool &outMiddle) {
    outLeft = (GHidButtonsDownMask & 0x1u) != 0;
    outRight = (GHidButtonsDownMask & 0x2u) != 0;
    outMiddle = (GHidButtonsDownMask & 0x4u) != 0;
}

bool FKernelCommandConsole::ConsumeHidButtonPressEdges(bool &outLeft, bool &outRight, bool &outMiddle) {
    const uint8_t edges = GHidButtonPressEdgesMask;
    GHidButtonPressEdgesMask = 0;

    outLeft = (edges & 0x1u) != 0;
    outRight = (edges & 0x2u) != 0;
    outMiddle = (edges & 0x4u) != 0;
    return edges != 0;
}

} // namespace Fortress::Kernel
