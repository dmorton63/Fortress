#include "Fortress/Kernel/FDesktopSurfaceContentHost.hpp"

#include "Fortress/Kernel/FKernelTextFormat.hpp"
#include "Fortress/Video/FColor.hpp"

namespace Fortress::Kernel {

namespace {

static constexpr Fortress::Core::uint32 GDesktopTerminalLauncherButtonId = 2002u;

static bool IsRectValid(const FDesktopRect &rect) {
    return rect.Width > 0 && rect.Height > 0;
}

static bool PointInRect(Fortress::Core::int32 x, Fortress::Core::int32 y, const FDesktopRect &rect) {
    return x >= rect.X && y >= rect.Y && x < (rect.X + rect.Width) && y < (rect.Y + rect.Height);
}

static void DrawFilledRect(const Fortress::Video::FVideoSurfaceView &surface,
                           Fortress::Core::int32 x,
                           Fortress::Core::int32 y,
                           Fortress::Core::int32 width,
                           Fortress::Core::int32 height,
                           Fortress::Video::FColor color) {
    if (!surface.IsValid() || width <= 0 || height <= 0 ||
        surface.Desc.PixelFormat != Fortress::Video::EPixelFormat::Masked32) {
        return;
    }

    const Fortress::Core::uint32 packed = Fortress::Video::FVideoSurfaceOps::PackMasked32(color, surface.Desc.PixelMask);
    for (Fortress::Core::int32 py = 0; py < height; py++) {
        for (Fortress::Core::int32 px = 0; px < width; px++) {
            Fortress::Video::FVideoSurfaceOps::DrawPixel32(surface, x + px, y + py, packed);
        }
    }
}

static void DrawFilledRectAlpha(const Fortress::Video::FVideoSurfaceView &surface,
                                Fortress::Core::int32 x,
                                Fortress::Core::int32 y,
                                Fortress::Core::int32 width,
                                Fortress::Core::int32 height,
                                Fortress::Video::FColor color) {
    if (!surface.IsValid() || width <= 0 || height <= 0 ||
        surface.Desc.PixelFormat != Fortress::Video::EPixelFormat::Masked32) {
        return;
    }

    Fortress::Video::FVideoSurfaceOps::FillRectAlpha32(surface, x, y, width, height, color);
}

static void DrawRectFrame(const Fortress::Video::FVideoSurfaceView &surface,
                          const FDesktopRect &rect,
                          Fortress::Video::FColor color) {
    if (!surface.IsValid() || rect.Width <= 1 || rect.Height <= 1 ||
        surface.Desc.PixelFormat != Fortress::Video::EPixelFormat::Masked32) {
        return;
    }

    const Fortress::Core::uint32 packed = Fortress::Video::FVideoSurfaceOps::PackMasked32(color, surface.Desc.PixelMask);
    const Fortress::Core::int32 left = rect.X;
    const Fortress::Core::int32 top = rect.Y;
    const Fortress::Core::int32 right = rect.X + rect.Width - 1;
    const Fortress::Core::int32 bottom = rect.Y + rect.Height - 1;
    Fortress::Video::FVideoSurfaceOps::DrawLine32(surface, left, top, right, top, packed);
    Fortress::Video::FVideoSurfaceOps::DrawLine32(surface, left, bottom, right, bottom, packed);
    Fortress::Video::FVideoSurfaceOps::DrawLine32(surface, left, top, left, bottom, packed);
    Fortress::Video::FVideoSurfaceOps::DrawLine32(surface, right, top, right, bottom, packed);
}

} // namespace

bool FDesktopSurfaceContentHost::Initialize(FDesktopCompositor *compositor) {
    if (compositor == nullptr || !compositor->IsReady()) {
        return false;
    }

    Compositor = compositor;
    for (Fortress::Core::uint32 i = 0u; i < MaxSurfaceHosts; i++) {
        SurfaceStates[i] = FSurfaceControlState{};
    }
    Ready = true;
    return true;
}

bool FDesktopSurfaceContentHost::IsReady() const {
    return Ready;
}

void FDesktopSurfaceContentHost::SetLogSink(FLogSinkFn logSink) {
    LogSink = logSink;
}

Fortress::Core::int32 FDesktopSurfaceContentHost::FindSurfaceIndex(FDesktopSurfaceId surfaceId) const {
    for (Fortress::Core::uint32 i = 0u; i < MaxSurfaceHosts; i++) {
        if (SurfaceStates[i].InUse && SurfaceStates[i].SurfaceId == surfaceId) {
            return static_cast<Fortress::Core::int32>(i);
        }
    }
    return -1;
}

Fortress::Core::int32 FDesktopSurfaceContentHost::FindOrAllocateSurfaceIndex(FDesktopSurfaceId surfaceId) {
    const Fortress::Core::int32 existingIndex = FindSurfaceIndex(surfaceId);
    if (existingIndex >= 0) {
        return existingIndex;
    }

    for (Fortress::Core::uint32 i = 0u; i < MaxSurfaceHosts; i++) {
        if (!SurfaceStates[i].InUse) {
            SurfaceStates[i] = FSurfaceControlState{
                .InUse = true,
                .SurfaceId = surfaceId,
            };
            return static_cast<Fortress::Core::int32>(i);
        }
    }

    return -1;
}

bool FDesktopSurfaceContentHost::RegisterSurfaceContent(FDesktopSurfaceId surfaceId,
                                                        const FDesktopSurfaceContentContract &contract) {
    if (!Ready || surfaceId == DesktopInvalidSurfaceId || Compositor == nullptr || !Compositor->SurfaceExists(surfaceId)) {
        return false;
    }

    const Fortress::Core::int32 index = FindOrAllocateSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    SurfaceStates[index].Contract = contract;
    return true;
}

bool FDesktopSurfaceContentHost::UnregisterSurfaceContent(FDesktopSurfaceId surfaceId) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    SurfaceStates[index] = FSurfaceControlState{};
    return true;
}

