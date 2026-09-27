#ifndef FORTRESS_KERNEL_FKERNELRUNTIMEDIAGNOSTICS_HPP
#define FORTRESS_KERNEL_FKERNELRUNTIMEDIAGNOSTICS_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Kernel/FDesktopRuntime.hpp"
#include "Fortress/Kernel/FEventManager.hpp"
#include "Fortress/Kernel/FKernelBootstrap.hpp"

namespace Fortress::Kernel {

class FKernelRuntimeDiagnostics {
  public:
    bool Initialize(FDesktopRuntime *desktopRuntime);
    void Tick(FKernelRuntimeContext &runtime);

  private:
    static void HandleDesktopInputEvent(const FKernelEvent &event, void *context);

    void TryLogSchedulerStats();
    void TryLogEventFlowStats();
    void TryLogTextPipelineStats(FKernelRuntimeContext &runtime);
    void TryLogTimerStats();
    void TryLogIrqStats();
    void TryLogCoreDispatchStats();

    FDesktopRuntime *DesktopRuntime = nullptr;

    Fortress::Core::uint64 NextSchedulerStatsTick = 1u;
    Fortress::Core::uint64 NextEventStatsTick = 600u;
    Fortress::Core::uint64 NextTextStatsTick = 600u;
    Fortress::Core::uint64 NextTimerStatsTick = 600u;
    Fortress::Core::uint64 NextIrqStatsTick = 600u;
    Fortress::Core::uint64 NextCoreDispatchStatsTick = 600u;

    static constexpr Fortress::Core::uint64 SchedulerStatsLogIntervalTicks = 900u;
    static constexpr Fortress::Core::uint64 EventStatsLogIntervalTicks = 600u;
    static constexpr Fortress::Core::uint64 TextStatsLogIntervalTicks = 1200u;
    static constexpr Fortress::Core::uint64 TimerStatsLogIntervalTicks = 1200u;
    static constexpr Fortress::Core::uint64 IrqStatsLogIntervalTicks = 1200u;
    static constexpr Fortress::Core::uint64 CoreDispatchStatsLogIntervalTicks = 1200u;

    FKernelEventSubscriptionHandle HeartbeatSubscriptionHandle = {};
    FKernelEventSubscriptionHandle CommandSubscriptionHandle = {};
    FKernelEventSubscriptionHandle InputSubscriptionHandle = {};
    FKernelEventSubscriptionHandle DesktopInputSubscriptionHandle = {};
};

} // namespace Fortress::Kernel

#endif
