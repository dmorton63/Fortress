#ifndef FORTRESS_KERNEL_FKERNELSUBSYSTEMHEALTHSNAPSHOT_HPP
#define FORTRESS_KERNEL_FKERNELSUBSYSTEMHEALTHSNAPSHOT_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Kernel/FKernelSubsystemStateTracker.hpp"

namespace Fortress::Kernel {

struct FKernelSchedulerHealthSnapshot {
    Fortress::Core::uint32 ReadyDepth = 0;
    Fortress::Core::uint64 PreemptionCount = 0;
    Fortress::Core::uint64 StarvationCount = 0;
    Fortress::Core::uint64 TickDriftMicros = 0;
};

struct FKernelEventManagerHealthSnapshot {
    Fortress::Core::uint32 QueueDepth = 0;
    Fortress::Core::uint64 FanoutLatencyMicros = 0;
    Fortress::Core::uint64 DroppedEvents = 0;
    Fortress::Core::uint64 HandlerFaultCount = 0;
};

struct FKernelServiceHealthSnapshot {
    Fortress::Core::uint32 ServiceCount = 0;
    Fortress::Core::uint64 FailedStarts = 0;
    Fortress::Core::uint64 RestartAttempts = 0;
    Fortress::Core::uint64 DependencyViolations = 0;
};

struct FKernelNetworkHealthSnapshot {
    Fortress::Core::uint32 InterfaceCount = 0;
    Fortress::Core::uint64 RxCount = 0;
    Fortress::Core::uint64 TxCount = 0;
    Fortress::Core::uint64 DropCount = 0;
    bool LinkUp = false;
};

struct FKernelSecurityHealthSnapshot {
    Fortress::Core::uint64 DeniedCapabilityChecks = 0;
    bool PolicyLoaded = true;
    Fortress::Core::uint32 AuditQueuePressure = 0;
};

struct FKernelDesktopWindowHealthSnapshot {
    bool DesktopReady = false;
    Fortress::Core::uint32 SurfaceCount = 0;
    Fortress::Core::uint32 DirtySurfaceCount = 0;
    Fortress::Core::uint32 HighestZOrder = 0;
};

struct FKernelSubsystemHealthSnapshot {
    EKernelSubsystemPhase Phase = EKernelSubsystemPhase::Boot;
    Fortress::Core::uint64 TickCount = 0;
    FKernelSchedulerHealthSnapshot Scheduler = {};
    FKernelEventManagerHealthSnapshot EventManager = {};
    FKernelServiceHealthSnapshot Services = {};
    FKernelNetworkHealthSnapshot Network = {};
    FKernelSecurityHealthSnapshot Security = {};
    FKernelDesktopWindowHealthSnapshot DesktopWindow = {};
};

} // namespace Fortress::Kernel

#endif
