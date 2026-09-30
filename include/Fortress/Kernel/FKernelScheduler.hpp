#ifndef FORTRESS_KERNEL_FKERNELSCHEDULER_HPP
#define FORTRESS_KERNEL_FKERNELSCHEDULER_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

using FKernelTaskEntry = void (*)(void *context);

enum class EKernelTaskState : Fortress::Core::uint8 {
    Inactive = 0,
    Ready,
    Running,
    Blocked,
};

struct FKernelTaskHandle {
    Fortress::Core::uint32 Id = 0;
};

struct FKernelTaskCreateInfo {
    const char *Name = nullptr;
    FKernelTaskEntry Entry = nullptr;
    void *Context = nullptr;
    Fortress::Core::uint32 PreferredCoreId = 0;
    Fortress::Core::uint32 TimeSliceTicks = 1;
    bool StartReady = true;
};

struct FKernelSchedulerStats {
    Fortress::Core::uint64 TickCount = 0;
    Fortress::Core::uint32 OnlineCoreCount = 1;
    Fortress::Core::uint32 TotalTaskCount = 0;
    Fortress::Core::uint32 ReadyTaskCount = 0;
    Fortress::Core::uint32 RunningTaskCount = 0;
    Fortress::Core::uint32 BlockedTaskCount = 0;
    Fortress::Core::uint32 LastScheduledTaskId = 0;
    Fortress::Core::uint64 PreemptionCount = 0;
    Fortress::Core::uint64 StarvationTickCount = 0;
};

class FKernelScheduler {
  public:
    static bool Initialize(Fortress::Core::uint32 bootstrapCoreId, Fortress::Core::uint32 onlineCoreCount);

    static void OnTick();
    static bool CreateTask(const FKernelTaskCreateInfo &createInfo, FKernelTaskHandle &outHandle);
    static bool SetTaskState(FKernelTaskHandle handle, EKernelTaskState state);
    static void GetStats(FKernelSchedulerStats &outStats);
};

} // namespace Fortress::Kernel

#endif