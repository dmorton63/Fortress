#include "Fortress/Kernel/FKernelApWorker.hpp"

#include "Fortress/Kernel/FCpuCoreManager.hpp"
#include "Fortress/Kernel/FKernelCoreDispatch.hpp"
#include "Fortress/Kernel/FKernelSpinLock.hpp"

namespace Fortress::Kernel {

namespace {

static FKernelSpinLock GStatsLock = {};
static bool GDrainEnabled = false;
#if defined(FORTRESS_EXPERIMENTAL_AP_DRAIN_ENABLE)
static constexpr bool GAllowDrainRuntimeEnable = true;
#else
static constexpr bool GAllowDrainRuntimeEnable = false;
#endif
#if defined(FORTRESS_EXPERIMENTAL_AP_DISPATCH_CALLBACKS)
static constexpr bool GAllowDispatchCallbacks = true;
#else
static constexpr bool GAllowDispatchCallbacks = false;
#endif
#if defined(FORTRESS_EXPERIMENTAL_AP_DISPATCH_CALLBACKS_ARMED)
static constexpr bool GDispatchCallbacksArmed = true;
#else
static constexpr bool GDispatchCallbacksArmed = false;
#endif
static FKernelApWorkerStats GStats = {
    .DrainBudgetPerLoop = 16u,
    .StartedWorkers = 0u,
    .ActiveWorkers = 0u,
    .LastStartedCoreId = 0u,
    .ProbeTicksTotal = 0u,
    .LastProbeCoreId = 0u,
};

static inline void CpuPause() {
    __asm__ volatile("pause");
}

} // namespace

bool FKernelApWorker::InstallEntrypoint() {
    return FCpuCoreManager::InstallParallelWorkerEntrypoint(&FKernelApWorker::Entrypoint, 0u);
}

bool FKernelApWorker::Enable(bool enabled) {
    if (!enabled) {
        __atomic_store_n(&GDrainEnabled, false, __ATOMIC_RELEASE);
    }
    return FCpuCoreManager::SetParallelWorkersEnabled(enabled);
}

bool FKernelApWorker::IsEnabled() {
    return FCpuCoreManager::AreParallelWorkersEnabled();
}

bool FKernelApWorker::AreDispatchCallbacksEnabled() {
    return GAllowDispatchCallbacks && GDispatchCallbacksArmed;
}

bool FKernelApWorker::SetDrainEnabled(bool enabled) {
    if (enabled && !GAllowDrainRuntimeEnable) {
        return false;
    }

    if (enabled) {
        FKernelCoreDispatchStats dispatchStats{};
        FKernelCoreDispatch::GetStats(dispatchStats);
        if (dispatchStats.PendingWorkItems != 0u) {
            return false;
        }
    }

    __atomic_store_n(&GDrainEnabled, enabled, __ATOMIC_RELEASE);
    return true;
}

bool FKernelApWorker::IsDrainEnabled() {
    return __atomic_load_n(&GDrainEnabled, __ATOMIC_ACQUIRE);
}

void FKernelApWorker::SetDrainBudgetPerLoop(Fortress::Core::uint32 budget) {
    if (budget == 0u) {
        return;
    }

    GStatsLock.Acquire();
    GStats.DrainBudgetPerLoop = budget;
    GStatsLock.Release();
}

void FKernelApWorker::GetStats(FKernelApWorkerStats &outStats) {
    GStatsLock.Acquire();
    outStats = GStats;
    GStatsLock.Release();
}

void FKernelApWorker::Entrypoint(limine_smp_info *cpuInfo) {
    // Keep AP worker execution deterministic until full per-core IRQ routing/IDT policy is in place.
    __asm__ volatile("cli");

    Fortress::Core::uint32 coreId = 0u;
    const Fortress::Core::uint32 lapicId = (cpuInfo == nullptr) ? 0u : cpuInfo->lapic_id;
    const bool haveCoreId = FCpuCoreManager::TryGetCoreIdByLocalApicId(lapicId, coreId);

    if (!haveCoreId) {
        for (;;) {
            CpuPause();
        }
    }

    GStatsLock.Acquire();
    GStats.StartedWorkers++;
    GStats.ActiveWorkers++;
    GStats.LastStartedCoreId = coreId;
    GStatsLock.Release();

    Fortress::Core::uint32 probeTickSampleCounter = 0u;

    for (;;) {
        if (!FCpuCoreManager::AreParallelWorkersEnabled()) {
            CpuPause();
            continue;
        }

        if (!FKernelApWorker::IsDrainEnabled()) {
            CpuPause();
            continue;
        }

        if (!FKernelApWorker::AreDispatchCallbacksEnabled()) {
            probeTickSampleCounter++;
            if (probeTickSampleCounter >= 4096u) {
                GStatsLock.Acquire();
                GStats.ProbeTicksTotal++;
                GStats.LastProbeCoreId = coreId;
                GStatsLock.Release();
                probeTickSampleCounter = 0u;
            }

            CpuPause();
            continue;
        }

        Fortress::Core::uint32 budget = 0u;
        GStatsLock.Acquire();
        budget = GStats.DrainBudgetPerLoop;
        GStatsLock.Release();

        const Fortress::Core::uint32 drained = FKernelCoreDispatch::DrainForCore(coreId, budget);
        if (drained == 0u) {
            CpuPause();
        }
    }
}

} // namespace Fortress::Kernel
