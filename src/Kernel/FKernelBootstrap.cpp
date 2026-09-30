#include "Fortress/Kernel/FKernelBootstrap.hpp"

#include <cstddef>
#include <cstdint>

#include "Fortress/Cpu/FInterruptsX64.hpp"
#include "Fortress/Kernel/FCpuCoreManager.hpp"
#include "Fortress/Cpu/FPanicScreen.hpp"
#include "Fortress/Kernel/FKernelCommandConsole.hpp"
#include "Fortress/Kernel/FKernelConfig.hpp"
#include "Fortress/Kernel/FKernelApWorker.hpp"
#include "Fortress/Kernel/FKernelAIExecutionMonitor.hpp"
#include "Fortress/Kernel/FKernelCoreDispatch.hpp"
#include "Fortress/Kernel/FKernelNetworkTelemetry.hpp"
#include "Fortress/Kernel/FKernelCubeScene.hpp"
#include "Fortress/Kernel/FEventManager.hpp"
#include "Fortress/Kernel/FKernelIrqControlPlane.hpp"
#include "Fortress/Kernel/FKeyboardManager.hpp"
#include "Fortress/Kernel/FKernelRuntimeIds.hpp"
#include "Fortress/Kernel/FMessageBus.hpp"
#include "Fortress/Kernel/FPortManager.hpp"
#include "Fortress/Kernel/FKernelScheduler.hpp"
#include "Fortress/Kernel/FServiceRegistry.hpp"
#include "Fortress/Kernel/FServiceRegistryDatabaseAdapter.hpp"
#include "Fortress/Memory/FDmaMemoryManager.hpp"
#include "Fortress/Memory/FPinnedMappingManager.hpp"
#include "Fortress/Memory/FKernelHeap.hpp"
#include "Fortress/Memory/FMemoryArena.hpp"
#include "Fortress/Memory/FPhysicalMemoryManager.hpp"
#include "Fortress/Memory/FVirtualMemoryManager.hpp"
#include "Fortress/Platform/FTimerX86.hpp"
#include "Fortress/Storage/FRamBlockDevice.hpp"
#include "Fortress/Storage/FRawBlockFileSystemDriver.hpp"
#include "Fortress/Storage/FVirtualFileSystem.hpp"
#include "Fortress/Video/FDisplayManager.hpp"
#include "Fortress/Video/FRenderer3D.hpp"
#include "Fortress/Video/FRendererFactory.hpp"
#include "Fortress/Video/FSoftwareRenderer3D.hpp"
#include "Fortress/Video/FTextRenderer.hpp"
#include "Fortress/Video/FVideoConsole.hpp"
#include "Fortress/Video/FVideoDevice.hpp"

