#ifndef FORTRESS_KERNEL_FKERNELSCHEDULEREVENTPLANE_HPP
#define FORTRESS_KERNEL_FKERNELSCHEDULEREVENTPLANE_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Kernel/FEventManager.hpp"

namespace Fortress::Kernel {

struct FKernelSchedulerEventStats {
    Fortress::Core::uint64 HeartbeatEventsReceived = 0;
    Fortress::Core::uint32 LastHeartbeatRunCount = 0;
};

class FKernelSchedulerEventPlane {
  public:
    static void Initialize();
    static void HandleSchedulerEvent(const FKernelEvent &event, void *context);
    static void GetStats(FKernelSchedulerEventStats &outStats);
};

} // namespace Fortress::Kernel

#endif