bool FDesktopSurfaceContentHost::AddControl(FDesktopSurfaceId surfaceId,
                                            Fortress::Core::uint32 controlId,
                                            EDesktopControlType type,
                                            const FDesktopRect &localBounds,
                                            bool focusable) {
    if (!Ready || !IsRectValid(localBounds) || controlId == 0u) {
        return false;
    }

    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    FSurfaceControlState &state = SurfaceStates[index];
    if (state.ControlCount >= MaxControlsPerSurface) {
        return false;
    }

    const Fortress::Core::uint32 addIndex = state.ControlCount;
    state.Controls[addIndex] = FDesktopControlNode{
        .ControlId = controlId,
        .Type = type,
        .LocalBounds = localBounds,
        .Visible = true,
        .Enabled = true,
        .Focusable = focusable,
    };
    state.ControlCount++;

    if (focusable && state.FocusedControlIndex < 0) {
        state.FocusedControlIndex = static_cast<Fortress::Core::int32>(addIndex);
        state.Controls[addIndex].Focused = true;
    }

    return InvalidateControl(surfaceId, localBounds);
}

bool FDesktopSurfaceContentHost::AddLabelControl(FDesktopSurfaceId surfaceId,
                                                 Fortress::Core::uint32 controlId,
                                                 const FDesktopRect &localBounds) {
    return AddControl(surfaceId, controlId, EDesktopControlType::Label, localBounds, false);
}

bool FDesktopSurfaceContentHost::AddButtonControl(FDesktopSurfaceId surfaceId,
                                                  Fortress::Core::uint32 controlId,
                                                  const FDesktopRect &localBounds) {
    return AddControl(surfaceId, controlId, EDesktopControlType::Button, localBounds, true);
}

bool FDesktopSurfaceContentHost::SetFocusedControlIndex(FSurfaceControlState &state, Fortress::Core::int32 controlIndex) {
    if (controlIndex < 0 || static_cast<Fortress::Core::uint32>(controlIndex) >= state.ControlCount) {
        return false;
    }

    if (state.FocusedControlIndex == controlIndex) {
        return true;
    }

    if (state.FocusedControlIndex >= 0 &&
        static_cast<Fortress::Core::uint32>(state.FocusedControlIndex) < state.ControlCount) {
        state.Controls[state.FocusedControlIndex].Focused = false;
    }

    state.FocusedControlIndex = controlIndex;
    state.Controls[state.FocusedControlIndex].Focused = true;
    return true;
}

bool FDesktopSurfaceContentHost::FocusNextControl(FDesktopSurfaceId surfaceId) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    FSurfaceControlState &state = SurfaceStates[index];
    if (state.ControlCount == 0u) {
        return false;
    }

    Fortress::Core::int32 startIndex = state.FocusedControlIndex;
    if (startIndex < 0) {
        startIndex = 0;
    }

    for (Fortress::Core::uint32 step = 0u; step < state.ControlCount; step++) {
        const Fortress::Core::uint32 probe =
            static_cast<Fortress::Core::uint32>((startIndex + 1 + static_cast<Fortress::Core::int32>(step)) %
                                                static_cast<Fortress::Core::int32>(state.ControlCount));
        FDesktopControlNode &control = state.Controls[probe];
        if (!control.Visible || !control.Enabled || !control.Focusable) {
            continue;
        }

        if (SetFocusedControlIndex(state, static_cast<Fortress::Core::int32>(probe))) {
            LogControlEvent("CTRL FOCUS", surfaceId, control.ControlId, 0u);
            (void)InvalidateControl(surfaceId, control.LocalBounds);
            return true;
        }
    }

    return false;
}