namespace Fortress::Kernel {

using Fortress::Cpu::FInterruptsX64;
using Fortress::Cpu::FPanicScreen;
using Fortress::Kernel::FCpuCoreManager;
using Fortress::Kernel::FCpuCoreManagerStats;
using Fortress::Kernel::FKernelApWorker;
using Fortress::Kernel::FKernelCoreDispatch;
using Fortress::Kernel::FKernelScheduler;
using Fortress::Kernel::FKernelSchedulerStats;
using Fortress::Kernel::FMessageBus;
using Fortress::Kernel::FMessageBusStats;
using Fortress::Kernel::FEventManager;
using Fortress::Kernel::FEventManagerStats;
using Fortress::Kernel::FKernelIrqControlPlane;
using Fortress::Kernel::FPortManager;
using Fortress::Kernel::FServiceRegistry;
using Fortress::Kernel::FServiceRegistryStats;
using Fortress::Kernel::FServiceRegistryDatabaseAdapter;
using Fortress::Memory::FKernelHeap;
using Fortress::Memory::FMemoryArena;
using Fortress::Memory::FPhysicalMemoryManager;
using Fortress::Memory::FPinnedMappingManager;
using Fortress::Memory::FDmaMemoryManager;
using Fortress::Memory::FVirtualMemoryManager;
using Fortress::Kernel::FKeyboardManager;
using Fortress::Platform::FTimerX86;
using Fortress::Storage::FRamBlockDevice;
using Fortress::Storage::FRawBlockFileSystemDriver;
using Fortress::Storage::FVirtualFileSystem;
using Fortress::Storage::FVirtualFileSystemRoute;
using Fortress::Storage::FVirtualFileSystemStats;
using Fortress::Video::ERendererBackend;
using Fortress::Video::FRenderer3D;
using Fortress::Video::FRendererFactory;
using Fortress::Video::FTextRenderer;
using Fortress::Video::FTextShaperConfig;
using Fortress::Video::FVideoConsole;
using Fortress::Video::FVideoDevice;
using Fortress::Video::FSoftwareRenderer3D;
using Fortress::Video::FDisplayManager;

alignas(16) static unsigned char GKernelArenaBuffer[FKernelConfig::KernelArenaBytes];
static FVideoDevice GVideoDevice;
static FDisplayManager GDisplayManager;
static FVideoConsole GConsole;
static FSoftwareRenderer3D GSoftwareRenderer3D;
static FRenderer3D *GRenderer3D = nullptr;
static FKernelCubeScene GCubeScene;
static FRamBlockDevice GBootVolumeDevice;
static FRawBlockFileSystemDriver GBootVolumeFileSystem;

struct FBootstrapTaskContext {
    uint64_t RunCount;
};

static FBootstrapTaskContext GIdleTaskContext = {};
static FBootstrapTaskContext GHeartbeatTaskContext = {};
static constexpr bool GEnableParallelWorkersAtBoot = false;

static void RunIdleTask(void *context) {
    if (context == nullptr) {
        return;
    }

    FBootstrapTaskContext *taskContext = static_cast<FBootstrapTaskContext *>(context);
    taskContext->RunCount++;
}

static void RunHeartbeatTask(void *context) {
    if (context == nullptr) {
        return;
    }

    FBootstrapTaskContext *taskContext = static_cast<FBootstrapTaskContext *>(context);
    taskContext->RunCount++;

    const FKernelEvent heartbeatEvent{
        .EventId = FKernelRuntimeIds::EventSchedulerHeartbeat,
        .TopicId = FKernelRuntimeIds::TopicScheduler,
        .SourceServiceId = FKernelRuntimeIds::ServiceScheduler,
        .Arg0 = static_cast<uint32_t>(taskContext->RunCount & 0xFFFFFFFFull),
        .Arg1 = 0,
        .Arg2 = 0,
    };
    (void)FEventManager::Publish(heartbeatEvent);
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

static bool BuffersEqual(const Fortress::Core::uint8 *a,
                         const Fortress::Core::uint8 *b,
                         Fortress::Core::uint32 sizeBytes) {
    if (a == nullptr || b == nullptr) {
        return false;
    }

    for (Fortress::Core::uint32 i = 0; i < sizeBytes; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }

    return true;
}

static void RunStorageSmokeDiagnostic() {
    FVirtualFileSystemRoute route{};
    if (!FVirtualFileSystem::ResolvePath("/boot/blk/1", route) || !route.Found) {
        FKernelCommandConsole::PushSystemLog("VFS RWTEST ROUTE FAIL");
        return;
    }

    if (route.ReadOnly) {
        FKernelCommandConsole::PushSystemLog("VFS RWTEST READONLY");
        return;
    }

    constexpr Fortress::Core::uint32 MaxTestBlockBytes = 1024u;
    const Fortress::Core::uint32 blockSizeBytes = GBootVolumeDevice.GetBlockSizeBytes();
    if (blockSizeBytes == 0u || blockSizeBytes > MaxTestBlockBytes) {
        FKernelCommandConsole::PushSystemLog("VFS RWTEST GEOM FAIL");
        return;
    }

    Fortress::Core::uint8 writeBuffer[MaxTestBlockBytes] = {};
    Fortress::Core::uint8 readBuffer[MaxTestBlockBytes] = {};

    for (Fortress::Core::uint32 i = 0; i < blockSizeBytes; i++) {
        writeBuffer[i] = static_cast<Fortress::Core::uint8>((i * 17u + 3u) & 0xFFu);
    }

    Fortress::Core::uint32 writtenBytes = 0u;
    if (!FVirtualFileSystem::WriteFile("/boot/blk/1", writeBuffer, blockSizeBytes, writtenBytes) ||
        writtenBytes != blockSizeBytes) {
        FKernelCommandConsole::PushSystemLog("VFS RWTEST WRITE FAIL");
        return;
    }

    Fortress::Core::uint32 readBytes = 0u;
    if (!FVirtualFileSystem::ReadFile("/boot/blk/1", readBuffer, blockSizeBytes, readBytes) ||
        readBytes != blockSizeBytes) {
        FKernelCommandConsole::PushSystemLog("VFS RWTEST READ FAIL");
        return;
    }

    if (!BuffersEqual(writeBuffer, readBuffer, blockSizeBytes)) {
        FKernelCommandConsole::PushSystemLog("VFS RWTEST VERIFY FAIL");
        return;
    }

    char line[96] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "VFS RWTEST PASS BS ");
    AppendUInt(line, sizeof(line), pos, blockSizeBytes);
    AppendString(line, sizeof(line), pos, " DRV ");
    AppendString(line, sizeof(line), pos, route.DriverName);
    FKernelCommandConsole::PushSystemLog(line);
}

static void RunTextShaperSmokeDiagnostic() {
    FTextShaperConfig originalConfig{};
    FTextRenderer::GetShaperConfig(originalConfig);

    FTextShaperConfig wrapConfig = originalConfig;
    wrapConfig.Mode = Fortress::Video::ETextShaperMode::Wrap;
    wrapConfig.WrapWidthPixels = 320;
    FTextRenderer::SetShaperConfig(wrapConfig);

    FTextShaperConfig wrapReadback{};
    FTextRenderer::GetShaperConfig(wrapReadback);
    if (wrapReadback.Mode == Fortress::Video::ETextShaperMode::Wrap && wrapReadback.WrapWidthPixels == 320) {
        FKernelCommandConsole::PushSystemLog("TEXT SHAPER WRAP 320");
    } else {
        FKernelCommandConsole::PushSystemLog("TEXT SHAPER WRAP VERIFY FAIL");
    }

    FTextShaperConfig basicConfig = wrapReadback;
    basicConfig.Mode = Fortress::Video::ETextShaperMode::Basic;
    FTextRenderer::SetShaperConfig(basicConfig);

    FTextShaperConfig basicReadback{};
    FTextRenderer::GetShaperConfig(basicReadback);
    if (basicReadback.Mode == Fortress::Video::ETextShaperMode::Basic) {
        FKernelCommandConsole::PushSystemLog("TEXT SHAPER BASIC");
    } else {
        FKernelCommandConsole::PushSystemLog("TEXT SHAPER BASIC VERIFY FAIL");
    }

    FTextRenderer::SetShaperConfig(originalConfig);
}

static const char *GetPixelFormatName(Fortress::Video::EPixelFormat format) {
    switch (format) {
    case Fortress::Video::EPixelFormat::Masked32:
        return "MASKED32";
    case Fortress::Video::EPixelFormat::Unknown:
    default:
        return "UNKNOWN";
    }
}

void FKernelBootstrap::HaltForever() {
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void FKernelBootstrap::EnableFPUAndSSE() {
    uint64_t cr0;
    uint64_t cr4;

    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ull << 2);
    cr0 |= (1ull << 1);
    cr0 &= ~(1ull << 3);
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ull << 9);
    cr4 |= (1ull << 10);
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));

    __asm__ volatile("fninit");
}

