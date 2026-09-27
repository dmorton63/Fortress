#include "Fortress/Video/FVideoSurface.hpp"

#include "Fortress/Runtime/FRuntime.hpp"

namespace Fortress::Video {

static Fortress::Core::uint32 ExpandChannel(Fortress::Core::uint8 value, Fortress::Core::uint8 bits) {
    if (bits >= 8) {
        return value;
    }
    const Fortress::Core::uint32 maxIn = 255;
    const Fortress::Core::uint32 maxOut = (1u << bits) - 1u;
    return (static_cast<Fortress::Core::uint32>(value) * maxOut) / maxIn;
}

Fortress::Core::uint32 FVideoSurfaceOps::PackMasked32(FColor color, const FPixelMask32 &mask) {
    Fortress::Core::uint32 packed = 0;
    packed |= ExpandChannel(color.R, mask.RedMaskSize) << mask.RedMaskShift;
    packed |= ExpandChannel(color.G, mask.GreenMaskSize) << mask.GreenMaskShift;
    packed |= ExpandChannel(color.B, mask.BlueMaskSize) << mask.BlueMaskShift;
    return packed;
}

void FVideoSurfaceOps::Clear32(const FVideoSurfaceView &surface, Fortress::Core::uint32 packedColor) {
    if (!surface.IsValid() || surface.Desc.PixelFormat != EPixelFormat::Masked32) {
        return;
    }

    const Fortress::Core::usize stridePixels = surface.Desc.StrideBytes / sizeof(Fortress::Core::uint32);
    auto *pixels = static_cast<Fortress::Core::uint32 *>(surface.Pixels);
    for (Fortress::Core::usize y = 0; y < surface.Desc.Height; y++) {
        Fortress::Core::uint32 *row = pixels + y * stridePixels;
        for (Fortress::Core::usize x = 0; x < surface.Desc.Width; x++) {
            row[x] = packedColor;
        }
    }
}

void FVideoSurfaceOps::Blit32(const FVideoSurfaceView &destination,
                              const FVideoSurfaceView &source,
                              Fortress::Core::usize copyWidthPixels,
                              Fortress::Core::usize copyHeightPixels) {
    if (!destination.IsValid() || !source.IsValid()) {
        return;
    }

    if (destination.Desc.PixelFormat != EPixelFormat::Masked32 || source.Desc.PixelFormat != EPixelFormat::Masked32) {
        return;
    }

    if (copyWidthPixels == 0 || copyHeightPixels == 0) {
        return;
    }

    if (copyWidthPixels > destination.Desc.Width) {
        copyWidthPixels = destination.Desc.Width;
    }
    if (copyWidthPixels > source.Desc.Width) {
        copyWidthPixels = source.Desc.Width;
    }
    if (copyHeightPixels > destination.Desc.Height) {
        copyHeightPixels = destination.Desc.Height;
    }
    if (copyHeightPixels > source.Desc.Height) {
        copyHeightPixels = source.Desc.Height;
    }

    const Fortress::Core::usize destinationStridePixels = destination.Desc.StrideBytes / sizeof(Fortress::Core::uint32);
    const Fortress::Core::usize sourceStridePixels = source.Desc.StrideBytes / sizeof(Fortress::Core::uint32);
    auto *destinationPixels = static_cast<Fortress::Core::uint32 *>(destination.Pixels);
    auto *sourcePixels = static_cast<Fortress::Core::uint32 *>(source.Pixels);
    const Fortress::Core::usize copyRowBytes = copyWidthPixels * sizeof(Fortress::Core::uint32);

    for (Fortress::Core::usize y = 0; y < copyHeightPixels; y++) {
        Fortress::Runtime::Memcpy(destinationPixels + y * destinationStridePixels,
                                  sourcePixels + y * sourceStridePixels,
                                  copyRowBytes);
    }
}

void FVideoSurfaceOps::DrawPixel32(const FVideoSurfaceView &surface,
                                   Fortress::Core::int32 x,
                                   Fortress::Core::int32 y,
                                   Fortress::Core::uint32 packedColor) {
    if (!surface.IsValid() || surface.Desc.PixelFormat != EPixelFormat::Masked32) {
        return;
    }

    if (x < 0 || y < 0) {
        return;
    }

    if (static_cast<Fortress::Core::usize>(x) >= surface.Desc.Width ||
        static_cast<Fortress::Core::usize>(y) >= surface.Desc.Height) {
        return;
    }

    const Fortress::Core::usize stridePixels = surface.Desc.StrideBytes / sizeof(Fortress::Core::uint32);
    auto *pixels = static_cast<Fortress::Core::uint32 *>(surface.Pixels);
    pixels[static_cast<Fortress::Core::usize>(y) * stridePixels + static_cast<Fortress::Core::usize>(x)] = packedColor;
}

void FVideoSurfaceOps::DrawLine32(const FVideoSurfaceView &surface,
                                  Fortress::Core::int32 x0,
                                  Fortress::Core::int32 y0,
                                  Fortress::Core::int32 x1,
                                  Fortress::Core::int32 y1,
                                  Fortress::Core::uint32 packedColor) {
    Fortress::Core::int32 dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    Fortress::Core::int32 sx = (x0 < x1) ? 1 : -1;
    Fortress::Core::int32 dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    Fortress::Core::int32 sy = (y0 < y1) ? 1 : -1;
    Fortress::Core::int32 err = dx + dy;

    while (true) {
        DrawPixel32(surface, x0, y0, packedColor);
        if (x0 == x1 && y0 == y1) {
            break;
        }

        Fortress::Core::int32 e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

} // namespace Fortress::Video