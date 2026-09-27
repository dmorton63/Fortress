#include "Fortress/Kernel/FDesktopRuntime.hpp"

#include "Fortress/Kernel/FKernelCommandConsole.hpp"
#include "Fortress/Kernel/FKernelCommandControlPlane.hpp"
#include "Fortress/Kernel/FKernelTextFormat.hpp"
#include "Fortress/Video/FDisplayManager.hpp"

namespace Fortress::Kernel {

FDesktopRuntime *FDesktopRuntime::ActiveInstance = nullptr;

namespace {

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

} // namespace

bool FDesktopRuntime::Initialize(FKernelRuntimeContext &runtime) {
    Ready = false;
    DesktopTickCount = 0u;
    ShellPolicy.ResetDefaults();
    OcclusionProbeSurfaceAId = 0u;
    OcclusionProbeSurfaceBId = 0u;
    OcclusionProbeSurfaceCId = 0u;
    OcclusionProbeCleanupPending = false;
    if (runtime.DisplayManager == nullptr || !runtime.DisplayManager->IsReady()) {
        FKernelCommandConsole::PushSystemLog("DESKTOP INIT FAIL: DISPLAY");
        return false;
    }

    const Fortress::Video::FDisplayMode mode = runtime.DisplayManager->GetMode();
    if (!Compositor.Initialize(mode.Width, mode.Height)) {
        FKernelCommandConsole::PushSystemLog("DESKTOP INIT FAIL: COMPOSITOR");
        return false;
    }

    if (!Shell.Initialize(&Compositor)) {
        FKernelCommandConsole::PushSystemLog("DESKTOP INIT FAIL: SHELL");
        return false;
    }

    if (!InputRouter.Initialize(&Compositor)) {
        FKernelCommandConsole::PushSystemLog("DESKTOP INIT FAIL: INPUT");
        return false;
    }

    InputRouter.SetPolicyConfig(FDesktopInputRouter::FPolicyConfig{
        .RaiseOnFocusChange = ShellPolicy.ShouldRaiseOnFocusChange(),
        .CaptureOnPointerFocus = ShellPolicy.ShouldCaptureOnPointerFocus(),
        .ReleaseCaptureOnPointerRelease = ShellPolicy.ShouldReleaseCaptureOnPointerRelease(),
    });

    Overlay.Bind(&Compositor, &InputRouter);
    ActiveInstance = this;

    const FDesktopShellComponent hudComponent{
        .Name = "HudSurface",
        .OnAttach = &FDesktopRuntime::DesktopHudAttach,
        .OnTick = &FDesktopRuntime::DesktopHudTick,
    };
    if (!Shell.RegisterComponent(hudComponent)) {
        FKernelCommandConsole::PushSystemLog("DESKTOP INIT FAIL: COMPONENT");
        return false;
    }

    const FDesktopShellComponent statusComponent{
        .Name = "StatusSurface",
        .OnAttach = &FDesktopRuntime::DesktopStatusAttach,
        .OnTick = &FDesktopRuntime::DesktopStatusTick,
    };
    if (!Shell.RegisterComponent(statusComponent)) {
        FKernelCommandConsole::PushSystemLog("DESKTOP INIT FAIL: COMPONENT2");
        return false;
    }

    FKernelCommandConsole::BindDesktopCompositor(&Compositor);
    FKernelCommandConsole::BindDesktopInputRouter(&InputRouter);

    if (DesktopHudSurfaceId == 0u) {
        Fortress::Kernel::FDesktopSurfaceId surfaceId = 0u;
        if (Compositor.CreateSurface(DesktopInvalidSurfaceId,
                                     FDesktopRect{.X = 8, .Y = 8, .Width = 360, .Height = 220},
                                     10u,
                                     surfaceId)) {
            DesktopHudSurfaceId = surfaceId;
        }
    }

    if (DesktopStatusSurfaceId == 0u) {
        Fortress::Kernel::FDesktopSurfaceId surfaceId = 0u;
        if (Compositor.CreateSurface(DesktopInvalidSurfaceId,
                                     FDesktopRect{.X = 420, .Y = 18, .Width = 220, .Height = 120},
                                     20u,
                                     surfaceId)) {
            DesktopStatusSurfaceId = surfaceId;
        }
    }

    if (ShellPolicy.ShouldAssignInitialFocusOnInitialize()) {
        if (!InputRouter.FocusNext()) {
            FKernelCommandConsole::PushSystemLog("DESKTOP INIT INFO: NO FOCUS");
        }
    }

    const bool desktopStatusSmokeOk =
        Compositor.SetSurfaceVisible(DesktopStatusSurfaceId, false) &&
        Compositor.SetSurfaceVisible(DesktopStatusSurfaceId, true) &&
        Compositor.MarkSurfaceDamaged(DesktopStatusSurfaceId,
                                      FDesktopRect{.X = 420, .Y = 18, .Width = 220, .Height = 120});
    if (!desktopStatusSmokeOk) {
        FKernelCommandConsole::PushSystemLog("DESKTOP SMOKE WARN: STATUS");
    }

    Fortress::Kernel::FDesktopSurfaceId smokeSurfaceId = Fortress::Kernel::DesktopInvalidSurfaceId;
    const bool dsksurfLifecycleOk =
        Compositor.CreateSurface(DesktopInvalidSurfaceId,
                                 FDesktopRect{.X = 240, .Y = 220, .Width = 96, .Height = 64},
                                 30u,
                                 smokeSurfaceId) &&
        (smokeSurfaceId != Fortress::Kernel::DesktopInvalidSurfaceId) &&
        Compositor.MoveSurface(smokeSurfaceId, 268, 236) &&
        Compositor.ResizeSurface(smokeSurfaceId, 112, 72) &&
        Compositor.MarkSurfaceDamaged(smokeSurfaceId,
                                      FDesktopRect{.X = 268, .Y = 236, .Width = 112, .Height = 72}) &&
        Compositor.CloseSurface(smokeSurfaceId);

    if (!dsksurfLifecycleOk) {
        FKernelCommandConsole::PushSystemLog("DSKSURF SMOKE FAIL");
        return false;
    }

    const bool occlusionProbeOk =
        Compositor.CreateSurface(DesktopInvalidSurfaceId,
                                 FDesktopRect{.X = 120, .Y = 120, .Width = 360, .Height = 240},
                                 40u,
                                 OcclusionProbeSurfaceAId) &&
        Compositor.CreateSurface(DesktopInvalidSurfaceId,
                                 FDesktopRect{.X = 220, .Y = 180, .Width = 280, .Height = 180},
                                 50u,
                                 OcclusionProbeSurfaceBId) &&
        Compositor.CreateSurface(DesktopInvalidSurfaceId,
                                 FDesktopRect{.X = 300, .Y = 210, .Width = 220, .Height = 160},
                                 60u,
                                 OcclusionProbeSurfaceCId) &&
        Compositor.MarkSurfaceDamaged(OcclusionProbeSurfaceAId,
                                      FDesktopRect{.X = 120, .Y = 120, .Width = 360, .Height = 240}) &&
        Compositor.MarkSurfaceDamaged(OcclusionProbeSurfaceBId,
                                      FDesktopRect{.X = 220, .Y = 180, .Width = 280, .Height = 180});

    if (!occlusionProbeOk) {
        FKernelCommandConsole::PushSystemLog("DSKSURF OCCLUSION SMOKE FAIL");
        return false;
    }

    // Ensure one known visible surface exercises fallback repaint in the first profiled frame.
    Compositor.ClearSurfaceDirty(DesktopHudSurfaceId);
    OcclusionProbeCleanupPending = true;

    FKernelCommandConsole::PushSystemLog("DESKTOP SMOKE PASS");
    FKernelCommandConsole::PushSystemLog("DSKSURF SMOKE PASS");
    FKernelCommandConsole::PushSystemLog("DSKSURF OCCLUSION SMOKE READY");
    FKernelCommandConsole::PushSystemLog("DSKSURF OVERLAY ON");
    FKernelCommandConsole::PushSystemLog("DESKTOP SHELL READY");

    NextDesktopStatsTick = 2u;
    Ready = true;
    return true;
}

bool FDesktopRuntime::IsReady() const {
    return Ready;
}

void FDesktopRuntime::HandleInputEvent(const FKernelEvent &event) {
    if (!Ready) {
        return;
    }

    InputRouter.HandleInputEvent(event);
}

void FDesktopRuntime::Tick(const FKernelRuntimeContext &runtime) {
    if (!Ready) {
        return;
    }

    DesktopTickCount++;
    RoutePointerSample(runtime);
    Shell.Tick();
    TryLogStats();
}

FDesktopCompositor &FDesktopRuntime::GetCompositor() {
    return Compositor;
}

FDesktopInputRouter &FDesktopRuntime::GetInputRouter() {
    return InputRouter;
}

FDesktopShell &FDesktopRuntime::GetShell() {
    return Shell;
}

FDesktopSurfaceOverlay &FDesktopRuntime::GetOverlay() {
    return Overlay;
}

void FDesktopRuntime::DesktopHudAttach(FDesktopCompositor &compositor) {
    if (ActiveInstance != nullptr) {
        ActiveInstance->OnDesktopHudAttach(compositor);
    }
}

void FDesktopRuntime::DesktopHudTick(FDesktopCompositor &compositor, Fortress::Core::uint64 tickCount) {
    if (ActiveInstance != nullptr) {
        ActiveInstance->OnDesktopHudTick(compositor, tickCount);
    }
}

void FDesktopRuntime::DesktopStatusAttach(FDesktopCompositor &compositor) {
    if (ActiveInstance != nullptr) {
        ActiveInstance->OnDesktopStatusAttach(compositor);
    }
}

void FDesktopRuntime::DesktopStatusTick(FDesktopCompositor &compositor, Fortress::Core::uint64 tickCount) {
    if (ActiveInstance != nullptr) {
        ActiveInstance->OnDesktopStatusTick(compositor, tickCount);
    }
}

void FDesktopRuntime::OnDesktopHudAttach(FDesktopCompositor &compositor) {
    if (DesktopHudSurfaceId != 0u) {
        return;
    }

    Fortress::Kernel::FDesktopSurfaceId surfaceId = 0u;
    if (compositor.CreateSurface(DesktopInvalidSurfaceId,
                                 FDesktopRect{.X = 8, .Y = 8, .Width = 360, .Height = 220},
                                 10u,
                                 surfaceId)) {
        DesktopHudSurfaceId = surfaceId;
    }
}

void FDesktopRuntime::OnDesktopHudTick(FDesktopCompositor &compositor, Fortress::Core::uint64 tickCount) {
    if (DesktopHudSurfaceId == 0u) {
        return;
    }

    if ((tickCount % 30u) == 0u) {
        (void)compositor.MarkSurfaceDamaged(DesktopHudSurfaceId,
                                            FDesktopRect{.X = 8, .Y = 8, .Width = 360, .Height = 220});
    }
}

void FDesktopRuntime::OnDesktopStatusAttach(FDesktopCompositor &compositor) {
    if (DesktopStatusSurfaceId != 0u) {
        return;
    }

    Fortress::Kernel::FDesktopSurfaceId surfaceId = 0u;
    if (compositor.CreateSurface(DesktopInvalidSurfaceId,
                                 FDesktopRect{.X = 420, .Y = 18, .Width = 220, .Height = 120},
                                 20u,
                                 surfaceId)) {
        DesktopStatusSurfaceId = surfaceId;
    }
}

void FDesktopRuntime::OnDesktopStatusTick(FDesktopCompositor &compositor, Fortress::Core::uint64 tickCount) {
    if (DesktopStatusSurfaceId == 0u) {
        return;
    }

    if ((tickCount % 45u) == 0u) {
        (void)compositor.MarkSurfaceDamaged(DesktopStatusSurfaceId,
                                            FDesktopRect{.X = 420, .Y = 18, .Width = 220, .Height = 120});
    }
}

void FDesktopRuntime::CleanupOcclusionProbeSurfaces() {
    if (!OcclusionProbeCleanupPending) {
        return;
    }

    (void)Compositor.CloseSurface(OcclusionProbeSurfaceCId);
    (void)Compositor.CloseSurface(OcclusionProbeSurfaceBId);
    (void)Compositor.CloseSurface(OcclusionProbeSurfaceAId);

    OcclusionProbeSurfaceAId = 0u;
    OcclusionProbeSurfaceBId = 0u;
    OcclusionProbeSurfaceCId = 0u;
    OcclusionProbeCleanupPending = false;
    FKernelCommandConsole::PushSystemLog("DSKSURF OCCLUSION SMOKE PASS");
}

void FDesktopRuntime::RoutePointerSample(const FKernelRuntimeContext &runtime) {
    if (!ShellPolicy.ShouldRoutePointerFocus()) {
        return;
    }

    if (!InputRouter.IsReady() || runtime.DisplayManager == nullptr || !runtime.DisplayManager->IsReady()) {
        return;
    }

    Fortress::Core::int32 hidNormX = 500;
    Fortress::Core::int32 hidNormY = 500;
    if (!FKernelCommandConsole::GetHidCursorNormalized(hidNormX, hidNormY)) {
        return;
    }

    if (FKernelCommandConsole::IsCursorInvertX()) {
        hidNormX = 1000 - hidNormX;
    }
    if (FKernelCommandConsole::IsCursorInvertY()) {
        hidNormY = 1000 - hidNormY;
    }

    const Fortress::Core::int32 sensitivity =
        static_cast<Fortress::Core::int32>(FKernelCommandConsole::GetCursorSensitivityPercent());
    hidNormX = 500 + ((hidNormX - 500) * sensitivity) / 100;
    hidNormY = 500 + ((hidNormY - 500) * sensitivity) / 100;
    hidNormX = ClampInt32(hidNormX, 0, 1000);
    hidNormY = ClampInt32(hidNormY, 0, 1000);

    const Fortress::Video::FDisplayMode mode = runtime.DisplayManager->GetMode();
    Fortress::Core::int32 cursorX = 0;
    Fortress::Core::int32 cursorY = 0;
    if (!NormalizeToScreenCoordinates(mode, hidNormX, hidNormY, cursorX, cursorY)) {
        return;
    }

    bool leftDown = false;
    bool rightDown = false;
    bool middleDown = false;
    FKernelCommandConsole::GetHidButtonsDown(leftDown, rightDown, middleDown);
    InputRouter.HandlePointerSample(cursorX, cursorY, leftDown);
}

void FDesktopRuntime::TryLogStats() {
    if (!Compositor.IsReady()) {
        return;
    }

    if (DesktopTickCount < NextDesktopStatsTick) {
        return;
    }

    FDesktopCompositorStats compositorStats{};
    Compositor.GetStats(compositorStats);
    FDesktopShellStats shellStats{};
    if (Shell.IsReady()) {
        Shell.GetStats(shellStats);
    }
    FDesktopInputRouterStats inputStats{};
    if (InputRouter.IsReady()) {
        InputRouter.GetStats(inputStats);
    }
    FDesktopOverlayFrameStats frameStats{};
    Overlay.GetFrameStats(frameStats);

    char line[320] = {};
    size_t pos = 0;
    FKernelTextFormat::AppendString(line, sizeof(line), pos, "DESKTOP S ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, compositorStats.SurfaceCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " D ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, compositorStats.DirtySurfaceCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " DA ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, compositorStats.DirtyPixelArea);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " ACK ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, compositorStats.DirtyAcknowledgeCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " APX ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, compositorStats.DirtyAcknowledgePixels);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " FACK ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, frameStats.AckCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " FPX ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, frameStats.AckPixels);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " FFBK ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, frameStats.FallbackCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " FFBP ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, frameStats.FallbackPixels);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " FSPL ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, frameStats.SplitAttempts);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " FGEN ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, frameStats.SplitFragmentsGenerated);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " FDROP ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, frameStats.SplitFragmentsDropped);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " Z ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, compositorStats.HighestZOrder);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " C ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, shellStats.ComponentCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " T ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, shellStats.TickCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " F ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, static_cast<Fortress::Core::uint64>(inputStats.FocusSurfaceId));
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " CAP ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, static_cast<Fortress::Core::uint64>(inputStats.CaptureSurfaceId));
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " CLK ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, inputStats.PointerFocusClickCount);
    FKernelCommandConsole::PushSystemLog(line);

    char dirtyLine[220] = {};
    pos = 0;
    FKernelTextFormat::AppendString(dirtyLine, sizeof(dirtyLine), pos, "DESKTOP DIRTY TOP N ");
    FKernelTextFormat::AppendUInt(dirtyLine, sizeof(dirtyLine), pos, frameStats.TopDirtyCount);
    for (Fortress::Core::uint32 i = 0; i < frameStats.TopDirtyCount; i++) {
        FKernelTextFormat::AppendString(dirtyLine, sizeof(dirtyLine), pos, " ID ");
        FKernelTextFormat::AppendUInt(dirtyLine,
                                      sizeof(dirtyLine),
                                      pos,
                                      static_cast<Fortress::Core::uint64>(frameStats.TopDirtySurfaceIds[i]));
        FKernelTextFormat::AppendString(dirtyLine, sizeof(dirtyLine), pos, " PX ");
        FKernelTextFormat::AppendUInt(dirtyLine, sizeof(dirtyLine), pos, frameStats.TopDirtyPixels[i]);
    }
    FKernelCommandConsole::PushSystemLog(dirtyLine);

    char splitLine[220] = {};
    pos = 0;
    FKernelTextFormat::AppendString(splitLine, sizeof(splitLine), pos, "DESKTOP SPLIT TOP N ");
    FKernelTextFormat::AppendUInt(splitLine, sizeof(splitLine), pos, frameStats.TopSplitCount);
    for (Fortress::Core::uint32 i = 0; i < frameStats.TopSplitCount; i++) {
        FKernelTextFormat::AppendString(splitLine, sizeof(splitLine), pos, " ID ");
        FKernelTextFormat::AppendUInt(splitLine,
                                      sizeof(splitLine),
                                      pos,
                                      static_cast<Fortress::Core::uint64>(frameStats.TopSplitSurfaceIds[i]));
        FKernelTextFormat::AppendString(splitLine, sizeof(splitLine), pos, " CNT ");
        FKernelTextFormat::AppendUInt(splitLine, sizeof(splitLine), pos, frameStats.TopSplitCounts[i]);
    }
    FKernelCommandConsole::PushSystemLog(splitLine);

    char inputLine[128] = {};
    pos = 0;
    FKernelTextFormat::AppendString(inputLine, sizeof(inputLine), pos, "DESKTOP INPUT RX ");
    FKernelTextFormat::AppendUInt(inputLine, sizeof(inputLine), pos, inputStats.RoutedKeyCount);
    FKernelTextFormat::AppendString(inputLine, sizeof(inputLine), pos, " DROP ");
    FKernelTextFormat::AppendUInt(inputLine, sizeof(inputLine), pos, inputStats.DroppedKeyCount);
    FKernelTextFormat::AppendString(inputLine, sizeof(inputLine), pos, " KEY ");
    FKernelTextFormat::AppendUInt(inputLine, sizeof(inputLine), pos, inputStats.LastRoutedKeyAscii);
    FKernelTextFormat::AppendString(inputLine, sizeof(inputLine), pos, " PTR ");
    FKernelTextFormat::AppendUInt(inputLine, sizeof(inputLine), pos, inputStats.PointerSampleCount);
    FKernelCommandConsole::PushSystemLog(inputLine);

    CleanupOcclusionProbeSurfaces();

    NextDesktopStatsTick = DesktopTickCount + DesktopStatsLogIntervalTicks;
}

} // namespace Fortress::Kernel