bool FKernelBootstrap::Initialize(const limine_framebuffer_response *framebufferResponse,
                                  const limine_memmap_response *memmapResponse,
                                  const limine_hhdm_response *hhdmResponse,
                                  const limine_smp_response *mpResponse,
                                  FKernelRuntimeContext &outContext) {
    if (framebufferResponse == nullptr || framebufferResponse->framebuffer_count < 1) {
        return false;
    }

    if (memmapResponse == nullptr || memmapResponse->entry_count == 0) {
        return false;
    }

    if (hhdmResponse == nullptr) {
        return false;
    }

    limine_framebuffer *framebuffer = framebufferResponse->framebuffers[0];
    if (framebuffer == nullptr || framebuffer->address == nullptr || framebuffer->bpp != 32) {
        return false;
    }

    FPanicScreen::Initialize(framebuffer);
    FInterruptsX64::Initialize(FPanicScreen::OnException);

    if (!FMemoryArena::Initialize(GKernelArenaBuffer, sizeof(GKernelArenaBuffer))) {
        return false;
    }

    if (!FPhysicalMemoryManager::Initialize(memmapResponse)) {
        return false;
    }

    (void)FPhysicalMemoryManager::ReserveRange(0, FKernelConfig::ReserveLowBytes);

    if (!FVirtualMemoryManager::Initialize(hhdmResponse->offset)) {
        return false;
    }

    if (!FKernelHeap::Initialize(FKernelConfig::KernelHeapBase,
                                 FKernelConfig::KernelHeapInitialPages,
                                 FVirtualMemoryManager::FlagsKernelRWNX)) {
        return false;
    }

    if (!FPinnedMappingManager::Initialize(FKernelConfig::PinnedMapBase, FKernelConfig::PinnedMapPages)) {
        return false;
    }

    if (!FDmaMemoryManager::Initialize(FKernelConfig::DmaMapBase, FKernelConfig::DmaMapPages)) {
        return false;
    }

    if (!GVideoDevice.Initialize(framebuffer)) {
        return false;
    }

    if (!GDisplayManager.Initialize(&GVideoDevice)) {
        return false;
    }

    const ERendererBackend selectedBackend = FRendererFactory::SelectDefaultBackend();
    if (!FRendererFactory::CreateAndInitializeRenderer(selectedBackend,
                                                       &GVideoDevice,
                                                       GSoftwareRenderer3D,
                                                       GRenderer3D)) {
        return false;
    }

    if (!GConsole.Initialize(&GVideoDevice, 2)) {
        return false;
    }

    const Fortress::Video::FDisplayMode displayMode = GDisplayManager.GetMode();
    if (!GCubeScene.Initialize(GRenderer3D, displayMode.Width, displayMode.Height)) {
        return false;
    }

    if (!FTimerX86::Initialize()) {
        return false;
    }

    if (!FKeyboardManager::Initialize()) {
        return false;
    }

    if (!FCpuCoreManager::Initialize(mpResponse)) {
        return false;
    }

    if (!FKernelCoreDispatch::Initialize(FCpuCoreManager::GetOnlineCoreCount(),
                                         FCpuCoreManager::GetBootstrapCoreId())) {
        return false;
    }

    if (!FKernelApWorker::InstallEntrypoint()) {
        if (FCpuCoreManager::GetOnlineCoreCount() > 1u) {
            return false;
        }
    }

    if (!FKernelApWorker::Enable(GEnableParallelWorkersAtBoot)) {
        return false;
    }

    if (!FKernelScheduler::Initialize(FCpuCoreManager::GetBootstrapCoreId(), FCpuCoreManager::GetOnlineCoreCount())) {
        return false;
    }

    if (!FMessageBus::Initialize()) {
        return false;
    }

    if (!FEventManager::Initialize()) {
        return false;
    }

    if (!FKernelNetworkTelemetry::Initialize()) {
        return false;
    }

    FKernelNetworkTelemetry::SetInterfaceCount(0u);
    FKernelNetworkTelemetry::SetLinkState(false);

    if (!FKernelAIExecutionMonitor::Initialize()) {
        return false;
    }

    FKernelAIExecutionMonitor::SetBuiltInPolicyMode(EKernelAIBuiltInPolicyMode::NoOp);

    if (!FServiceRegistry::Initialize()) {
        return false;
    }

    if (!FServiceRegistry::RegisterService(FServiceRegistrationInfo{
            .ServiceId = FKernelRuntimeIds::ServiceScheduler,
            .EndpointChannelId = FKernelRuntimeIds::ChannelSchedulerEndpoint,
            .Name = "KernelScheduler",
        })) {
        return false;
    }

    if (!FServiceRegistry::RegisterService(FServiceRegistrationInfo{
            .ServiceId = FKernelRuntimeIds::ServiceDisplayManager,
            .EndpointChannelId = FKernelRuntimeIds::ChannelDisplayEndpoint,
            .Name = "DisplayManager",
        })) {
        return false;
    }

    if (!FServiceRegistry::RegisterService(FServiceRegistrationInfo{
            .ServiceId = FKernelRuntimeIds::ServiceCommandConsole,
            .EndpointChannelId = FKernelRuntimeIds::ChannelCommandConsoleEndpoint,
            .Name = "CommandConsole",
        })) {
        return false;
    }

    if (!FServiceRegistry::RegisterService(FServiceRegistrationInfo{
            .ServiceId = FKernelRuntimeIds::ServiceKeyboardInput,
            .EndpointChannelId = FKernelRuntimeIds::ChannelKeyboardInputEndpoint,
            .Name = "KeyboardInput",
        })) {
        return false;
    }

    if (!FServiceRegistry::RegisterService(FServiceRegistrationInfo{
            .ServiceId = FKernelRuntimeIds::ServiceVirtualFileSystem,
            .EndpointChannelId = FKernelRuntimeIds::ChannelVirtualFileSystemEndpoint,
            .Name = "VirtualFileSystem",
        })) {
        return false;
    }

    if (!FServiceRegistry::RegisterService(FServiceRegistrationInfo{
            .ServiceId = FKernelRuntimeIds::ServiceDesktopCompositor,
            .EndpointChannelId = FKernelRuntimeIds::ChannelDesktopCompositorEndpoint,
            .Name = "DesktopCompositor",
        })) {
        return false;
    }

    if (!FServiceRegistry::RegisterService(FServiceRegistrationInfo{
            .ServiceId = FKernelRuntimeIds::ServiceDesktopShell,
            .EndpointChannelId = FKernelRuntimeIds::ChannelDesktopShellEndpoint,
            .Name = "DesktopShell",
        })) {
        return false;
    }

    if (!FServiceRegistryDatabaseAdapter::Initialize()) {
        return false;
    }

    if (!FServiceRegistryDatabaseAdapter::RefreshFromServiceRegistry()) {
        return false;
    }

    if (!FPortManager::Initialize()) {
        return false;
    }

    if (!FPortManager::RegisterPort(FKernelRuntimeIds::PortDisplaySurface, "DisplaySurface")) {
        return false;
    }

    if (!FPortManager::RegisterPort(FKernelRuntimeIds::PortKeyboardInput, "KeyboardInput")) {
        return false;
    }

    if (!FPortManager::RegisterPort(FKernelRuntimeIds::PortBootVolume, "BootVolume")) {
        return false;
    }

    uint32_t displayLeaseId = 0u;
    if (!FPortManager::OpenLease(FKernelRuntimeIds::PortDisplaySurface,
                                 FKernelRuntimeIds::ServiceDisplayManager,
                                 "DisplayManager",
                                 displayLeaseId)) {
        return false;
    }

    uint32_t keyboardLeaseId = 0u;
    if (!FPortManager::OpenLease(FKernelRuntimeIds::PortKeyboardInput,
                                 FKernelRuntimeIds::ServiceKeyboardInput,
                                 "KeyboardInput",
                                 keyboardLeaseId)) {
        return false;
    }

    uint32_t bootVolumeLeaseId = 0u;
    if (!FPortManager::OpenLease(FKernelRuntimeIds::PortBootVolume,
                                 FKernelRuntimeIds::ServiceVirtualFileSystem,
                                 "VirtualFileSystem",
                                 bootVolumeLeaseId)) {
        return false;
    }

    (void)displayLeaseId;
    (void)keyboardLeaseId;
    (void)bootVolumeLeaseId;
    (void)FPortManager::CanServiceAccessPort(FKernelRuntimeIds::PortDisplaySurface,
                                             FKernelRuntimeIds::ServiceCommandConsole);
    (void)FPortManager::CanServiceAccessPort(FKernelRuntimeIds::PortKeyboardInput,
                                             FKernelRuntimeIds::ServiceDisplayManager);
    (void)FPortManager::CanServiceAccessPort(FKernelRuntimeIds::PortBootVolume,
                                             FKernelRuntimeIds::ServiceCommandConsole);

    if (!FKernelIrqControlPlane::Initialize()) {
        return false;
    }

    if (!FKernelIrqControlPlane::RegisterVector(32u,
                                                FKernelRuntimeIds::ServiceScheduler,
                                                "PIT_TIMER",
                                                false)) {
        return false;
    }

    if (!FKernelIrqControlPlane::RegisterVector(33u,
                                                FKernelRuntimeIds::ServiceKeyboardInput,
                                                "PS2_KEYBOARD",
                                                false)) {
        return false;
    }

    if (!GBootVolumeDevice.Initialize(FRamBlockDevice::SupportedBlockSizeBytes, 2048u, false)) {
        return false;
    }

    if (!FVirtualFileSystem::Initialize()) {
        return false;
    }

    if (!FVirtualFileSystem::Mount("/boot", &GBootVolumeDevice, &GBootVolumeFileSystem, false)) {
        return false;
    }

    FKernelTaskHandle idleTaskHandle{};
    const FKernelTaskCreateInfo idleTask{
        .Name = "KernelIdle",
        .Entry = &RunIdleTask,
        .Context = &GIdleTaskContext,
        .PreferredCoreId = FCpuCoreManager::GetBootstrapCoreId(),
        .TimeSliceTicks = 1,
        .StartReady = true,
    };
    if (!FKernelScheduler::CreateTask(idleTask, idleTaskHandle)) {
        return false;
    }

    FKernelTaskHandle heartbeatTaskHandle{};
    const FKernelTaskCreateInfo heartbeatTask{
        .Name = "KernelHeartbeat",
        .Entry = &RunHeartbeatTask,
        .Context = &GHeartbeatTaskContext,
        .PreferredCoreId = FCpuCoreManager::GetBootstrapCoreId(),
        .TimeSliceTicks = 1,
        .StartReady = true,
    };
    if (!FKernelScheduler::CreateTask(heartbeatTask, heartbeatTaskHandle)) {
        return false;
    }

    FKernelCommandConsole::Initialize();
    FKernelCommandConsole::BindVideoConsole(&GConsole);

    char backendLine[64] = {};
    size_t backendLinePos = 0;
    AppendString(backendLine, sizeof(backendLine), backendLinePos, "RENDERER BACKEND: ");
    AppendString(backendLine,
                 sizeof(backendLine),
                 backendLinePos,
                 FRendererFactory::GetBackendName(selectedBackend));
    FKernelCommandConsole::PushSystemLog(backendLine);

    char modeLine[80] = {};
    size_t modeLinePos = 0;
    AppendString(modeLine, sizeof(modeLine), modeLinePos, "DISPLAY MODE: ");
    AppendUInt(modeLine, sizeof(modeLine), modeLinePos, displayMode.Width);
    AppendChar(modeLine, sizeof(modeLine), modeLinePos, 'x');
    AppendUInt(modeLine, sizeof(modeLine), modeLinePos, displayMode.Height);
    AppendChar(modeLine, sizeof(modeLine), modeLinePos, ' ');
    AppendString(modeLine, sizeof(modeLine), modeLinePos, GetPixelFormatName(displayMode.PixelFormat));
    FKernelCommandConsole::PushSystemLog(modeLine);

    char keyboardLayoutLine[64] = {};
    size_t keyboardLayoutLinePos = 0;
    AppendString(keyboardLayoutLine, sizeof(keyboardLayoutLine), keyboardLayoutLinePos, "KEYBOARD LAYOUT: ");
    AppendString(keyboardLayoutLine,
                 sizeof(keyboardLayoutLine),
                 keyboardLayoutLinePos,
                 FKeyboardManager::GetLayoutName());
    FKernelCommandConsole::PushSystemLog(keyboardLayoutLine);

    char keyboardModifierLine[64] = {};
    size_t keyboardModifierLinePos = 0;
    AppendString(keyboardModifierLine, sizeof(keyboardModifierLine), keyboardModifierLinePos, "KEYBOARD MODS: SHIFT ");
    AppendUInt(keyboardModifierLine,
               sizeof(keyboardModifierLine),
               keyboardModifierLinePos,
               (FKeyboardManager::GetModifierFlags() & KeyboardModifierShift) != 0u ? 1u : 0u);
    FKernelCommandConsole::PushSystemLog(keyboardModifierLine);

    FTextShaperConfig textShaperConfig{};
    FTextRenderer::GetShaperConfig(textShaperConfig);
    char textShaperLine[96] = {};
    size_t textShaperLinePos = 0;
    AppendString(textShaperLine, sizeof(textShaperLine), textShaperLinePos, "TEXT SHAPER: ");
    AppendString(textShaperLine, sizeof(textShaperLine), textShaperLinePos, FTextRenderer::GetShaperModeName());
    AppendString(textShaperLine, sizeof(textShaperLine), textShaperLinePos, " WRAP ");
    AppendUInt(textShaperLine,
               sizeof(textShaperLine),
               textShaperLinePos,
               static_cast<uint64_t>(textShaperConfig.WrapWidthPixels));
    FKernelCommandConsole::PushSystemLog(textShaperLine);

    Fortress::Video::FFontCacheStats fontCacheStats{};
    GConsole.GetFontCacheStats(fontCacheStats);
    char fontCacheLine[96] = {};
    size_t fontCacheLinePos = 0;
    AppendString(fontCacheLine, sizeof(fontCacheLine), fontCacheLinePos, "FONT CACHE: E ");
    AppendUInt(fontCacheLine, sizeof(fontCacheLine), fontCacheLinePos, fontCacheStats.EntryCount);
    AppendString(fontCacheLine, sizeof(fontCacheLine), fontCacheLinePos, " C ");
    AppendUInt(fontCacheLine, sizeof(fontCacheLine), fontCacheLinePos, fontCacheStats.Capacity);
    AppendString(fontCacheLine, sizeof(fontCacheLine), fontCacheLinePos, " H ");
    AppendUInt(fontCacheLine, sizeof(fontCacheLine), fontCacheLinePos, fontCacheStats.HitCount);
    AppendString(fontCacheLine, sizeof(fontCacheLine), fontCacheLinePos, " M ");
    AppendUInt(fontCacheLine, sizeof(fontCacheLine), fontCacheLinePos, fontCacheStats.MissCount);
    FKernelCommandConsole::PushSystemLog(fontCacheLine);

    FCpuCoreManagerStats coreStats{};
    FCpuCoreManager::GetStats(coreStats);
    char coreLine[80] = {};
    size_t coreLinePos = 0;
    AppendString(coreLine, sizeof(coreLine), coreLinePos, "CPU CORES ONLINE: ");
    AppendUInt(coreLine, sizeof(coreLine), coreLinePos, coreStats.OnlineCoreCount);
    AppendString(coreLine, sizeof(coreLine), coreLinePos, " BSP: ");
    AppendUInt(coreLine, sizeof(coreLine), coreLinePos, coreStats.BootstrapCoreId);
    FKernelCommandConsole::PushSystemLog(coreLine);

    char parallelLine[112] = {};
    size_t parallelLinePos = 0;
    AppendString(parallelLine, sizeof(parallelLine), parallelLinePos, "CPU PARALLEL SUPPORT: ");
    AppendUInt(parallelLine, sizeof(parallelLine), parallelLinePos, coreStats.ParallelWorkersSupported ? 1u : 0u);
    AppendString(parallelLine, sizeof(parallelLine), parallelLinePos, " ENABLED: ");
    AppendUInt(parallelLine, sizeof(parallelLine), parallelLinePos, coreStats.ParallelWorkersEnabled ? 1u : 0u);
    AppendString(parallelLine, sizeof(parallelLine), parallelLinePos, " PLANNED AP: ");
    AppendUInt(parallelLine, sizeof(parallelLine), parallelLinePos, coreStats.PlannedParallelWorkerCount);
    FKernelCommandConsole::PushSystemLog(parallelLine);

    char parallelPolicyLine[112] = {};
    size_t parallelPolicyLinePos = 0;
    AppendString(parallelPolicyLine, sizeof(parallelPolicyLine), parallelPolicyLinePos, "CPU PARALLEL BOOT FLAG: ");
    AppendUInt(parallelPolicyLine,
               sizeof(parallelPolicyLine),
               parallelPolicyLinePos,
               GEnableParallelWorkersAtBoot ? 1u : 0u);
    AppendString(parallelPolicyLine, sizeof(parallelPolicyLine), parallelPolicyLinePos, " ENTRY INSTALLED: ");
    AppendUInt(parallelPolicyLine,
               sizeof(parallelPolicyLine),
               parallelPolicyLinePos,
               coreStats.ParallelWorkerEntrypointInstalled ? 1u : 0u);
    FKernelCommandConsole::PushSystemLog(parallelPolicyLine);

    FKernelSchedulerStats schedulerStats{};
    FKernelScheduler::GetStats(schedulerStats);
    char schedulerLine[80] = {};
    size_t schedulerLinePos = 0;
    AppendString(schedulerLine, sizeof(schedulerLine), schedulerLinePos, "SCHED INIT CORES: ");
    AppendUInt(schedulerLine, sizeof(schedulerLine), schedulerLinePos, schedulerStats.OnlineCoreCount);
    FKernelCommandConsole::PushSystemLog(schedulerLine);

    char schedulerTasksLine[80] = {};
    size_t schedulerTasksLinePos = 0;
    AppendString(schedulerTasksLine, sizeof(schedulerTasksLine), schedulerTasksLinePos, "SCHED TASKS: ");
    AppendUInt(schedulerTasksLine, sizeof(schedulerTasksLine), schedulerTasksLinePos, schedulerStats.TotalTaskCount);
    FKernelCommandConsole::PushSystemLog(schedulerTasksLine);

    FMessageBusStats messageBusStats{};
    FMessageBus::GetStats(messageBusStats);
    char messageBusLine[80] = {};
    size_t messageBusLinePos = 0;
    AppendString(messageBusLine, sizeof(messageBusLine), messageBusLinePos, "MSG BUS CAP: ");
    AppendUInt(messageBusLine, sizeof(messageBusLine), messageBusLinePos, messageBusStats.Capacity);
    FKernelCommandConsole::PushSystemLog(messageBusLine);

    FEventManagerStats eventStats{};
    FEventManager::GetStats(eventStats);
    char eventLine[80] = {};
    size_t eventLinePos = 0;
    AppendString(eventLine, sizeof(eventLine), eventLinePos, "EVENT SUBS: ");
    AppendUInt(eventLine, sizeof(eventLine), eventLinePos, eventStats.SubscriptionCount);
    FKernelCommandConsole::PushSystemLog(eventLine);

    FServiceRegistryStats serviceStats{};
    FServiceRegistry::GetStats(serviceStats);
    char serviceLine[80] = {};
    size_t serviceLinePos = 0;
    AppendString(serviceLine, sizeof(serviceLine), serviceLinePos, "SERVICES: ");
    AppendUInt(serviceLine, sizeof(serviceLine), serviceLinePos, serviceStats.ServiceCount);
    FKernelCommandConsole::PushSystemLog(serviceLine);

    FVirtualFileSystemStats vfsStats{};
    FVirtualFileSystem::GetStats(vfsStats);
    char vfsLine[96] = {};
    size_t vfsLinePos = 0;
    AppendString(vfsLine, sizeof(vfsLine), vfsLinePos, "VFS MOUNTS: ");
    AppendUInt(vfsLine, sizeof(vfsLine), vfsLinePos, vfsStats.MountCount);
    AppendString(vfsLine, sizeof(vfsLine), vfsLinePos, " CAP: ");
    AppendUInt(vfsLine, sizeof(vfsLine), vfsLinePos, vfsStats.Capacity);
    FKernelCommandConsole::PushSystemLog(vfsLine);

    FVirtualFileSystemRoute bootRoute{};
    if (FVirtualFileSystem::ResolvePath("/boot/kernel.elf", bootRoute) && bootRoute.Found) {
        char routeLine[96] = {};
        size_t routeLinePos = 0;
        AppendString(routeLine, sizeof(routeLine), routeLinePos, "VFS ROUTE: ");
        AppendString(routeLine, sizeof(routeLine), routeLinePos, bootRoute.DriverName);
        AppendString(routeLine, sizeof(routeLine), routeLinePos, " -> ");
        AppendString(routeLine, sizeof(routeLine), routeLinePos, bootRoute.RelativePath);
        FKernelCommandConsole::PushSystemLog(routeLine);
    }

    RunStorageSmokeDiagnostic();
    RunTextShaperSmokeDiagnostic();

    outContext = FKernelRuntimeContext{
        .DisplayManager = &GDisplayManager,
        .DisplayDevice = &GVideoDevice,
        .VideoDevice = &GVideoDevice,
        .Console = &GConsole,
        .Renderer3D = GRenderer3D,
        .CubeScene = &GCubeScene,
        .SubsystemState = {},
    };

    return true;
}

} // namespace Fortress::Kernel