bool FDesktopSurfaceContentHost::FocusPreviousControl(FDesktopSurfaceId surfaceId) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    FSurfaceControlState &state = SurfaceStates[index];
    if (state.ControlCount == 0u) {
        return false;
    }

    Fortress::Core::int32 startIndex = state.FocusedControlIndex;
    if (startIndex < 0) {
        startIndex = 0;
    }

    const Fortress::Core::int32 count = static_cast<Fortress::Core::int32>(state.ControlCount);
    for (Fortress::Core::uint32 step = 0u; step < state.ControlCount; step++) {
        Fortress::Core::int32 probe = startIndex - 1 - static_cast<Fortress::Core::int32>(step);
        while (probe < 0) {
            probe += count;
        }
        probe %= count;

        FDesktopControlNode &control = state.Controls[probe];
        if (!control.Visible || !control.Enabled || !control.Focusable) {
            continue;
        }

        if (SetFocusedControlIndex(state, probe)) {
            LogControlEvent("CTRL FOCUS", surfaceId, control.ControlId, 0u);
            (void)InvalidateControl(surfaceId, control.LocalBounds);
            return true;
        }
    }

    return false;
}

Fortress::Core::int32 FDesktopSurfaceContentHost::FindControlAtPoint(const FSurfaceControlState &state,
                                                                      const FDesktopRect &surfaceBounds,
                                                                      Fortress::Core::int32 x,
                                                                      Fortress::Core::int32 y) const {
    for (Fortress::Core::int32 i = static_cast<Fortress::Core::int32>(state.ControlCount) - 1; i >= 0; i--) {
        const FDesktopControlNode &control = state.Controls[i];
        if (!control.Visible || !control.Enabled) {
            continue;
        }

        const FDesktopRect globalRect{
            .X = surfaceBounds.X + control.LocalBounds.X,
            .Y = surfaceBounds.Y + control.LocalBounds.Y,
            .Width = control.LocalBounds.Width,
            .Height = control.LocalBounds.Height,
        };
        if (PointInRect(x, y, globalRect)) {
            return i;
        }
    }

    return -1;
}

bool FDesktopSurfaceContentHost::HandleSurfaceFocus(FDesktopSurfaceId surfaceId) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    FSurfaceControlState &state = SurfaceStates[index];
    if (state.FocusedControlIndex >= 0 &&
        static_cast<Fortress::Core::uint32>(state.FocusedControlIndex) < state.ControlCount &&
        state.Controls[state.FocusedControlIndex].Focusable) {
        return true;
    }

    for (Fortress::Core::uint32 i = 0u; i < state.ControlCount; i++) {
        if (!state.Controls[i].Visible || !state.Controls[i].Enabled || !state.Controls[i].Focusable) {
            continue;
        }

        (void)SetFocusedControlIndex(state, static_cast<Fortress::Core::int32>(i));
        LogControlEvent("CTRL FOCUS", surfaceId, state.Controls[i].ControlId, 1u);
        return true;
    }

    return false;
}

bool FDesktopSurfaceContentHost::HandleSurfacePointerPress(FDesktopSurfaceId surfaceId,
                                                           Fortress::Core::int32 x,
                                                           Fortress::Core::int32 y) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0 || Compositor == nullptr) {
        return false;
    }

    FDesktopRect surfaceBounds{};
    if (!Compositor->GetSurfaceBounds(surfaceId, surfaceBounds)) {
        return false;
    }

    FSurfaceControlState &state = SurfaceStates[index];
    const Fortress::Core::int32 hitIndex = FindControlAtPoint(state, surfaceBounds, x, y);
    if (hitIndex < 0) {
        return false;
    }

    FDesktopControlNode &control = state.Controls[hitIndex];
    if (control.Focusable) {
        (void)SetFocusedControlIndex(state, hitIndex);
    }

    if (control.Type == EDesktopControlType::Button) {
        control.Pressed = !control.Pressed;
    }

    if (state.Contract.InputFn != nullptr) {
        state.Contract.InputFn(surfaceId,
                               EDesktopSurfaceInputEvent::PointerPress,
                               control.ControlId,
                               x,
                               y,
                               state.Contract.UserData);
    }

    LogControlEvent("CTRL CLICK", surfaceId, control.ControlId, control.Pressed ? 1u : 0u);
    (void)InvalidateControl(surfaceId, control.LocalBounds);
    return true;
}

