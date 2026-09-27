#ifndef FORTRESS_KERNEL_FDESKTOPSURFACEOVERLAY_HPP
#define FORTRESS_KERNEL_FDESKTOPSURFACEOVERLAY_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Kernel/FDesktopCompositor.hpp"
#include "Fortress/Kernel/FDesktopInputRouter.hpp"
#include "Fortress/Video/FVideoSurface.hpp"

namespace Fortress::Kernel {

struct FDesktopOverlayFrameStats {
    Fortress::Core::uint64 AckCount = 0;
    Fortress::Core::uint64 AckPixels = 0;
    Fortress::Core::uint64 FallbackCount = 0;
    Fortress::Core::uint64 FallbackPixels = 0;
    Fortress::Core::uint64 SplitAttempts = 0;
    Fortress::Core::uint64 SplitFragmentsGenerated = 0;
    Fortress::Core::uint64 SplitFragmentsDropped = 0;

    Fortress::Core::uint32 TopDirtyCount = 0;
    FDesktopSurfaceId TopDirtySurfaceIds[3] = {};
    Fortress::Core::uint64 TopDirtyPixels[3] = {};

    Fortress::Core::uint32 TopSplitCount = 0;
    FDesktopSurfaceId TopSplitSurfaceIds[3] = {};
    Fortress::Core::uint64 TopSplitCounts[3] = {};
};

class FDesktopSurfaceOverlay {
  public:
    void Bind(FDesktopCompositor *compositor, FDesktopInputRouter *inputRouter);
    bool IsReady() const;

    void Render(const Fortress::Video::FVideoSurfaceView &surface);
    void GetFrameStats(FDesktopOverlayFrameStats &outStats) const;

  private:
    FDesktopCompositor *Compositor = nullptr;
    FDesktopInputRouter *InputRouter = nullptr;
    FDesktopOverlayFrameStats FrameStats{};
};

} // namespace Fortress::Kernel

#endif
