#ifndef FORTRESS_KERNEL_FDESKTOPINPUTROUTER_HPP
#define FORTRESS_KERNEL_FDESKTOPINPUTROUTER_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Kernel/FDesktopCompositor.hpp"
#include "Fortress/Kernel/FEventManager.hpp"

namespace Fortress::Kernel {

struct FDesktopInputRouterStats {
  FDesktopSurfaceId FocusSurfaceId = DesktopInvalidSurfaceId;
  FDesktopSurfaceId CaptureSurfaceId = DesktopInvalidSurfaceId;
    Fortress::Core::uint64 RoutedKeyCount = 0;
    Fortress::Core::uint64 DroppedKeyCount = 0;
    Fortress::Core::uint64 FocusChangeCount = 0;
    Fortress::Core::uint64 CaptureChangeCount = 0;
    Fortress::Core::uint32 LastRoutedKeyAscii = 0;
    Fortress::Core::uint64 PointerSampleCount = 0;
    Fortress::Core::uint64 PointerFocusClickCount = 0;
};

  struct FDesktopSurfaceInputStats {
    FDesktopSurfaceId SurfaceId = DesktopInvalidSurfaceId;
    Fortress::Core::uint64 RoutedKeyCount = 0;
    Fortress::Core::uint64 PointerFocusClickCount = 0;
    Fortress::Core::uint32 LastRoutedKeyAscii = 0;
  };

class FDesktopInputRouter {
  public:
    struct FPolicyConfig {
        bool RaiseOnFocusChange = true;
        bool CaptureOnPointerFocus = true;
        bool ReleaseCaptureOnPointerRelease = true;
    };

    bool Initialize(FDesktopCompositor *compositor);
    bool IsReady() const;
    void SetPolicyConfig(const FPolicyConfig &policyConfig);

    bool SetFocus(FDesktopSurfaceId surfaceId);
    bool FocusNext();
    bool SetCapture(FDesktopSurfaceId surfaceId);
    void ReleaseCapture();

    void HandleInputEvent(const FKernelEvent &event);
    void HandlePointerSample(Fortress::Core::int32 x, Fortress::Core::int32 y, bool leftButtonDown);
    void GetStats(FDesktopInputRouterStats &outStats) const;
    bool GetSurfaceInputStats(FDesktopSurfaceId surfaceId, FDesktopSurfaceInputStats &outStats) const;

  private:
    static constexpr Fortress::Core::uint32 MaxTrackedSurfaceStats = 32u;

    struct FTrackedSurfaceInputStats {
        bool InUse = false;
        FDesktopSurfaceInputStats Stats = {};
    };

    Fortress::Core::int32 FindTrackedSurfaceIndex(FDesktopSurfaceId surfaceId) const;
    Fortress::Core::int32 FindOrAllocateTrackedSurfaceIndex(FDesktopSurfaceId surfaceId);

    FDesktopCompositor *Compositor = nullptr;
    FDesktopSurfaceId FocusSurfaceId = DesktopInvalidSurfaceId;
    FDesktopSurfaceId CaptureSurfaceId = DesktopInvalidSurfaceId;
    Fortress::Core::uint64 RoutedKeyCount = 0;
    Fortress::Core::uint64 DroppedKeyCount = 0;
    Fortress::Core::uint64 FocusChangeCount = 0;
    Fortress::Core::uint64 CaptureChangeCount = 0;
    Fortress::Core::uint32 LastRoutedKeyAscii = 0;
    Fortress::Core::uint64 PointerSampleCount = 0;
    Fortress::Core::uint64 PointerFocusClickCount = 0;
    FTrackedSurfaceInputStats TrackedSurfaceStats[MaxTrackedSurfaceStats] = {};
    bool PreviousLeftButtonDown = false;
    FPolicyConfig PolicyConfig = {};
    bool Ready = false;
};

} // namespace Fortress::Kernel

#endif