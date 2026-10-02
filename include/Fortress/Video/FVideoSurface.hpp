#ifndef FORTRESS_VIDEO_FVIDEOSURFACE_HPP
#define FORTRESS_VIDEO_FVIDEOSURFACE_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Video/FColor.hpp"

namespace Fortress::Video {

enum class EPixelFormat : Fortress::Core::uint8 {
    Unknown = 0,
    Masked32,
};

struct FPixelMask32 {
    Fortress::Core::uint8 RedMaskSize = 8;
    Fortress::Core::uint8 RedMaskShift = 16;
    Fortress::Core::uint8 GreenMaskSize = 8;
    Fortress::Core::uint8 GreenMaskShift = 8;
    Fortress::Core::uint8 BlueMaskSize = 8;
    Fortress::Core::uint8 BlueMaskShift = 0;
};

struct FVideoSurfaceDesc {
    Fortress::Core::usize Width = 0;
    Fortress::Core::usize Height = 0;
    Fortress::Core::usize StrideBytes = 0;
    EPixelFormat PixelFormat = EPixelFormat::Unknown;
    FPixelMask32 PixelMask{};
};

struct FVideoSurfaceView {
    void *Pixels = nullptr;
    FVideoSurfaceDesc Desc{};

    bool IsValid() const {
        return Pixels != nullptr && Desc.Width > 0 && Desc.Height > 0 && Desc.StrideBytes > 0;
    }
};

class FVideoSurfaceOps {
  public:
        static Fortress::Core::uint32 PackMasked32(FColor color, const FPixelMask32 &mask);
    static void Clear32(const FVideoSurfaceView &surface, Fortress::Core::uint32 packedColor);
    static void Blit32(const FVideoSurfaceView &destination,
                       const FVideoSurfaceView &source,
                       Fortress::Core::usize copyWidthPixels,
                       Fortress::Core::usize copyHeightPixels);
        static void DrawPixel32(const FVideoSurfaceView &surface,
                                                        Fortress::Core::int32 x,
                                                        Fortress::Core::int32 y,
                                                        Fortress::Core::uint32 packedColor);
        static void BlendPixel32(const FVideoSurfaceView &surface,
                                                         Fortress::Core::int32 x,
                                                         Fortress::Core::int32 y,
                                                         FColor srcColor);
        static void FillRect32(const FVideoSurfaceView &surface,
                                                     Fortress::Core::int32 x,
                                                     Fortress::Core::int32 y,
                                                     Fortress::Core::int32 width,
                                                     Fortress::Core::int32 height,
                                                     Fortress::Core::uint32 packedColor);
        static void FillRectAlpha32(const FVideoSurfaceView &surface,
                                                                Fortress::Core::int32 x,
                                                                Fortress::Core::int32 y,
                                                                Fortress::Core::int32 width,
                                                                Fortress::Core::int32 height,
                                                                FColor srcColor);
        static void FillGradientVertical32(const FVideoSurfaceView &surface,
                                                                             Fortress::Core::int32 x,
                                                                             Fortress::Core::int32 y,
                                                                             Fortress::Core::int32 width,
                                                                             Fortress::Core::int32 height,
                                                                             FColor topColor,
                                                                             FColor bottomColor);
        static void FillGradientHorizontal32(const FVideoSurfaceView &surface,
                                                                                 Fortress::Core::int32 x,
                                                                                 Fortress::Core::int32 y,
                                                                                 Fortress::Core::int32 width,
                                                                                 Fortress::Core::int32 height,
                                                                                 FColor leftColor,
                                                                                 FColor rightColor);
        static void FillGradientVerticalAlpha32(const FVideoSurfaceView &surface,
                                                Fortress::Core::int32 x,
                                                Fortress::Core::int32 y,
                                                Fortress::Core::int32 width,
                                                Fortress::Core::int32 height,
                                                FColor topColor,
                                                FColor bottomColor);
        static void FillGradientHorizontalAlpha32(const FVideoSurfaceView &surface,
                                                  Fortress::Core::int32 x,
                                                  Fortress::Core::int32 y,
                                                  Fortress::Core::int32 width,
                                                  Fortress::Core::int32 height,
                                                  FColor leftColor,
                                                  FColor rightColor);
        static void DrawLine32(const FVideoSurfaceView &surface,
                                                     Fortress::Core::int32 x0,
                                                     Fortress::Core::int32 y0,
                                                     Fortress::Core::int32 x1,
                                                     Fortress::Core::int32 y1,
                                                     Fortress::Core::uint32 packedColor);
};

} // namespace Fortress::Video

#endif