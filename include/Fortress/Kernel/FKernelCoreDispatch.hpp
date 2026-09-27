#ifndef FORTRESS_KERNEL_FKERNELCOREDISPATCH_HPP
#define FORTRESS_KERNEL_FKERNELCOREDISPATCH_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

static constexpr Fortress::Core::uint32 FKernelCoreDispatchMaxTrackedCores = 256u;

using FKernelCoreDispatchFn = void (*)(void *context);

struct FKernelCoreDispatchStats {
    Fortress::Core::uint32 OnlineCoreCount = 1;
    Fortress::Core::uint32 BootstrapCoreId = 0;
    Fortress::Core::uint32 QueueCapacityPerCore = 0;
    Fortress::Core::uint32 PendingWorkItems = 0;
    Fortress::Core::uint64 EnqueuedCount = 0;
    Fortress::Core::uint64 ExecutedCount = 0;
    Fortress::Core::uint64 DroppedCount = 0;
};

class FKernelCoreDispatch {
  public:
    static bool Initialize(Fortress::Core::uint32 onlineCoreCount, Fortress::Core::uint32 bootstrapCoreId);

    static bool DispatchToCore(Fortress::Core::uint32 coreId, FKernelCoreDispatchFn fn, void *context);
    static bool DispatchRoundRobin(FKernelCoreDispatchFn fn, void *context);

    // Drains queued work for a core and executes at most maxItems callbacks.
    static Fortress::Core::uint32 DrainForCore(Fortress::Core::uint32 coreId, Fortress::Core::uint32 maxItems);

    static void GetStats(FKernelCoreDispatchStats &outStats);
    static Fortress::Core::uint64 GetExecutedCountForCore(Fortress::Core::uint32 coreId);
};

} // namespace Fortress::Kernel

#endif
