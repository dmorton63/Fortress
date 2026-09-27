#include "Fortress/Kernel/FCpuCoreManager.hpp"

namespace Fortress::Kernel {

static constexpr Fortress::Core::uint32 GMaxTrackedCores = 256;
static FCpuCoreInfo GCoreInfos[GMaxTrackedCores] = {};
static FCpuCoreManagerStats GStats = {};
static const limine_smp_response *GSmpResponse = nullptr;

bool FCpuCoreManager::Initialize(const limine_smp_response *mpResponse) {
    for (Fortress::Core::uint32 i = 0; i < GMaxTrackedCores; i++) {
        GCoreInfos[i] = FCpuCoreInfo{};
    }

    GStats = FCpuCoreManagerStats{};
    GSmpResponse = mpResponse;

    if (mpResponse == nullptr || mpResponse->cpu_count == 0 || mpResponse->cpus == nullptr) {
        GStats.DetectedCoreCount = 1;
        GStats.OnlineCoreCount = 1;
        GStats.BootstrapCoreId = 0;
        GStats.MpAvailable = false;
        GStats.ParallelWorkersSupported = false;
        GCoreInfos[0] = FCpuCoreInfo{
            .CoreId = 0,
            .LocalApicId = 0,
            .IsBootstrapProcessor = true,
            .IsOnline = true,
        };
        return true;
    }

    const Fortress::Core::uint32 detected =
        static_cast<Fortress::Core::uint32>(mpResponse->cpu_count > GMaxTrackedCores ? GMaxTrackedCores
                                                                                        : mpResponse->cpu_count);

    GStats.DetectedCoreCount = detected;
    GStats.OnlineCoreCount = detected;
    GStats.BootstrapCoreId = 0;
    GStats.MpAvailable = true;
    GStats.ParallelWorkersSupported = (detected > 1u);

    for (Fortress::Core::uint32 i = 0; i < detected; i++) {
        limine_smp_info *info = mpResponse->cpus[i];
        if (info == nullptr) {
            continue;
        }

        const bool isBsp = (info->lapic_id == mpResponse->bsp_lapic_id);
        GCoreInfos[i] = FCpuCoreInfo{
            .CoreId = i,
            .LocalApicId = info->lapic_id,
            .IsBootstrapProcessor = isBsp,
            .IsOnline = true,
        };

        if (isBsp) {
            GStats.BootstrapCoreId = i;
        }
    }

    return true;
}

Fortress::Core::uint32 FCpuCoreManager::GetOnlineCoreCount() {
    return GStats.OnlineCoreCount;
}

Fortress::Core::uint32 FCpuCoreManager::GetBootstrapCoreId() {
    return GStats.BootstrapCoreId;
}

bool FCpuCoreManager::GetCoreInfo(Fortress::Core::uint32 index, FCpuCoreInfo &outInfo) {
    if (index >= GStats.DetectedCoreCount || index >= GMaxTrackedCores) {
        return false;
    }

    outInfo = GCoreInfos[index];
    return true;
}

bool FCpuCoreManager::TryGetCoreIdByLocalApicId(Fortress::Core::uint32 localApicId, Fortress::Core::uint32 &outCoreId) {
    for (Fortress::Core::uint32 i = 0; i < GStats.DetectedCoreCount && i < GMaxTrackedCores; i++) {
        if (!GCoreInfos[i].IsOnline) {
            continue;
        }

        if (GCoreInfos[i].LocalApicId == localApicId) {
            outCoreId = GCoreInfos[i].CoreId;
            return true;
        }
    }

    return false;
}

bool FCpuCoreManager::InstallParallelWorkerEntrypoint(limine_goto_address entrypoint,
                                                      Fortress::Core::uint64 extraArgument) {
    if (!GStats.ParallelWorkersSupported || GSmpResponse == nullptr || GSmpResponse->cpus == nullptr || entrypoint == nullptr) {
        return false;
    }

    Fortress::Core::uint32 plannedWorkers = 0;
    for (Fortress::Core::uint32 i = 0; i < GStats.DetectedCoreCount; i++) {
        limine_smp_info *info = GSmpResponse->cpus[i];
        if (info == nullptr) {
            continue;
        }

        const bool isBsp = (i == GStats.BootstrapCoreId);
        if (isBsp) {
            continue;
        }

        info->goto_address = entrypoint;
        info->extra_argument = extraArgument;
        plannedWorkers++;
    }

    GStats.ParallelWorkerEntrypointInstalled = (plannedWorkers > 0u);
    GStats.PlannedParallelWorkerCount = plannedWorkers;
    if (!GStats.ParallelWorkerEntrypointInstalled) {
        GStats.ParallelWorkersEnabled = false;
    }

    return GStats.ParallelWorkerEntrypointInstalled;
}

bool FCpuCoreManager::SetParallelWorkersEnabled(bool enabled) {
    if (!enabled) {
        __atomic_store_n(&GStats.ParallelWorkersEnabled, false, __ATOMIC_RELEASE);
        return true;
    }

    if (!GStats.ParallelWorkersSupported || !GStats.ParallelWorkerEntrypointInstalled) {
        return false;
    }

    __atomic_store_n(&GStats.ParallelWorkersEnabled, true, __ATOMIC_RELEASE);
    return true;
}

bool FCpuCoreManager::AreParallelWorkersEnabled() {
    return __atomic_load_n(&GStats.ParallelWorkersEnabled, __ATOMIC_ACQUIRE);
}

void FCpuCoreManager::GetStats(FCpuCoreManagerStats &outStats) {
    outStats = GStats;
}

} // namespace Fortress::Kernel