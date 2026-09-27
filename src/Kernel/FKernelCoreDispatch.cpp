#include "Fortress/Kernel/FKernelCoreDispatch.hpp"

#include "Fortress/Kernel/FKernelSpinLock.hpp"

namespace Fortress::Kernel {

namespace {

#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
static inline void DispatchCaptureOut8(unsigned short port, unsigned char value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static FKernelSpinLock GDispatchCaptureLock = {};

static void DispatchCaptureChar(char c) {
    // Route low-level dispatch breadcrumbs to QEMU debugcon (0xE9)
    // so they can be captured separately from COM1 kernel console logs.
    DispatchCaptureOut8(0xE9u, static_cast<unsigned char>(c));
}

static void DispatchCaptureMark(char code, char tag) {
    GDispatchCaptureLock.Acquire();
    DispatchCaptureChar('C');
    DispatchCaptureChar('D');
    DispatchCaptureChar(code);
    DispatchCaptureChar(' ');
    DispatchCaptureChar(tag);
    DispatchCaptureChar('\r');
    DispatchCaptureChar('\n');
    GDispatchCaptureLock.Release();
}
#endif

static constexpr Fortress::Core::uint32 GQueueCapacityPerCore = 64u;

struct FDispatchItem {
    FKernelCoreDispatchFn Fn = nullptr;
    void *Context = nullptr;
};

struct FCoreQueue {
    FDispatchItem Items[GQueueCapacityPerCore] = {};
    Fortress::Core::uint32 Head = 0;
    Fortress::Core::uint32 Tail = 0;
    Fortress::Core::uint32 Count = 0;
    FKernelSpinLock Lock = {};
};

static bool GInitialized = false;
static Fortress::Core::uint32 GOnlineCoreCount = 1u;
static Fortress::Core::uint32 GBootstrapCoreId = 0u;
static Fortress::Core::uint32 GNextDispatchCore = 0u;

static FCoreQueue GQueues[FKernelCoreDispatchMaxTrackedCores] = {};
static FKernelCoreDispatchStats GStats = {};
static Fortress::Core::uint64 GExecutedByCore[FKernelCoreDispatchMaxTrackedCores] = {};
static FKernelSpinLock GStatsLock = {};
static FKernelSpinLock GRoundRobinLock = {};

static Fortress::Core::uint32 NormalizeCoreId(Fortress::Core::uint32 coreId) {
    const Fortress::Core::uint32 count = (GOnlineCoreCount == 0u) ? 1u : GOnlineCoreCount;
    return coreId % count;
}

static void IncrementDroppedStat() {
    GStatsLock.Acquire();
    GStats.DroppedCount++;
    GStatsLock.Release();
}

static bool Enqueue(Fortress::Core::uint32 coreId, FKernelCoreDispatchFn fn, void *context) {
    if (!GInitialized || fn == nullptr || coreId >= FKernelCoreDispatchMaxTrackedCores) {
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
        DispatchCaptureMark('1', 'X');
#endif
        return false;
    }

    FCoreQueue &queue = GQueues[coreId];
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
    DispatchCaptureMark('1', 'A');
#endif
    queue.Lock.Acquire();
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
    DispatchCaptureMark('1', 'B');
#endif
    if (queue.Count >= GQueueCapacityPerCore) {
        queue.Lock.Release();
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
        DispatchCaptureMark('1', 'F');
#endif
        IncrementDroppedStat();
        return false;
    }

    queue.Items[queue.Tail] = FDispatchItem{.Fn = fn, .Context = context};
    queue.Tail = (queue.Tail + 1u) % GQueueCapacityPerCore;
    queue.Count++;
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
    DispatchCaptureMark('1', 'C');
#endif
    queue.Lock.Release();
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
    DispatchCaptureMark('1', 'D');
#endif

    GStatsLock.Acquire();
    GStats.PendingWorkItems++;
    GStats.EnqueuedCount++;
    GStatsLock.Release();
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
    DispatchCaptureMark('1', 'E');
#endif
    return true;
}

} // namespace

bool FKernelCoreDispatch::Initialize(Fortress::Core::uint32 onlineCoreCount, Fortress::Core::uint32 bootstrapCoreId) {
    GInitialized = true;
    GOnlineCoreCount = (onlineCoreCount == 0u) ? 1u : onlineCoreCount;
    if (GOnlineCoreCount > FKernelCoreDispatchMaxTrackedCores) {
        GOnlineCoreCount = FKernelCoreDispatchMaxTrackedCores;
    }
    GBootstrapCoreId = NormalizeCoreId(bootstrapCoreId);
    GNextDispatchCore = GBootstrapCoreId;

    for (Fortress::Core::uint32 i = 0; i < FKernelCoreDispatchMaxTrackedCores; i++) {
        GQueues[i] = FCoreQueue{};
        GExecutedByCore[i] = 0ull;
    }

    GStats = FKernelCoreDispatchStats{
        .OnlineCoreCount = GOnlineCoreCount,
        .BootstrapCoreId = GBootstrapCoreId,
        .QueueCapacityPerCore = GQueueCapacityPerCore,
        .PendingWorkItems = 0,
        .EnqueuedCount = 0,
        .ExecutedCount = 0,
        .DroppedCount = 0,
    };

    return true;
}

bool FKernelCoreDispatch::DispatchToCore(Fortress::Core::uint32 coreId, FKernelCoreDispatchFn fn, void *context) {
    if (!GInitialized) {
        return false;
    }

    const Fortress::Core::uint32 normalizedCore = NormalizeCoreId(coreId);
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
    DispatchCaptureMark('1', 'T');
#endif
    return Enqueue(normalizedCore, fn, context);
}

bool FKernelCoreDispatch::DispatchRoundRobin(FKernelCoreDispatchFn fn, void *context) {
    if (!GInitialized || fn == nullptr) {
        return false;
    }

    GRoundRobinLock.Acquire();
    const Fortress::Core::uint32 targetCore = GNextDispatchCore;
    GNextDispatchCore = NormalizeCoreId(GNextDispatchCore + 1u);
    GRoundRobinLock.Release();
    return Enqueue(targetCore, fn, context);
}

Fortress::Core::uint32 FKernelCoreDispatch::DrainForCore(Fortress::Core::uint32 coreId,
                                                          Fortress::Core::uint32 maxItems) {
    if (!GInitialized || maxItems == 0u) {
        return 0u;
    }

    const Fortress::Core::uint32 normalizedCore = NormalizeCoreId(coreId);
    FCoreQueue &queue = GQueues[normalizedCore];
    Fortress::Core::uint32 drained = 0u;

    while (drained < maxItems) {
        queue.Lock.Acquire();
        if (queue.Count == 0u) {
            queue.Lock.Release();
            break;
        }

#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
        if (normalizedCore != GBootstrapCoreId) {
            DispatchCaptureMark('2', 'A');
        }
#endif

#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
    if (normalizedCore != GBootstrapCoreId) {
        DispatchCaptureMark('2', 'G');
    }
#endif
        if (queue.Head >= GQueueCapacityPerCore || queue.Tail >= GQueueCapacityPerCore ||
            queue.Count > GQueueCapacityPerCore) {
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
            if (normalizedCore != GBootstrapCoreId) {
                DispatchCaptureMark('2', 'Z');
            }
#endif
            queue.Head = 0u;
            queue.Tail = 0u;
            queue.Count = 0u;
            queue.Lock.Release();
            IncrementDroppedStat();
            break;
        }

        const FKernelCoreDispatchFn itemFn = queue.Items[queue.Head].Fn;
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
    if (normalizedCore != GBootstrapCoreId) {
        DispatchCaptureMark('2', 'H');
    }
#endif
        void *itemContext = queue.Items[queue.Head].Context;
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
    if (normalizedCore != GBootstrapCoreId) {
        DispatchCaptureMark('2', 'I');
    }
#endif
        queue.Items[queue.Head].Fn = nullptr;
        queue.Items[queue.Head].Context = nullptr;
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
    if (normalizedCore != GBootstrapCoreId) {
        DispatchCaptureMark('2', 'K');
    }
#endif
        queue.Head = (queue.Head + 1u) % GQueueCapacityPerCore;
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
    if (normalizedCore != GBootstrapCoreId) {
        DispatchCaptureMark('2', 'J');
    }
#endif
        queue.Count--;
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
        if (normalizedCore != GBootstrapCoreId) {
            DispatchCaptureMark('2', 'B');
        }
#endif
        queue.Lock.Release();
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
        if (normalizedCore != GBootstrapCoreId) {
            DispatchCaptureMark('2', 'C');
        }
#endif

        GStatsLock.Acquire();
        if (GStats.PendingWorkItems > 0u) {
            GStats.PendingWorkItems--;
        }
        GStatsLock.Release();

        if (itemFn != nullptr) {
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
            if (normalizedCore != GBootstrapCoreId) {
                DispatchCaptureMark('2', 'D');
            }
#endif
            itemFn(itemContext);
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
            if (normalizedCore != GBootstrapCoreId) {
                DispatchCaptureMark('2', 'E');
            }
#endif
            GStatsLock.Acquire();
            GStats.ExecutedCount++;
            GExecutedByCore[normalizedCore]++;
            GStatsLock.Release();
#if defined(FORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1)
            if (normalizedCore != GBootstrapCoreId) {
                DispatchCaptureMark('2', 'F');
            }
#endif
        } else {
            IncrementDroppedStat();
        }

        drained++;
    }

    return drained;
}

void FKernelCoreDispatch::GetStats(FKernelCoreDispatchStats &outStats) {
    GStatsLock.Acquire();
    outStats = GStats;
    GStatsLock.Release();
}

Fortress::Core::uint64 FKernelCoreDispatch::GetExecutedCountForCore(Fortress::Core::uint32 coreId) {
    if (!GInitialized) {
        return 0ull;
    }

    const Fortress::Core::uint32 normalizedCore = NormalizeCoreId(coreId);
    GStatsLock.Acquire();
    const Fortress::Core::uint64 count = GExecutedByCore[normalizedCore];
    GStatsLock.Release();
    return count;
}

} // namespace Fortress::Kernel
