#ifndef FORTRESS_VIDEO_FVIDEODEVICE_HPP
#define FORTRESS_VIDEO_FVIDEODEVICE_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Video/FColor.hpp"
#include "Fortress/Video/FDisplayDevice.hpp"
#include "limine.h"

namespace Fortress::Video {

class FVideoDevice : public FDisplayDevice {
  public:
    bool Initialize(limine_framebuffer *framebuffer);

    void Clear(FColor color);
    void DrawPixel(Fortress::Core::int32 x, Fortress::Core::int32 y, FColor color);
    void DrawLine(Fortress::Core::int32 x0, Fortress::Core::int32 y0, Fortress::Core::int32 x1, Fortress::Core::int32 y1, FColor color);

    bool IsReady() const override;
    FDisplayMode GetCurrentMode() const override;
    FVideoSurfaceView GetFrontSurface() override;
    FVideoSurfaceView GetBackSurface() override;
    void Present() override;

    Fortress::Core::uint32 PackColor(FColor color) const;

    Fortress::Core::uint32 *GetBackBuffer();
    Fortress::Core::usize GetWidth() const;
    Fortress::Core::usize GetHeight() const;
    Fortress::Core::usize GetPitchPixels() const;

  private:
    Fortress::Core::uint32 *FrontBuffer = nullptr;
    Fortress::Core::uint32 *BackBuffer = nullptr;

    Fortress::Core::usize Width = 0;
    Fortress::Core::usize Height = 0;
    Fortress::Core::usize PitchPixels = 0;

    Fortress::Core::uint8 RedMaskSize = 8;
    Fortress::Core::uint8 RedMaskShift = 16;
    Fortress::Core::uint8 GreenMaskSize = 8;
    Fortress::Core::uint8 GreenMaskShift = 8;
    Fortress::Core::uint8 BlueMaskSize = 8;
    Fortress::Core::uint8 BlueMaskShift = 0;
    bool Ready = false;
};

} // namespace Fortress::Video

#endif
