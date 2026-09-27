#ifndef FORTRESS_KERNEL_FKERNELCOMMANDCONTROLPLANE_HPP
#define FORTRESS_KERNEL_FKERNELCOMMANDCONTROLPLANE_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Kernel/FEventManager.hpp"

namespace Fortress::Kernel {

struct FKernelCommandControlStats {
    Fortress::Core::uint64 CommandEventsReceived = 0;
    Fortress::Core::uint32 LastCommandLength = 0;
    Fortress::Core::uint32 LastCommandHash = 0;
};

class FKernelCommandControlPlane {
  public:
    static void Initialize();
    static void HandleCommandEvent(const FKernelEvent &event, void *context);

    static bool IsWireframeEnabled();
    static bool IsScenePaused();
    static bool IsCursorOverlayEnabled();
    static bool IsDesktopSurfaceOverlayEnabled();

    // Returns true while a command pulse is active and consumes one frame of it.
    static bool ConsumeCommandPulseFrame();
    static void GetStats(FKernelCommandControlStats &outStats);
};

} // namespace Fortress::Kernel

#endif