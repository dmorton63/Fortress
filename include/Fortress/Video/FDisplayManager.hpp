#ifndef FORTRESS_VIDEO_FDISPLAYMANAGER_HPP
#define FORTRESS_VIDEO_FDISPLAYMANAGER_HPP

#include "Fortress/Video/FDisplayDevice.hpp"

namespace Fortress::Video {

struct FDisplayFrameContext {
  FDisplayMode Mode{};
  FVideoSurfaceView BackSurface{};
  bool ModeChanged = false;

  bool IsValid() const {
    return BackSurface.IsValid() &&
         BackSurface.Desc.PixelFormat == EPixelFormat::Masked32 &&
         Mode.Width != 0 &&
         Mode.Height != 0;
  }
};

class FDisplayManager {
  public:
    bool Initialize(FDisplayDevice *inDevice);

    bool IsReady() const;
    const FDisplayMode &GetMode() const;
  FDisplayFrameContext BeginFrame();
    FVideoSurfaceView AcquireBackSurface();
    void Present();

  private:
    bool RefreshMode();

    FDisplayDevice *Device = nullptr;
    FDisplayMode Mode{};
    bool Ready = false;
};

} // namespace Fortress::Video

#endif