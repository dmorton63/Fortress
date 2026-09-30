#ifndef FORTRESS_KERNEL_FKERNELCOMMANDCONSOLE_HPP
#define FORTRESS_KERNEL_FKERNELCOMMANDCONSOLE_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Video {
class FVideoConsole;
}

namespace Fortress::Kernel {
class FDesktopCompositor;
class FDesktopInputRouter;
}

namespace Fortress::Kernel {

class FKernelCommandConsole {
  public:
    using FLongOperationYieldCallback = void (*)();

    enum class EHudLogViewMode : Fortress::Core::uint8 {
        Hidden = 0,
        ShowLog,
        BootLog,
    };

    enum class EHudLogDetailMode : Fortress::Core::uint8 {
      Tail = 0,
      Full,
      Errors,
      Warn,
      AllIssues,
    };

    static void Initialize();
    static void PollInput();
    static void SetLongOperationYieldCallback(FLongOperationYieldCallback callback);
    static void BindVideoConsole(Fortress::Video::FVideoConsole *console);
    static void BindDesktopCompositor(Fortress::Kernel::FDesktopCompositor *compositor);
    static void BindDesktopInputRouter(Fortress::Kernel::FDesktopInputRouter *router);
    static void PushSystemLog(const char *line);

    static bool IsWireframeEnabled();
    static bool IsPaused();

    static const char *GetCommandBuffer();
    static EHudLogViewMode GetHudLogViewMode();
    static EHudLogDetailMode GetHudLogDetailMode();
    static bool IsTerminalModeEnabled();
    static bool IsTerminalWindowEnabled();
    static bool TryGetTerminalWindowBounds(Fortress::Core::int32 &outX,
                         Fortress::Core::int32 &outY,
                         Fortress::Core::int32 &outWidth,
                         Fortress::Core::int32 &outHeight);
    static bool IsHudParallelStatsEnabled();
    static Fortress::Core::usize GetLogCount();
    static const char *GetLogLine(Fortress::Core::usize index);
    static Fortress::Core::usize GetBootLogCount();
    static const char *GetBootLogLine(Fortress::Core::usize index);
    static bool GetHidCursorNormalized(Fortress::Core::int32 &outX, Fortress::Core::int32 &outY);
    static bool IsCursorOverlayEnabled();
    static bool IsCursorInvertX();
    static bool IsCursorInvertY();
    static Fortress::Core::uint32 GetCursorSensitivityPercent();
    static void GetHidButtonsDown(bool &outLeft, bool &outRight, bool &outMiddle);
    static bool ConsumeHidButtonPressEdges(bool &outLeft, bool &outRight, bool &outMiddle);
};

} // namespace Fortress::Kernel

#endif
