#ifndef FORTRESS_KERNEL_FDESKTOPSURFACECONTENTHOST_HPP
#define FORTRESS_KERNEL_FDESKTOPSURFACECONTENTHOST_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Kernel/FDesktopCompositor.hpp"
#include "Fortress/Video/FVideoSurface.hpp"

namespace Fortress::Kernel {

enum class EDesktopControlType : Fortress::Core::uint8 {
    Label = 0,
    Button,
};

enum class EDesktopSurfaceInputEvent : Fortress::Core::uint32 {
    Focus = 1u,
    PointerPress = 2u,
    KeyPress = 3u,
};

struct FDesktopControlNode {
    Fortress::Core::uint32 ControlId = 0u;
    EDesktopControlType Type = EDesktopControlType::Label;
    FDesktopRect LocalBounds = {};
    bool Visible = true;
    bool Enabled = true;
    bool Focusable = false;
    bool Focused = false;
    bool Pressed = false;
};

struct FDesktopSurfaceContentContract {
    void (*RenderFn)(FDesktopSurfaceId surfaceId, const FDesktopRect &surfaceBounds, void *userData) = nullptr;
    void (*InputFn)(FDesktopSurfaceId surfaceId,
                    EDesktopSurfaceInputEvent event,
                    Fortress::Core::uint32 arg0,
                    Fortress::Core::int32 x,
                    Fortress::Core::int32 y,
                    void *userData) = nullptr;
    void *UserData = nullptr;
};

class FDesktopSurfaceContentHost {
  public:
    using FLogSinkFn = void (*)(const char *line);

    bool Initialize(FDesktopCompositor *compositor);
    bool IsReady() const;
    void SetLogSink(FLogSinkFn logSink);

    bool RegisterSurfaceContent(FDesktopSurfaceId surfaceId, const FDesktopSurfaceContentContract &contract);
    bool UnregisterSurfaceContent(FDesktopSurfaceId surfaceId);

    bool AddLabelControl(FDesktopSurfaceId surfaceId, Fortress::Core::uint32 controlId, const FDesktopRect &localBounds);
    bool AddButtonControl(FDesktopSurfaceId surfaceId, Fortress::Core::uint32 controlId, const FDesktopRect &localBounds);

    bool FocusNextControl(FDesktopSurfaceId surfaceId);
    bool FocusPreviousControl(FDesktopSurfaceId surfaceId);
    bool HandleSurfaceFocus(FDesktopSurfaceId surfaceId);
    bool HandleSurfacePointerPress(FDesktopSurfaceId surfaceId, Fortress::Core::int32 x, Fortress::Core::int32 y);
    bool HandleSurfaceFocusedControlPress(FDesktopSurfaceId surfaceId, Fortress::Core::int32 x, Fortress::Core::int32 y);
    bool HandleSurfaceKeyPress(FDesktopSurfaceId surfaceId, Fortress::Core::uint32 keyAscii);

    void RenderSurfaceContent(const Fortress::Video::FVideoSurfaceView &surface,
                              const FDesktopSurfaceSnapshot &snapshot,
                              bool surfaceFocused) const;

    bool GetSurfaceControlSummary(FDesktopSurfaceId surfaceId,
                                  Fortress::Core::uint32 &outControlCount,
                                  Fortress::Core::uint32 &outFocusedControlId) const;
    bool GetSurfaceControlNode(FDesktopSurfaceId surfaceId,
                               Fortress::Core::uint32 controlIndex,
                               FDesktopControlNode &outNode) const;

  private:
    static constexpr Fortress::Core::uint32 MaxSurfaceHosts = 32u;
    static constexpr Fortress::Core::uint32 MaxControlsPerSurface = 8u;

    struct FSurfaceControlState {
        bool InUse = false;
        FDesktopSurfaceId SurfaceId = DesktopInvalidSurfaceId;
        FDesktopSurfaceContentContract Contract = {};
        FDesktopControlNode Controls[MaxControlsPerSurface] = {};
        Fortress::Core::uint32 ControlCount = 0u;
        Fortress::Core::int32 FocusedControlIndex = -1;
    };

    Fortress::Core::int32 FindSurfaceIndex(FDesktopSurfaceId surfaceId) const;
    Fortress::Core::int32 FindOrAllocateSurfaceIndex(FDesktopSurfaceId surfaceId);
    bool AddControl(FDesktopSurfaceId surfaceId,
                    Fortress::Core::uint32 controlId,
                    EDesktopControlType type,
                    const FDesktopRect &localBounds,
                    bool focusable);
    bool SetFocusedControlIndex(FSurfaceControlState &state, Fortress::Core::int32 controlIndex);
    Fortress::Core::int32 FindControlAtPoint(const FSurfaceControlState &state,
                                             const FDesktopRect &surfaceBounds,
                                             Fortress::Core::int32 x,
                                             Fortress::Core::int32 y) const;
    bool InvalidateControl(FDesktopSurfaceId surfaceId, const FDesktopRect &controlLocalBounds) const;
    void LogControlEvent(const char *prefix,
                         FDesktopSurfaceId surfaceId,
                         Fortress::Core::uint32 controlId,
                         Fortress::Core::uint32 arg0) const;

    FDesktopCompositor *Compositor = nullptr;
    FLogSinkFn LogSink = nullptr;
    FSurfaceControlState SurfaceStates[MaxSurfaceHosts] = {};
    bool Ready = false;
};

} // namespace Fortress::Kernel

#endif