#ifndef FORTRESS_KERNEL_FKERNELAPWORKER_HPP
#define FORTRESS_KERNEL_FKERNELAPWORKER_HPP

#include "Fortress/Core/FTypes.hpp"
#include "limine.h"

namespace Fortress::Kernel {

struct FKernelApWorkerStats {
    Fortress::Core::uint32 DrainBudgetPerLoop = 0;
    Fortress::Core::uint32 StartedWorkers = 0;
    Fortress::Core::uint32 ActiveWorkers = 0;
    Fortress::Core::uint32 LastStartedCoreId = 0;
  Fortress::Core::uint64 ProbeTicksTotal = 0;
  Fortress::Core::uint32 LastProbeCoreId = 0;
};

class FKernelApWorker {
  public:
    static bool InstallEntrypoint();
    static bool Enable(bool enabled);
    static bool IsEnabled();
    static bool AreDispatchCallbacksEnabled();
    static bool SetDrainEnabled(bool enabled);
    static bool IsDrainEnabled();

    static void SetDrainBudgetPerLoop(Fortress::Core::uint32 budget);
    static void GetStats(FKernelApWorkerStats &outStats);

    static void Entrypoint(limine_smp_info *cpuInfo);
};

} // namespace Fortress::Kernel

#endif
