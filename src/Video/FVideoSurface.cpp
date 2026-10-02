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

static Fortress::Core::uint8 CompressChannel(Fortress::Core::uint32 value, Fortress::Core::uint8 bits) {
    if (bits >= 8u) {
        return static_cast<Fortress::Core::uint8>(value & 0xFFu);
    }
    const Fortress::Core::uint32 maxIn = (1u << bits) - 1u;
    if (maxIn == 0u) {
        return 0u;
    }
    return static_cast<Fortress::Core::uint8>((value * 255u) / maxIn);
}

static Fortress::Core::uint8 ExtractChannel(const Fortress::Core::uint32 packed,
                                            const Fortress::Core::uint8 bits,
                                            const Fortress::Core::uint8 shift) {
    if (bits == 0u || shift >= 32u) {
        return 0u;
    }
    const Fortress::Core::uint32 mask = (1u << bits) - 1u;
    const Fortress::Core::uint32 raw = (packed >> shift) & mask;
    return CompressChannel(raw, bits);
}

Fortress::Core::uint32 FVideoSurfaceOps::PackMasked32(FColor color, const FPixelMask32 &mask) {
    if (mask.RedMaskSize == 0u && mask.GreenMaskSize == 0u && mask.BlueMaskSize == 0u) {
        return (static_cast<Fortress::Core::uint32>(color.R) << 16u) |
               (static_cast<Fortress::Core::uint32>(color.G) << 8u) |
               static_cast<Fortress::Core::uint32>(color.B);
    }

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

void FVideoSurfaceOps::BlendPixel32(const FVideoSurfaceView &surface,
                                    Fortress::Core::int32 x,
                                    Fortress::Core::int32 y,
                                    FColor srcColor) {
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

    const Fortress::Core::uint8 alpha = srcColor.A;
    if (alpha == 0u) {
        return;
    }

    const Fortress::Core::usize stridePixels = surface.Desc.StrideBytes / sizeof(Fortress::Core::uint32);
    auto *pixels = static_cast<Fortress::Core::uint32 *>(surface.Pixels);
    Fortress::Core::uint32 &dstPacked =
        pixels[static_cast<Fortress::Core::usize>(y) * stridePixels + static_cast<Fortress::Core::usize>(x)];

    if (alpha == 255u) {
        dstPacked = PackMasked32(srcColor, surface.Desc.PixelMask);
        return;
    }

    const Fortress::Core::uint8 dstR = ExtractChannel(dstPacked,
                                                      surface.Desc.PixelMask.RedMaskSize,
                                                      surface.Desc.PixelMask.RedMaskShift);
    const Fortress::Core::uint8 dstG = ExtractChannel(dstPacked,
                                                      surface.Desc.PixelMask.GreenMaskSize,
                                                      surface.Desc.PixelMask.GreenMaskShift);
    const Fortress::Core::uint8 dstB = ExtractChannel(dstPacked,
                                                      surface.Desc.PixelMask.BlueMaskSize,
                                                      surface.Desc.PixelMask.BlueMaskShift);

    const Fortress::Core::uint32 invA = 255u - static_cast<Fortress::Core::uint32>(alpha);
    // Use rounded alpha lerp to reduce visible gradient/banding artifacts.
    const Fortress::Core::uint8 outR = static_cast<Fortress::Core::uint8>(
        (static_cast<Fortress::Core::uint32>(srcColor.R) * alpha +
         static_cast<Fortress::Core::uint32>(dstR) * invA + 127u) /
        255u);
    const Fortress::Core::uint8 outG = static_cast<Fortress::Core::uint8>(
        (static_cast<Fortress::Core::uint32>(srcColor.G) * alpha +
         static_cast<Fortress::Core::uint32>(dstG) * invA + 127u) /
        255u);
    const Fortress::Core::uint8 outB = static_cast<Fortress::Core::uint8>(
        (static_cast<Fortress::Core::uint32>(srcColor.B) * alpha +
         static_cast<Fortress::Core::uint32>(dstB) * invA + 127u) /
        255u);

    dstPacked = PackMasked32(FColor{.R = outR, .G = outG, .B = outB, .A = 255u}, surface.Desc.PixelMask);
}

void FVideoSurfaceOps::FillRect32(const FVideoSurfaceView &surface,
                                  Fortress::Core::int32 x,
                                  Fortress::Core::int32 y,
                                  Fortress::Core::int32 width,
                                  Fortress::Core::int32 height,
                                  Fortress::Core::uint32 packedColor) {
    if (!surface.IsValid() || surface.Desc.PixelFormat != EPixelFormat::Masked32 || width <= 0 || height <= 0) {
        return;
    }

    const Fortress::Core::int32 surfaceWidth = static_cast<Fortress::Core::int32>(surface.Desc.Width);
    const Fortress::Core::int32 surfaceHeight = static_cast<Fortress::Core::int32>(surface.Desc.Height);

    Fortress::Core::int32 startX = x;
    Fortress::Core::int32 startY = y;
    Fortress::Core::int32 endX = x + width;
    Fortress::Core::int32 endY = y + height;

    if (startX < 0) {
        startX = 0;
    }
    if (startY < 0) {
        startY = 0;
    }
    if (endX > surfaceWidth) {
        endX = surfaceWidth;
    }
    if (endY > surfaceHeight) {
        endY = surfaceHeight;
    }

    if (startX >= endX || startY >= endY) {
        return;
    }

    const Fortress::Core::usize stridePixels = surface.Desc.StrideBytes / sizeof(Fortress::Core::uint32);
    auto *pixels = static_cast<Fortress::Core::uint32 *>(surface.Pixels);
    for (Fortress::Core::int32 py = startY; py < endY; py++) {
        Fortress::Core::uint32 *row = pixels + static_cast<Fortress::Core::usize>(py) * stridePixels;
        for (Fortress::Core::int32 px = startX; px < endX; px++) {
            row[static_cast<Fortress::Core::usize>(px)] = packedColor;
        }
    }
}

void FVideoSurfaceOps::FillRectAlpha32(const FVideoSurfaceView &surface,
                                       Fortress::Core::int32 x,
                                       Fortress::Core::int32 y,
                                       Fortress::Core::int32 width,
                                       Fortress::Core::int32 height,
                                       FColor srcColor) {
    if (!surface.IsValid() || surface.Desc.PixelFormat != EPixelFormat::Masked32 || width <= 0 || height <= 0) {
        return;
    }

    const Fortress::Core::int32 surfaceWidth = static_cast<Fortress::Core::int32>(surface.Desc.Width);
    const Fortress::Core::int32 surfaceHeight = static_cast<Fortress::Core::int32>(surface.Desc.Height);

    Fortress::Core::int32 startX = x;
    Fortress::Core::int32 startY = y;
    Fortress::Core::int32 endX = x + width;
    Fortress::Core::int32 endY = y + height;

    if (startX < 0) {
        startX = 0;
    }
    if (startY < 0) {
        startY = 0;
    }
    if (endX > surfaceWidth) {
        endX = surfaceWidth;
    }
    if (endY > surfaceHeight) {
        endY = surfaceHeight;
    }

    if (startX >= endX || startY >= endY) {
        return;
    }

    for (Fortress::Core::int32 py = startY; py < endY; py++) {
        for (Fortress::Core::int32 px = startX; px < endX; px++) {
            BlendPixel32(surface, px, py, srcColor);
        }
    }
}

void FVideoSurfaceOps::FillGradientVertical32(const FVideoSurfaceView &surface,
                                              Fortress::Core::int32 x,
                                              Fortress::Core::int32 y,
                                              Fortress::Core::int32 width,
                                              Fortress::Core::int32 height,
                                              FColor topColor,
                                              FColor bottomColor) {
    if (!surface.IsValid() || surface.Desc.PixelFormat != EPixelFormat::Masked32 || width <= 0 || height <= 0) {
        return;
    }

    const Fortress::Core::int32 denom = (height > 1) ? (height - 1) : 1;
    for (Fortress::Core::int32 py = 0; py < height; py++) {
        const Fortress::Core::int32 r = static_cast<Fortress::Core::int32>(topColor.R) +
            ((static_cast<Fortress::Core::int32>(bottomColor.R) - static_cast<Fortress::Core::int32>(topColor.R)) * py) / denom;
        const Fortress::Core::int32 g = static_cast<Fortress::Core::int32>(topColor.G) +
            ((static_cast<Fortress::Core::int32>(bottomColor.G) - static_cast<Fortress::Core::int32>(topColor.G)) * py) / denom;
        const Fortress::Core::int32 b = static_cast<Fortress::Core::int32>(topColor.B) +
            ((static_cast<Fortress::Core::int32>(bottomColor.B) - static_cast<Fortress::Core::int32>(topColor.B)) * py) / denom;

        const Fortress::Core::uint32 packed = PackMasked32(FColor::RGB(static_cast<Fortress::Core::uint8>(r),
                                                                        static_cast<Fortress::Core::uint8>(g),
                                                                        static_cast<Fortress::Core::uint8>(b)),
                                                           surface.Desc.PixelMask);
        FillRect32(surface, x, y + py, width, 1, packed);
    }
}

void FVideoSurfaceOps::FillGradientHorizontal32(const FVideoSurfaceView &surface,
                                                Fortress::Core::int32 x,
                                                Fortress::Core::int32 y,
                                                Fortress::Core::int32 width,
                                                Fortress::Core::int32 height,
                                                FColor leftColor,
                                                FColor rightColor) {
    if (!surface.IsValid() || surface.Desc.PixelFormat != EPixelFormat::Masked32 || width <= 0 || height <= 0) {
        return;
    }

    const Fortress::Core::int32 denom = (width > 1) ? (width - 1) : 1;
    for (Fortress::Core::int32 px = 0; px < width; px++) {
        const Fortress::Core::int32 r = static_cast<Fortress::Core::int32>(leftColor.R) +
            ((static_cast<Fortress::Core::int32>(rightColor.R) - static_cast<Fortress::Core::int32>(leftColor.R)) * px) / denom;
        const Fortress::Core::int32 g = static_cast<Fortress::Core::int32>(leftColor.G) +
            ((static_cast<Fortress::Core::int32>(rightColor.G) - static_cast<Fortress::Core::int32>(leftColor.G)) * px) / denom;
        const Fortress::Core::int32 b = static_cast<Fortress::Core::int32>(leftColor.B) +
            ((static_cast<Fortress::Core::int32>(rightColor.B) - static_cast<Fortress::Core::int32>(leftColor.B)) * px) / denom;

        const Fortress::Core::uint32 packed = PackMasked32(FColor::RGB(static_cast<Fortress::Core::uint8>(r),
                                                                        static_cast<Fortress::Core::uint8>(g),
                                                                        static_cast<Fortress::Core::uint8>(b)),
                                                           surface.Desc.PixelMask);
        FillRect32(surface, x + px, y, 1, height, packed);
    }
}

void FVideoSurfaceOps::FillGradientVerticalAlpha32(const FVideoSurfaceView &surface,
                                                   Fortress::Core::int32 x,
                                                   Fortress::Core::int32 y,
                                                   Fortress::Core::int32 width,
                                                   Fortress::Core::int32 height,
                                                   FColor topColor,
                                                   FColor bottomColor) {
    if (!surface.IsValid() || surface.Desc.PixelFormat != EPixelFormat::Masked32 || width <= 0 || height <= 0) {
        return;
    }

    const Fortress::Core::int32 denom = (height > 1) ? (height - 1) : 1;
    for (Fortress::Core::int32 py = 0; py < height; py++) {
        const Fortress::Core::int32 r = static_cast<Fortress::Core::int32>(topColor.R) +
            ((static_cast<Fortress::Core::int32>(bottomColor.R) - static_cast<Fortress::Core::int32>(topColor.R)) * py) / denom;
        const Fortress::Core::int32 g = static_cast<Fortress::Core::int32>(topColor.G) +
            ((static_cast<Fortress::Core::int32>(bottomColor.G) - static_cast<Fortress::Core::int32>(topColor.G)) * py) / denom;
        const Fortress::Core::int32 b = static_cast<Fortress::Core::int32>(topColor.B) +
            ((static_cast<Fortress::Core::int32>(bottomColor.B) - static_cast<Fortress::Core::int32>(topColor.B)) * py) / denom;
        const Fortress::Core::int32 a = static_cast<Fortress::Core::int32>(topColor.A) +
            ((static_cast<Fortress::Core::int32>(bottomColor.A) - static_cast<Fortress::Core::int32>(topColor.A)) * py) / denom;

        FillRectAlpha32(surface,
                        x,
                        y + py,
                        width,
                        1,
                        FColor{.R = static_cast<Fortress::Core::uint8>(r),
                               .G = static_cast<Fortress::Core::uint8>(g),
                               .B = static_cast<Fortress::Core::uint8>(b),
                               .A = static_cast<Fortress::Core::uint8>(a)});
    }
}

void FVideoSurfaceOps::FillGradientHorizontalAlpha32(const FVideoSurfaceView &surface,
                                                     Fortress::Core::int32 x,
                                                     Fortress::Core::int32 y,
                                                     Fortress::Core::int32 width,
                                                     Fortress::Core::int32 height,
                                                     FColor leftColor,
                                                     FColor rightColor) {
    if (!surface.IsValid() || surface.Desc.PixelFormat != EPixelFormat::Masked32 || width <= 0 || height <= 0) {
        return;
    }

    const Fortress::Core::int32 denom = (width > 1) ? (width - 1) : 1;
    for (Fortress::Core::int32 px = 0; px < width; px++) {
        const Fortress::Core::int32 r = static_cast<Fortress::Core::int32>(leftColor.R) +
            ((static_cast<Fortress::Core::int32>(rightColor.R) - static_cast<Fortress::Core::int32>(leftColor.R)) * px) / denom;
        const Fortress::Core::int32 g = static_cast<Fortress::Core::int32>(leftColor.G) +
            ((static_cast<Fortress::Core::int32>(rightColor.G) - static_cast<Fortress::Core::int32>(leftColor.G)) * px) / denom;
        const Fortress::Core::int32 b = static_cast<Fortress::Core::int32>(leftColor.B) +
            ((static_cast<Fortress::Core::int32>(rightColor.B) - static_cast<Fortress::Core::int32>(leftColor.B)) * px) / denom;
        const Fortress::Core::int32 a = static_cast<Fortress::Core::int32>(leftColor.A) +
            ((static_cast<Fortress::Core::int32>(rightColor.A) - static_cast<Fortress::Core::int32>(leftColor.A)) * px) / denom;

        FillRectAlpha32(surface,
                        x + px,
                        y,
                        1,
                        height,
                        FColor{.R = static_cast<Fortress::Core::uint8>(r),
                               .G = static_cast<Fortress::Core::uint8>(g),
                               .B = static_cast<Fortress::Core::uint8>(b),
                               .A = static_cast<Fortress::Core::uint8>(a)});
    }
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