bool FDesktopSurfaceContentHost::HandleSurfaceFocusedControlPress(FDesktopSurfaceId surfaceId,
                                                                  Fortress::Core::int32 x,
                                                                  Fortress::Core::int32 y) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    FSurfaceControlState &state = SurfaceStates[index];
    if (state.FocusedControlIndex < 0 || static_cast<Fortress::Core::uint32>(state.FocusedControlIndex) >= state.ControlCount) {
        return false;
    }

    FDesktopControlNode &control = state.Controls[state.FocusedControlIndex];
    if (!control.Visible || !control.Enabled) {
        return false;
    }

    if (control.Type == EDesktopControlType::Button) {
        control.Pressed = !control.Pressed;
    }

    if (state.Contract.InputFn != nullptr) {
        state.Contract.InputFn(surfaceId,
                               EDesktopSurfaceInputEvent::PointerPress,
                               control.ControlId,
                               x,
                               y,
                               state.Contract.UserData);
    }

    LogControlEvent("CTRL CLICK", surfaceId, control.ControlId, control.Pressed ? 1u : 0u);
    (void)InvalidateControl(surfaceId, control.LocalBounds);
    return true;
}

bool FDesktopSurfaceContentHost::HandleSurfaceKeyPress(FDesktopSurfaceId surfaceId, Fortress::Core::uint32 keyAscii) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    FSurfaceControlState &state = SurfaceStates[index];
    if (state.Contract.InputFn != nullptr) {
        state.Contract.InputFn(surfaceId,
                               EDesktopSurfaceInputEvent::KeyPress,
                               keyAscii,
                               0,
                               0,
                               state.Contract.UserData);
    }

    if (state.FocusedControlIndex < 0 || static_cast<Fortress::Core::uint32>(state.FocusedControlIndex) >= state.ControlCount) {
        return false;
    }

    FDesktopControlNode &control = state.Controls[state.FocusedControlIndex];
    if (control.Type == EDesktopControlType::Button && keyAscii == 13u) {
        control.Pressed = !control.Pressed;
        LogControlEvent("CTRL KEY", surfaceId, control.ControlId, keyAscii);
        (void)InvalidateControl(surfaceId, control.LocalBounds);
        return true;
    }

    return false;
}

bool FDesktopSurfaceContentHost::InvalidateControl(FDesktopSurfaceId surfaceId,
                                                   const FDesktopRect &controlLocalBounds) const {
    if (Compositor == nullptr) {
        return false;
    }
    return Compositor->MarkSurfaceDamagedLocal(surfaceId, controlLocalBounds);
}

void FDesktopSurfaceContentHost::LogControlEvent(const char *prefix,
                                                 FDesktopSurfaceId surfaceId,
                                                 Fortress::Core::uint32 controlId,
                                                 Fortress::Core::uint32 arg0) const {
    if (LogSink == nullptr || prefix == nullptr) {
        return;
    }

    char line[96] = {};
    size_t pos = 0u;
    FKernelTextFormat::AppendString(line, sizeof(line), pos, prefix);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " S ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, surfaceId);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " C ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, controlId);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " A ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, arg0);
    LogSink(line);
}

