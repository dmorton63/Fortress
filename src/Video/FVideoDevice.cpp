#include "Fortress/Video/FVideoDevice.hpp"

#include "Fortress/Memory/FMemoryArena.hpp"
#include "Fortress/Video/FVideoSurface.hpp"

namespace Fortress::Video {

static Fortress::Core::uint32 ExpandChannel(Fortress::Core::uint8 value, Fortress::Core::uint8 bits) {
    if (bits >= 8) {
        return value;
    }
    const Fortress::Core::uint32 maxIn = 255;
    const Fortress::Core::uint32 maxOut = (1u << bits) - 1u;
    return (static_cast<Fortress::Core::uint32>(value) * maxOut) / maxIn;
}

bool FVideoDevice::Initialize(limine_framebuffer *framebuffer) {
    if (framebuffer == nullptr || framebuffer->address == nullptr || framebuffer->bpp != 32) {
        Ready = false;
        return false;
    }

    Width = framebuffer->width;
    Height = framebuffer->height;
    PitchPixels = framebuffer->pitch / 4;

    RedMaskSize = framebuffer->red_mask_size;
    RedMaskShift = framebuffer->red_mask_shift;
    GreenMaskSize = framebuffer->green_mask_size;
    GreenMaskShift = framebuffer->green_mask_shift;
    BlueMaskSize = framebuffer->blue_mask_size;
    BlueMaskShift = framebuffer->blue_mask_shift;

    FrontBuffer = static_cast<Fortress::Core::uint32 *>(framebuffer->address);
    BackBuffer = static_cast<Fortress::Core::uint32 *>(
        Fortress::Memory::FMemoryArena::Allocate(PitchPixels * Height * sizeof(Fortress::Core::uint32), alignof(Fortress::Core::uint32)));

    if (BackBuffer == nullptr) {
        Ready = false;
        return false;
    }

    Ready = true;
    return true;
}

Fortress::Core::uint32 FVideoDevice::PackColor(FColor color) const {
    Fortress::Core::uint32 packed = 0;
    packed |= ExpandChannel(color.R, RedMaskSize) << RedMaskShift;
    packed |= ExpandChannel(color.G, GreenMaskSize) << GreenMaskShift;
    packed |= ExpandChannel(color.B, BlueMaskSize) << BlueMaskShift;
    return packed;
}

void FVideoDevice::Clear(FColor color) {
    FVideoSurfaceOps::Clear32(GetBackSurface(), PackColor(color));
}

void FVideoDevice::DrawPixel(Fortress::Core::int32 x, Fortress::Core::int32 y, FColor color) {
    if (x < 0 || y < 0) {
        return;
    }

    if (static_cast<Fortress::Core::usize>(x) >= Width || static_cast<Fortress::Core::usize>(y) >= Height) {
        return;
    }

    BackBuffer[static_cast<Fortress::Core::usize>(y) * PitchPixels + static_cast<Fortress::Core::usize>(x)] = PackColor(color);
}

void FVideoDevice::DrawLine(Fortress::Core::int32 x0, Fortress::Core::int32 y0, Fortress::Core::int32 x1, Fortress::Core::int32 y1, FColor color) {
    Fortress::Core::int32 dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    Fortress::Core::int32 sx = (x0 < x1) ? 1 : -1;
    Fortress::Core::int32 dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    Fortress::Core::int32 sy = (y0 < y1) ? 1 : -1;
    Fortress::Core::int32 err = dx + dy;

    while (true) {
        DrawPixel(x0, y0, color);
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

bool FVideoDevice::IsReady() const {
    return Ready;
}

FDisplayMode FVideoDevice::GetCurrentMode() const {
    return FDisplayMode{
        .Width = Width,
        .Height = Height,
        .StrideBytes = PitchPixels * sizeof(Fortress::Core::uint32),
        .PixelFormat = EPixelFormat::Masked32,
        .PixelMask = FPixelMask32{
            .RedMaskSize = RedMaskSize,
            .RedMaskShift = RedMaskShift,
            .GreenMaskSize = GreenMaskSize,
            .GreenMaskShift = GreenMaskShift,
            .BlueMaskSize = BlueMaskSize,
            .BlueMaskShift = BlueMaskShift,
        },
    };
}

FVideoSurfaceView FVideoDevice::GetFrontSurface() {
    return FVideoSurfaceView{
        .Pixels = FrontBuffer,
        .Desc = FVideoSurfaceDesc{
            .Width = Width,
            .Height = Height,
            .StrideBytes = PitchPixels * sizeof(Fortress::Core::uint32),
            .PixelFormat = EPixelFormat::Masked32,
            .PixelMask = FPixelMask32{
                .RedMaskSize = RedMaskSize,
                .RedMaskShift = RedMaskShift,
                .GreenMaskSize = GreenMaskSize,
                .GreenMaskShift = GreenMaskShift,
                .BlueMaskSize = BlueMaskSize,
                .BlueMaskShift = BlueMaskShift,
            },
        },
    };
}

FVideoSurfaceView FVideoDevice::GetBackSurface() {
    return FVideoSurfaceView{
        .Pixels = BackBuffer,
        .Desc = FVideoSurfaceDesc{
            .Width = Width,
            .Height = Height,
            .StrideBytes = PitchPixels * sizeof(Fortress::Core::uint32),
            .PixelFormat = EPixelFormat::Masked32,
            .PixelMask = FPixelMask32{
                .RedMaskSize = RedMaskSize,
                .RedMaskShift = RedMaskShift,
                .GreenMaskSize = GreenMaskSize,
                .GreenMaskShift = GreenMaskShift,
                .BlueMaskSize = BlueMaskSize,
                .BlueMaskShift = BlueMaskShift,
            },
        },
    };
}

void FVideoDevice::Present() {
    FVideoSurfaceOps::Blit32(GetFrontSurface(), GetBackSurface(), Width, Height);
}

Fortress::Core::uint32 *FVideoDevice::GetBackBuffer() {
    return BackBuffer;
}

Fortress::Core::usize FVideoDevice::GetWidth() const {
    return Width;
}

Fortress::Core::usize FVideoDevice::GetHeight() const {
    return Height;
}

Fortress::Core::usize FVideoDevice::GetPitchPixels() const {
    return PitchPixels;
}

} // namespace Fortress::Video
