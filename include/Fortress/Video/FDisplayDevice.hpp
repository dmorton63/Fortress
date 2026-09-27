#ifndef FORTRESS_VIDEO_FDISPLAYDEVICE_HPP
#define FORTRESS_VIDEO_FDISPLAYDEVICE_HPP

#include "Fortress/Video/FVideoSurface.hpp"

namespace Fortress::Video {

struct FDisplayMode {
    Fortress::Core::usize Width = 0;
    Fortress::Core::usize Height = 0;
    Fortress::Core::usize StrideBytes = 0;
    EPixelFormat PixelFormat = EPixelFormat::Unknown;
    FPixelMask32 PixelMask{};
};

class FDisplayDevice {
  public:
    virtual bool IsReady() const = 0;
    virtual FDisplayMode GetCurrentMode() const = 0;
    virtual FVideoSurfaceView GetFrontSurface() = 0;
    virtual FVideoSurfaceView GetBackSurface() = 0;
    virtual void Present() = 0;
};

} // namespace Fortress::Video

#endif