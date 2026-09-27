#include "Fortress/Video/FDisplayManager.hpp"

namespace Fortress::Video {

static bool IsModeValid(const FDisplayMode &mode) {
    return mode.Width != 0 && mode.Height != 0 && mode.StrideBytes != 0 && mode.PixelFormat != EPixelFormat::Unknown;
}

static bool IsModeEqual(const FDisplayMode &a, const FDisplayMode &b) {
    return a.Width == b.Width &&
           a.Height == b.Height &&
           a.StrideBytes == b.StrideBytes &&
           a.PixelFormat == b.PixelFormat &&
           a.PixelMask.RedMaskSize == b.PixelMask.RedMaskSize &&
           a.PixelMask.RedMaskShift == b.PixelMask.RedMaskShift &&
           a.PixelMask.GreenMaskSize == b.PixelMask.GreenMaskSize &&
           a.PixelMask.GreenMaskShift == b.PixelMask.GreenMaskShift &&
           a.PixelMask.BlueMaskSize == b.PixelMask.BlueMaskSize &&
           a.PixelMask.BlueMaskShift == b.PixelMask.BlueMaskShift;
}

bool FDisplayManager::Initialize(FDisplayDevice *inDevice) {
    Device = inDevice;
    Ready = false;
    Mode = FDisplayMode{};

    if (Device == nullptr || !Device->IsReady()) {
        return false;
    }

    Mode = Device->GetCurrentMode();
    if (!IsModeValid(Mode)) {
        return false;
    }

    Ready = true;
    return true;
}

bool FDisplayManager::IsReady() const {
    return Ready;
}

const FDisplayMode &FDisplayManager::GetMode() const {
    return Mode;
}

bool FDisplayManager::RefreshMode() {
    if (Device == nullptr || !Device->IsReady()) {
        return false;
    }

    const FDisplayMode currentMode = Device->GetCurrentMode();
    if (!IsModeValid(currentMode)) {
        return false;
    }

    Mode = currentMode;
    return true;
}

FDisplayFrameContext FDisplayManager::BeginFrame() {
    if (!Ready || Device == nullptr) {
        return FDisplayFrameContext{};
    }

    const FDisplayMode previousMode = Mode;
    if (!RefreshMode()) {
        return FDisplayFrameContext{};
    }

    return FDisplayFrameContext{
        .Mode = Mode,
        .BackSurface = Device->GetBackSurface(),
        .ModeChanged = !IsModeEqual(previousMode, Mode),
    };
}

FVideoSurfaceView FDisplayManager::AcquireBackSurface() {
    if (!Ready || Device == nullptr) {
        return FVideoSurfaceView{};
    }

    return Device->GetBackSurface();
}

void FDisplayManager::Present() {
    if (!Ready || Device == nullptr) {
        return;
    }

    Device->Present();
}

} // namespace Fortress::Video