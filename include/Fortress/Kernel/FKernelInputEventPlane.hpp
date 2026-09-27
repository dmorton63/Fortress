#ifndef FORTRESS_KERNEL_FKERNELINPUTEVENTPLANE_HPP
#define FORTRESS_KERNEL_FKERNELINPUTEVENTPLANE_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Kernel/FEventManager.hpp"

namespace Fortress::Kernel {

struct FKernelInputEventStats {
    Fortress::Core::uint64 InputEventsReceived = 0;
    Fortress::Core::uint32 LastKeyAscii = 0;
};

class FKernelInputEventPlane {
  public:
    static void Initialize();
    static void HandleInputEvent(const FKernelEvent &event, void *context);
    static bool ConsumeInputPulseFrame();
    static void GetStats(FKernelInputEventStats &outStats);
};

} // namespace Fortress::Kernel

#endif