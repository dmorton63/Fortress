#include "Fortress/Kernel/FDesktopInputRouter.hpp"

#include "Fortress/Kernel/FDesktopSurfaceContentHost.hpp"
#include "Fortress/Kernel/FKernelRuntimeIds.hpp"

namespace Fortress::Kernel {

Fortress::Core::int32 FDesktopInputRouter::FindTrackedSurfaceIndex(FDesktopSurfaceId surfaceId) const {
    for (Fortress::Core::uint32 i = 0; i < MaxTrackedSurfaceStats; i++) {
        if (TrackedSurfaceStats[i].InUse && TrackedSurfaceStats[i].Stats.SurfaceId == surfaceId) {
            return static_cast<Fortress::Core::int32>(i);
        }
    }

    return -1;
}

Fortress::Core::int32 FDesktopInputRouter::FindOrAllocateTrackedSurfaceIndex(FDesktopSurfaceId surfaceId) {
    const Fortress::Core::int32 existingIndex = FindTrackedSurfaceIndex(surfaceId);
    if (existingIndex >= 0) {
        return existingIndex;
    }

    for (Fortress::Core::uint32 i = 0; i < MaxTrackedSurfaceStats; i++) {
        if (!TrackedSurfaceStats[i].InUse) {
            TrackedSurfaceStats[i] = FTrackedSurfaceInputStats{
                .InUse = true,
                .Stats = FDesktopSurfaceInputStats{.SurfaceId = surfaceId},
            };
            return static_cast<Fortress::Core::int32>(i);
        }
    }

    return -1;
}

bool FDesktopInputRouter::Initialize(FDesktopCompositor *compositor) {
    if (compositor == nullptr || !compositor->IsReady()) {
        return false;
    }

    Compositor = compositor;
    ContentHost = nullptr;
    FocusSurfaceId = DesktopInvalidSurfaceId;
    CaptureSurfaceId = DesktopInvalidSurfaceId;
    RoutedKeyCount = 0;
    DroppedKeyCount = 0;
    FocusChangeCount = 0;
    FocusRejectCount = 0;
    CaptureChangeCount = 0;
    CaptureStaleDropCount = 0;
    LastRoutedKeyAscii = 0;
    PointerSampleCount = 0;
    PointerPressEdgeCount = 0;
    PointerHitSurfaceCount = 0;
    PointerFocusClickCount = 0;
    PointerFocusFailCount = 0;
    for (Fortress::Core::uint32 i = 0; i < MaxTrackedSurfaceStats; i++) {
        TrackedSurfaceStats[i] = FTrackedSurfaceInputStats{};
    }
    PreviousLeftButtonDown = false;
    PolicyConfig = FPolicyConfig{};
    Ready = true;
    return true;
}

bool FDesktopInputRouter::IsReady() const {
    return Ready;
}

void FDesktopInputRouter::SetPolicyConfig(const FPolicyConfig &policyConfig) {
    PolicyConfig = policyConfig;
}

void FDesktopInputRouter::BindSurfaceContentHost(FDesktopSurfaceContentHost *contentHost) {
    ContentHost = contentHost;
}

bool FDesktopInputRouter::SetFocus(FDesktopSurfaceId surfaceId) {
    if (!Ready || surfaceId == DesktopInvalidSurfaceId || !Compositor->IsSurfaceFocusable(surfaceId)) {
        FocusRejectCount++;
        return false;
    }

    if (PolicyConfig.RaiseOnFocusChange) {
        if (!Compositor->RaiseSurface(surfaceId)) {
            return false;
        }
    }

    FocusSurfaceId = surfaceId;
    FocusChangeCount++;
    if (ContentHost != nullptr) {
        (void)ContentHost->HandleSurfaceFocus(surfaceId);
    }
    return true;
}

bool FDesktopInputRouter::FocusNext() {
    if (!Ready) {
        return false;
    }

    FDesktopSurfaceId nextSurfaceId = DesktopInvalidSurfaceId;
    if (!Compositor->GetNextFocusableSurfaceId(FocusSurfaceId, nextSurfaceId)) {
        return false;
    }

    return SetFocus(nextSurfaceId);
}

bool FDesktopInputRouter::SetCapture(FDesktopSurfaceId surfaceId) {
    if (!Ready || surfaceId == DesktopInvalidSurfaceId || !Compositor->SurfaceExists(surfaceId)) {
        return false;
    }

    CaptureSurfaceId = surfaceId;
    CaptureChangeCount++;
    return true;
}

void FDesktopInputRouter::ReleaseCapture() {
    if (!Ready) {
        return;
    }

    if (CaptureSurfaceId != DesktopInvalidSurfaceId) {
        CaptureSurfaceId = DesktopInvalidSurfaceId;
        CaptureChangeCount++;
    }
}

void FDesktopInputRouter::HandleInputEvent(const FKernelEvent &event) {
    if (!Ready) {
        return;
    }

    if (event.EventId != Fortress::Kernel::FKernelRuntimeIds::EventInputKeyPressed ||
        event.SourceServiceId != Fortress::Kernel::FKernelRuntimeIds::ServiceKeyboardInput) {
        return;
    }

    if (CaptureSurfaceId != DesktopInvalidSurfaceId && !Compositor->IsSurfaceFocusable(CaptureSurfaceId)) {
        CaptureSurfaceId = DesktopInvalidSurfaceId;
        CaptureChangeCount++;
        CaptureStaleDropCount++;
    }

    const Fortress::Core::uint32 keyAscii = (event.Arg0 & 0xFFu);
    LastRoutedKeyAscii = keyAscii;

    if (!Compositor->IsSurfaceFocusable(FocusSurfaceId)) {
        FDesktopSurfaceId fallbackFocus = DesktopInvalidSurfaceId;
        if (Compositor->GetFocusableSurfaceId(fallbackFocus)) {
            (void)SetFocus(fallbackFocus);
        } else {
            FocusSurfaceId = DesktopInvalidSurfaceId;
        }
    }

    const FDesktopSurfaceId targetSurface = FocusSurfaceId;

    if (targetSurface == DesktopInvalidSurfaceId) {
        DroppedKeyCount++;
        return;
    }

    if (keyAscii == 9u) {
        if (ContentHost != nullptr && ContentHost->FocusNextControl(targetSurface)) {
            RoutedKeyCount++;
            return;
        }
        (void)FocusNext();
        return;
    }

    if (keyAscii == 11u) {
        if (ContentHost != nullptr && ContentHost->FocusPreviousControl(targetSurface)) {
            RoutedKeyCount++;
            return;
        }
    }

    if (ContentHost != nullptr) {
        (void)ContentHost->HandleSurfaceKeyPress(targetSurface, keyAscii);
    }

    RoutedKeyCount++;
    const Fortress::Core::int32 trackedIndex = FindOrAllocateTrackedSurfaceIndex(targetSurface);
    if (trackedIndex >= 0) {
        TrackedSurfaceStats[trackedIndex].Stats.RoutedKeyCount++;
        TrackedSurfaceStats[trackedIndex].Stats.LastRoutedKeyAscii = keyAscii;
    }
    (void)Compositor->MarkSurfaceDamaged(targetSurface,
                                         FDesktopRect{.X = 8, .Y = 8, .Width = 48, .Height = 24});
}

void FDesktopInputRouter::HandlePointerSample(Fortress::Core::int32 x,
                                              Fortress::Core::int32 y,
                                              bool leftButtonDown) {
    if (!Ready) {
        return;
    }

    PointerSampleCount++;
    const bool leftPressEdge = leftButtonDown && !PreviousLeftButtonDown;
    PreviousLeftButtonDown = leftButtonDown;
    if (leftPressEdge) {
        PointerPressEdgeCount++;
    }

    if (PolicyConfig.ReleaseCaptureOnPointerRelease &&
        !leftButtonDown &&
        CaptureSurfaceId != DesktopInvalidSurfaceId) {
        ReleaseCapture();
    }

    if (!leftPressEdge) {
        return;
    }

    FDesktopSurfaceId hitSurfaceId = DesktopInvalidSurfaceId;
    const bool haveTopHit = Compositor->GetTopSurfaceAtPoint(x, y, hitSurfaceId);
    if (!haveTopHit) {
        if (ContentHost != nullptr && FocusSurfaceId != DesktopInvalidSurfaceId) {
            const bool fallbackDispatched = ContentHost->HandleSurfaceFocusedControlPress(FocusSurfaceId, x, y);
            if (fallbackDispatched) {
                PointerHitSurfaceCount++;
                PointerFocusClickCount++;
                const Fortress::Core::int32 trackedIndex = FindOrAllocateTrackedSurfaceIndex(FocusSurfaceId);
                if (trackedIndex >= 0) {
                    TrackedSurfaceStats[trackedIndex].Stats.PointerFocusClickCount++;
                }

                if (PolicyConfig.CaptureOnPointerFocus) {
                    (void)SetCapture(FocusSurfaceId);
                }
            }
        }
        return;
    }
    PointerHitSurfaceCount++;

    if (!SetFocus(hitSurfaceId)) {
        PointerFocusFailCount++;
    }

    bool controlDispatched = false;
    if (ContentHost != nullptr) {
        controlDispatched = ContentHost->HandleSurfacePointerPress(hitSurfaceId, x, y);
    }

    if (controlDispatched) {
        PointerFocusClickCount++;
        const Fortress::Core::int32 trackedIndex = FindOrAllocateTrackedSurfaceIndex(hitSurfaceId);
        if (trackedIndex >= 0) {
            TrackedSurfaceStats[trackedIndex].Stats.PointerFocusClickCount++;
        }
    }

    if (PolicyConfig.CaptureOnPointerFocus) {
        (void)SetCapture(hitSurfaceId);
    }
}

void FDesktopInputRouter::GetStats(FDesktopInputRouterStats &outStats) const {
    outStats = FDesktopInputRouterStats{
        .FocusSurfaceId = FocusSurfaceId,
        .CaptureSurfaceId = CaptureSurfaceId,
        .RoutedKeyCount = RoutedKeyCount,
        .DroppedKeyCount = DroppedKeyCount,
        .FocusChangeCount = FocusChangeCount,
        .FocusRejectCount = FocusRejectCount,
        .CaptureChangeCount = CaptureChangeCount,
        .CaptureStaleDropCount = CaptureStaleDropCount,
        .LastRoutedKeyAscii = LastRoutedKeyAscii,
        .PointerSampleCount = PointerSampleCount,
        .PointerPressEdgeCount = PointerPressEdgeCount,
        .PointerHitSurfaceCount = PointerHitSurfaceCount,
        .PointerFocusClickCount = PointerFocusClickCount,
        .PointerFocusFailCount = PointerFocusFailCount,
    };
}

bool FDesktopInputRouter::GetSurfaceInputStats(FDesktopSurfaceId surfaceId, FDesktopSurfaceInputStats &outStats) const {
    if (!Ready || surfaceId == DesktopInvalidSurfaceId) {
        return false;
    }

    const Fortress::Core::int32 trackedIndex = FindTrackedSurfaceIndex(surfaceId);
    if (trackedIndex < 0) {
        return false;
    }

    outStats = TrackedSurfaceStats[trackedIndex].Stats;
    return true;
}

} // namespace Fortress::Kernel