#include "Fortress/Kernel/FKernelScheduler.hpp"

namespace Fortress::Kernel {

static constexpr Fortress::Core::uint32 GMaxTasks = 64;
static constexpr Fortress::Core::uint32 GMaxCores = 256;
static constexpr Fortress::Core::uint32 GInvalidSlot = GMaxTasks;

struct FTaskSlot {
    bool InUse = false;
    Fortress::Core::uint32 Id = 0;
    const char *Name = nullptr;
    FKernelTaskEntry Entry = nullptr;
    void *Context = nullptr;
    Fortress::Core::uint32 PreferredCoreId = 0;
    Fortress::Core::uint32 TimeSliceTicks = 1;
    EKernelTaskState State = EKernelTaskState::Inactive;
};

struct FCoreReadyQueue {
    Fortress::Core::uint32 SlotIndices[GMaxTasks] = {};
    Fortress::Core::uint32 Count = 0;
    Fortress::Core::uint32 Cursor = 0;
};

static bool GInitialized = false;
static Fortress::Core::uint32 GBootstrapCoreId = 0;
static Fortress::Core::uint32 GOnlineCoreCount = 1;
static Fortress::Core::uint32 GNextTaskId = 1;
static Fortress::Core::uint32 GLastScheduledSlot = 0;
static Fortress::Core::uint64 GTickCount = 0;
static Fortress::Core::uint32 GLastScheduledTaskId = 0;
static FTaskSlot GTaskSlots[GMaxTasks] = {};
static FCoreReadyQueue GReadyQueues[GMaxCores] = {};
static Fortress::Core::uint32 GRunningSlotByCore[GMaxCores] = {};

static Fortress::Core::uint32 NormalizeCoreId(Fortress::Core::uint32 coreId) {
    if (GOnlineCoreCount == 0) {
        return 0;
    }

    return coreId % GOnlineCoreCount;
}

static void ResetReadyQueues() {
    for (Fortress::Core::uint32 coreIndex = 0; coreIndex < GMaxCores; coreIndex++) {
        FCoreReadyQueue &queue = GReadyQueues[coreIndex];
        for (Fortress::Core::uint32 i = 0; i < GMaxTasks; i++) {
            queue.SlotIndices[i] = 0;
        }
        queue.Count = 0;
    }
}

static void EnqueueReadySlot(Fortress::Core::uint32 coreId, Fortress::Core::uint32 slotIndex) {
    if (coreId >= GMaxCores || slotIndex >= GMaxTasks) {
        return;
    }

    FCoreReadyQueue &queue = GReadyQueues[coreId];
    if (queue.Count >= GMaxTasks) {
        return;
    }

    for (Fortress::Core::uint32 i = 0; i < queue.Count; i++) {
        if (queue.SlotIndices[i] == slotIndex) {
            return;
        }
    }

    queue.SlotIndices[queue.Count++] = slotIndex;
}

static void RebuildReadyQueues() {
    ResetReadyQueues();

    for (Fortress::Core::uint32 slotIndex = 0; slotIndex < GMaxTasks; slotIndex++) {
        const FTaskSlot &slot = GTaskSlots[slotIndex];
        if (!slot.InUse || slot.State != EKernelTaskState::Ready) {
            continue;
        }

        const Fortress::Core::uint32 coreId = NormalizeCoreId(slot.PreferredCoreId);
        EnqueueReadySlot(coreId, slotIndex);
    }
}

static Fortress::Core::uint32 CountTasksInState(EKernelTaskState state) {
    Fortress::Core::uint32 count = 0;
    for (Fortress::Core::uint32 i = 0; i < GMaxTasks; i++) {
        if (GTaskSlots[i].InUse && GTaskSlots[i].State == state) {
            count++;
        }
    }
    return count;
}

static FTaskSlot *FindSlotByHandle(FKernelTaskHandle handle) {
    if (handle.Id == 0) {
        return nullptr;
    }

    for (Fortress::Core::uint32 i = 0; i < GMaxTasks; i++) {
        if (GTaskSlots[i].InUse && GTaskSlots[i].Id == handle.Id) {
            return &GTaskSlots[i];
        }
    }
    return nullptr;
}

bool FKernelScheduler::Initialize(Fortress::Core::uint32 bootstrapCoreId, Fortress::Core::uint32 onlineCoreCount) {
    GInitialized = true;
    GBootstrapCoreId = bootstrapCoreId;
    GOnlineCoreCount = (onlineCoreCount == 0) ? 1 : onlineCoreCount;
    GNextTaskId = 1;
    GLastScheduledSlot = 0;
    GTickCount = 0;
    GLastScheduledTaskId = 0;

    for (Fortress::Core::uint32 i = 0; i < GMaxTasks; i++) {
        GTaskSlots[i] = FTaskSlot{};
    }

    for (Fortress::Core::uint32 coreIndex = 0; coreIndex < GMaxCores; coreIndex++) {
        GRunningSlotByCore[coreIndex] = GInvalidSlot;
    }
    ResetReadyQueues();

    return true;
}

void FKernelScheduler::OnTick() {
    if (!GInitialized) {
        return;
    }

    GTickCount++;

    for (Fortress::Core::uint32 coreIndex = 0; coreIndex < GMaxCores; coreIndex++) {
        const Fortress::Core::uint32 runningSlot = GRunningSlotByCore[coreIndex];
        if (runningSlot >= GMaxTasks) {
            continue;
        }

        if (GTaskSlots[runningSlot].InUse && GTaskSlots[runningSlot].State == EKernelTaskState::Running) {
            GTaskSlots[runningSlot].State = EKernelTaskState::Ready;
        }
        GRunningSlotByCore[coreIndex] = GInvalidSlot;
    }

    RebuildReadyQueues();

    const Fortress::Core::uint32 tickCoreId = NormalizeCoreId(GBootstrapCoreId);
    if (tickCoreId >= GMaxCores) {
        GLastScheduledTaskId = 0;
        return;
    }

    FCoreReadyQueue &queue = GReadyQueues[tickCoreId];
    if (queue.Count == 0) {
        GLastScheduledTaskId = 0;
        return;
    }

    for (Fortress::Core::uint32 step = 0; step < queue.Count; step++) {
        const Fortress::Core::uint32 queueIndex = (queue.Cursor + step) % queue.Count;
        const Fortress::Core::uint32 candidate = queue.SlotIndices[queueIndex];
        if (candidate >= GMaxTasks) {
            continue;
        }

        const Fortress::Core::uint32 candidateCoreId = NormalizeCoreId(GTaskSlots[candidate].PreferredCoreId);
        if (!GTaskSlots[candidate].InUse || GTaskSlots[candidate].State != EKernelTaskState::Ready ||
            candidateCoreId != tickCoreId) {
            continue;
        }

        GTaskSlots[candidate].State = EKernelTaskState::Running;
        GLastScheduledSlot = candidate;
        GLastScheduledTaskId = GTaskSlots[candidate].Id;
        GRunningSlotByCore[tickCoreId] = candidate;
        queue.Cursor = (queueIndex + 1) % queue.Count;
        if (GTaskSlots[candidate].Entry != nullptr) {
            GTaskSlots[candidate].Entry(GTaskSlots[candidate].Context);
        }
        return;
    }

    GLastScheduledTaskId = 0;
}

bool FKernelScheduler::CreateTask(const FKernelTaskCreateInfo &createInfo, FKernelTaskHandle &outHandle) {
    outHandle = FKernelTaskHandle{};

    if (!GInitialized || createInfo.Entry == nullptr) {
        return false;
    }

    for (Fortress::Core::uint32 i = 0; i < GMaxTasks; i++) {
        if (GTaskSlots[i].InUse) {
            continue;
        }

        GTaskSlots[i] = FTaskSlot{
            .InUse = true,
            .Id = GNextTaskId++,
            .Name = createInfo.Name,
            .Entry = createInfo.Entry,
            .Context = createInfo.Context,
            .PreferredCoreId = NormalizeCoreId(createInfo.PreferredCoreId),
            .TimeSliceTicks = (createInfo.TimeSliceTicks == 0) ? 1 : createInfo.TimeSliceTicks,
            .State = createInfo.StartReady ? EKernelTaskState::Ready : EKernelTaskState::Blocked,
        };
        outHandle.Id = GTaskSlots[i].Id;
        return true;
    }

    return false;
}

bool FKernelScheduler::SetTaskState(FKernelTaskHandle handle, EKernelTaskState state) {
    if (!GInitialized) {
        return false;
    }

    FTaskSlot *slot = FindSlotByHandle(handle);
    if (slot == nullptr) {
        return false;
    }

    slot->State = state;
    return true;
}

void FKernelScheduler::GetStats(FKernelSchedulerStats &outStats) {
    outStats = FKernelSchedulerStats{
        .TickCount = GTickCount,
        .OnlineCoreCount = GOnlineCoreCount,
        .TotalTaskCount = CountTasksInState(EKernelTaskState::Ready) +
                          CountTasksInState(EKernelTaskState::Running) +
                          CountTasksInState(EKernelTaskState::Blocked),
        .ReadyTaskCount = CountTasksInState(EKernelTaskState::Ready),
        .RunningTaskCount = CountTasksInState(EKernelTaskState::Running),
        .BlockedTaskCount = CountTasksInState(EKernelTaskState::Blocked),
        .LastScheduledTaskId = GLastScheduledTaskId,
    };

    (void)GBootstrapCoreId;
}

} // namespace Fortress::Kernel