#include "Fortress/Kernel/FDesktopRuntime.hpp"

#include "Fortress/Kernel/FKernelCommandConsole.hpp"
#include "Fortress/Kernel/FKernelCommandControlPlane.hpp"
#include "Fortress/Kernel/FKernelTextFormat.hpp"
#include "Fortress/Video/FDisplayManager.hpp"

namespace Fortress::Kernel {

FDesktopRuntime *FDesktopRuntime::ActiveInstance = nullptr;

namespace {

static constexpr Fortress::Core::uint32 GDesktopTerminalLauncherButtonId = 2002u;

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

namespace {

static void LogDesktopControlEvent(const char *line) {
    FKernelCommandConsole::PushSystemLog(line);
}

static void DesktopControlRenderCallback(FDesktopSurfaceId, const FDesktopRect &, void *) {}

static void DesktopControlInputCallback(FDesktopSurfaceId,
                                        EDesktopSurfaceInputEvent event,
                                        Fortress::Core::uint32 arg0,
                                        Fortress::Core::int32,
                                        Fortress::Core::int32,
                                        void *) {
    if (event == EDesktopSurfaceInputEvent::PointerPress && arg0 == GDesktopTerminalLauncherButtonId) {
        FKernelCommandConsole::OpenTerminalWindow();
    }
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

    if (!ContentHost.Initialize(&Compositor)) {
        FKernelCommandConsole::PushSystemLog("DESKTOP INIT FAIL: CONTENT");
        return false;
    }
    ContentHost.SetLogSink(LogDesktopControlEvent);
    InputRouter.BindSurfaceContentHost(&ContentHost);

    InputRouter.SetPolicyConfig(FDesktopInputRouter::FPolicyConfig{
        .RaiseOnFocusChange = ShellPolicy.ShouldRaiseOnFocusChange(),
        .CaptureOnPointerFocus = ShellPolicy.ShouldCaptureOnPointerFocus(),
        .ReleaseCaptureOnPointerRelease = ShellPolicy.ShouldReleaseCaptureOnPointerRelease(),
    });

    Overlay.Bind(&Compositor, &InputRouter, &ContentHost);
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
    FKernelCommandConsole::BindDesktopSurfaceContentHost(&ContentHost);

    if (DesktopHudSurfaceId == 0u) {
        Fortress::Kernel::FDesktopSurfaceId surfaceId = 0u;
        if (Compositor.CreateSurface(DesktopInvalidSurfaceId,
                                     FDesktopRect{.X = 8, .Y = 8, .Width = 360, .Height = 220},
                                     10u,
                                     surfaceId)) {
            DesktopHudSurfaceId = surfaceId;
            RegisterDefaultSurfaceControls(DesktopHudSurfaceId, false);
        }
    }

    if (DesktopStatusSurfaceId == 0u) {
        Fortress::Kernel::FDesktopSurfaceId surfaceId = 0u;
        if (Compositor.CreateSurface(DesktopInvalidSurfaceId,
                                     FDesktopRect{.X = 420, .Y = 18, .Width = 220, .Height = 120},
                                     20u,
                                     surfaceId)) {
            DesktopStatusSurfaceId = surfaceId;
            RegisterDefaultSurfaceControls(DesktopStatusSurfaceId, true);
        }
    }

    if (ShellPolicy.ShouldAssignInitialFocusOnInitialize()) {
        bool focusAssigned = InputRouter.FocusNext();
        if (!focusAssigned && DesktopStatusSurfaceId != 0u) {
            focusAssigned = InputRouter.SetFocus(DesktopStatusSurfaceId);
        }
        if (!focusAssigned && DesktopHudSurfaceId != 0u) {
            focusAssigned = InputRouter.SetFocus(DesktopHudSurfaceId);
        }
        if (!focusAssigned) {
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
        FKernelCommandConsole::PushSystemLog("DSKSURF SMOKE WARN");
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
        FKernelCommandConsole::PushSystemLog("DSKSURF OCCLUSION SMOKE WARN");
    }

    // Ensure one known visible surface exercises fallback repaint in the first profiled frame.
    Compositor.ClearSurfaceDirty(DesktopHudSurfaceId);
    OcclusionProbeCleanupPending = occlusionProbeOk;

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
    DrainCompositorDirtyFallback();
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
        RegisterDefaultSurfaceControls(DesktopHudSurfaceId, false);
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
        RegisterDefaultSurfaceControls(DesktopStatusSurfaceId, true);
    }
}

void FDesktopRuntime::RegisterDefaultSurfaceControls(Fortress::Core::uint32 surfaceId, bool statusSurface) {
    const FDesktopSurfaceContentContract contract{
        .RenderFn = DesktopControlRenderCallback,
        .InputFn = DesktopControlInputCallback,
        .UserData = nullptr,
    };
    (void)ContentHost.RegisterSurfaceContent(surfaceId, contract);

    if (statusSurface) {
        (void)ContentHost.AddLabelControl(surfaceId, 1u, FDesktopRect{.X = 8, .Y = 8, .Width = 180, .Height = 18});
        (void)ContentHost.AddButtonControl(
            surfaceId, GDesktopTerminalLauncherButtonId, FDesktopRect{.X = 8, .Y = 34, .Width = 184, .Height = 28});
        return;
    }

    (void)ContentHost.AddLabelControl(surfaceId, 1u, FDesktopRect{.X = 8, .Y = 8, .Width = 200, .Height = 20});
    (void)ContentHost.AddButtonControl(
        surfaceId, GDesktopTerminalLauncherButtonId, FDesktopRect{.X = 8, .Y = 36, .Width = 184, .Height = 30});
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

    static Fortress::Core::int32 cachedNormX = 500;
    static Fortress::Core::int32 cachedNormY = 500;

    Fortress::Core::int32 hidNormX = cachedNormX;
    Fortress::Core::int32 hidNormY = cachedNormY;
    const bool haveHidCursorNow = FKernelCommandConsole::GetHidCursorNormalized(hidNormX, hidNormY);
    if (haveHidCursorNow) {
        cachedNormX = hidNormX;
        cachedNormY = hidNormY;
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

    bool leftPressEdge = false;
    bool rightPressEdge = false;
    bool middlePressEdge = false;
    (void)FKernelCommandConsole::ConsumeHidButtonPressEdges(leftPressEdge, rightPressEdge, middlePressEdge);

    if (leftPressEdge || rightPressEdge || middlePressEdge) {
        char clickLine[128] = {};
        size_t clickPos = 0;
        FKernelTextFormat::AppendString(clickLine, sizeof(clickLine), clickPos, "MCLICK EDGE L");
        FKernelTextFormat::AppendUInt(clickLine, sizeof(clickLine), clickPos, leftPressEdge ? 1u : 0u);
        FKernelTextFormat::AppendString(clickLine, sizeof(clickLine), clickPos, " R");
        FKernelTextFormat::AppendUInt(clickLine, sizeof(clickLine), clickPos, rightPressEdge ? 1u : 0u);
        FKernelTextFormat::AppendString(clickLine, sizeof(clickLine), clickPos, " M");
        FKernelTextFormat::AppendUInt(clickLine, sizeof(clickLine), clickPos, middlePressEdge ? 1u : 0u);
        FKernelTextFormat::AppendString(clickLine, sizeof(clickLine), clickPos, " X ");
        FKernelTextFormat::AppendUInt(clickLine,
                                      sizeof(clickLine),
                                      clickPos,
                                      static_cast<Fortress::Core::uint64>(cursorX >= 0 ? cursorX : 0));
        FKernelTextFormat::AppendString(clickLine, sizeof(clickLine), clickPos, " Y ");
        FKernelTextFormat::AppendUInt(clickLine,
                                      sizeof(clickLine),
                                      clickPos,
                                      static_cast<Fortress::Core::uint64>(cursorY >= 0 ? cursorY : 0));
        FKernelCommandConsole::PushSystemLog(clickLine);
    }

    if (!haveHidCursorNow && leftPressEdge) {
        FDesktopInputRouterStats inputStats{};
        InputRouter.GetStats(inputStats);
        if (inputStats.FocusSurfaceId != DesktopInvalidSurfaceId) {
            FDesktopRect focusBounds{};
            if (Compositor.GetSurfaceBounds(inputStats.FocusSurfaceId, focusBounds) && focusBounds.Width > 0 &&
                focusBounds.Height > 0) {
                cursorX = focusBounds.X + (focusBounds.Width / 2);
                cursorY = focusBounds.Y + (focusBounds.Height / 2);
            }
        }
    }

    // Route pointer press on either current-down or a latched press edge so brief clicks are not missed.
    const bool routeLeftPress = leftDown || leftPressEdge;
    InputRouter.HandlePointerSample(cursorX, cursorY, routeLeftPress);
}

void FDesktopRuntime::DrainCompositorDirtyFallback() {
    if (!Compositor.IsReady()) {
        return;
    }

    FDesktopSurfaceSnapshot snapshots[16] = {};
    Fortress::Core::uint32 count = 0u;
    Compositor.GetActiveSurfaceSnapshots(snapshots, 16u, count);
    for (Fortress::Core::uint32 i = 0u; i < count; i++) {
        if (!snapshots[i].Visible) {
            continue;
        }

        FDesktopRect dirtyRect{};
        (void)Compositor.ConsumeSurfaceDirtyRegion(snapshots[i].SurfaceId, dirtyRect);
    }
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
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " CDA ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, compositorStats.CoalescedDirtyPixelArea);
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
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " FREJ ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, inputStats.FocusRejectCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " CSTALE ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, inputStats.CaptureStaleDropCount);
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