void FDesktopSurfaceContentHost::RenderSurfaceContent(const Fortress::Video::FVideoSurfaceView &surface,
                                                      const FDesktopSurfaceSnapshot &snapshot,
                                                      bool surfaceFocused) const {
    if (!Ready || !surface.IsValid() || !snapshot.Visible) {
        return;
    }

    const Fortress::Core::int32 index = FindSurfaceIndex(snapshot.SurfaceId);
    if (index < 0) {
        return;
    }

    const FSurfaceControlState &state = SurfaceStates[index];
    if (!state.InUse) {
        return;
    }

    if (state.Contract.RenderFn != nullptr) {
        state.Contract.RenderFn(snapshot.SurfaceId, snapshot.Bounds, state.Contract.UserData);
    }

    for (Fortress::Core::uint32 i = 0u; i < state.ControlCount; i++) {
        const FDesktopControlNode &control = state.Controls[i];
        if (!control.Visible) {
            continue;
        }

        FDesktopRect globalRect{
            .X = snapshot.Bounds.X + control.LocalBounds.X,
            .Y = snapshot.Bounds.Y + control.LocalBounds.Y,
            .Width = control.LocalBounds.Width,
            .Height = control.LocalBounds.Height,
        };
        if (!IsRectValid(globalRect)) {
            continue;
        }

        if (control.Type == EDesktopControlType::Label) {
            DrawFilledRect(surface,
                           globalRect.X,
                           globalRect.Y,
                           globalRect.Width,
                           globalRect.Height,
                           Fortress::Video::FColor::RGB(40, 48, 62));
            DrawFilledRectAlpha(surface,
                                globalRect.X + 1,
                                globalRect.Y + 1,
                                globalRect.Width - 2,
                                globalRect.Height / 3,
                                Fortress::Video::FColor{.R = 255u, .G = 255u, .B = 255u, .A = 18u});
            DrawRectFrame(surface, globalRect, Fortress::Video::FColor::RGB(95, 130, 180));
        } else {
            const Fortress::Video::FColor fillColor = control.Pressed
                                                          ? Fortress::Video::FColor::RGB(58, 82, 104)
                                                          : Fortress::Video::FColor::RGB(70, 92, 116);
            DrawFilledRect(surface,
                           globalRect.X,
                           globalRect.Y,
                           globalRect.Width,
                           globalRect.Height,
                           fillColor);

            const Fortress::Video::FColor overlayColor = control.Pressed
                                                              ? Fortress::Video::FColor{.R = 0u, .G = 0u, .B = 0u, .A = 34u}
                                                              : Fortress::Video::FColor{.R = 255u, .G = 255u, .B = 255u, .A = 24u};
            DrawFilledRectAlpha(surface,
                                globalRect.X + 1,
                                globalRect.Y + 1,
                                globalRect.Width - 2,
                                globalRect.Height / 2,
                                overlayColor);

            if (control.Focused && surfaceFocused) {
                DrawFilledRectAlpha(surface,
                                    globalRect.X - 2,
                                    globalRect.Y - 2,
                                    globalRect.Width + 4,
                                    globalRect.Height + 4,
                                    Fortress::Video::FColor{.R = 78u, .G = 160u, .B = 255u, .A = 56u});
            }

            const Fortress::Video::FColor frameColor = (control.Focused && surfaceFocused)
                                                           ? Fortress::Video::FColor::RGB(172, 218, 255)
                                                           : Fortress::Video::FColor::RGB(120, 156, 194);
            DrawRectFrame(surface, globalRect, frameColor);

            if (globalRect.Height > 4) {
                DrawFilledRectAlpha(surface,
                                    globalRect.X + 1,
                                    globalRect.Y + globalRect.Height - 2,
                                    globalRect.Width - 2,
                                    1,
                                    Fortress::Video::FColor{.R = 0u, .G = 0u, .B = 0u, .A = 54u});
            }

            if (control.ControlId == GDesktopTerminalLauncherButtonId && globalRect.Width >= 44 && globalRect.Height >= 14) {
                // Draw a small CMD-style glyph to make launcher intent obvious.
                const FDesktopRect glyphRect{
                    .X = globalRect.X + 10,
                    .Y = globalRect.Y + (globalRect.Height / 2) - 4,
                    .Width = 24,
                    .Height = 9,
                };
                DrawRectFrame(surface, glyphRect, Fortress::Video::FColor::RGB(224, 244, 255));
                DrawFilledRect(surface,
                               glyphRect.X + 2,
                               glyphRect.Y + 2,
                               6,
                               1,
                               Fortress::Video::FColor::RGB(214, 240, 255));
                DrawFilledRect(surface,
                               glyphRect.X + 2,
                               glyphRect.Y + 5,
                               10,
                               1,
                               Fortress::Video::FColor::RGB(214, 240, 255));
            }
        }
    }
}

bool FDesktopSurfaceContentHost::GetSurfaceControlSummary(FDesktopSurfaceId surfaceId,
                                                          Fortress::Core::uint32 &outControlCount,
                                                          Fortress::Core::uint32 &outFocusedControlId) const {
    outControlCount = 0u;
    outFocusedControlId = 0u;

    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    const FSurfaceControlState &state = SurfaceStates[index];
    outControlCount = state.ControlCount;
    if (state.FocusedControlIndex >= 0 &&
        static_cast<Fortress::Core::uint32>(state.FocusedControlIndex) < state.ControlCount) {
        outFocusedControlId = state.Controls[state.FocusedControlIndex].ControlId;
    }
    return true;
}

bool FDesktopSurfaceContentHost::GetSurfaceControlNode(FDesktopSurfaceId surfaceId,
                                                       Fortress::Core::uint32 controlIndex,
                                                       FDesktopControlNode &outNode) const {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    const FSurfaceControlState &state = SurfaceStates[index];
    if (controlIndex >= state.ControlCount) {
        return false;
    }

    outNode = state.Controls[controlIndex];
    return true;
}

} // namespace Fortress::Kernel