#ifndef FORTRESS_KERNEL_FCPUCOREMANAGER_HPP
#define FORTRESS_KERNEL_FCPUCOREMANAGER_HPP

#include "Fortress/Core/FTypes.hpp"
#include "limine.h"

namespace Fortress::Kernel {

struct FCpuCoreInfo {
    Fortress::Core::uint32 CoreId = 0;
    Fortress::Core::uint32 LocalApicId = 0;
    bool IsBootstrapProcessor = false;
    bool IsOnline = false;
};

struct FCpuCoreManagerStats {
    Fortress::Core::uint32 DetectedCoreCount = 0;
    Fortress::Core::uint32 OnlineCoreCount = 0;
    Fortress::Core::uint32 BootstrapCoreId = 0;
    bool MpAvailable = false;
    bool ParallelWorkersSupported = false;
    bool ParallelWorkerEntrypointInstalled = false;
    bool ParallelWorkersEnabled = false;
    Fortress::Core::uint32 PlannedParallelWorkerCount = 0;
};

class FCpuCoreManager {
  public:
    static bool Initialize(const limine_smp_response *mpResponse);

    static Fortress::Core::uint32 GetOnlineCoreCount();
    static Fortress::Core::uint32 GetBootstrapCoreId();
    static bool GetCoreInfo(Fortress::Core::uint32 index, FCpuCoreInfo &outInfo);
    static bool TryGetCoreIdByLocalApicId(Fortress::Core::uint32 localApicId, Fortress::Core::uint32 &outCoreId);

    // Installs AP worker entrypoint pointers but does not automatically enable parallel execution.
    static bool InstallParallelWorkerEntrypoint(limine_goto_address entrypoint, Fortress::Core::uint64 extraArgument);
    static bool SetParallelWorkersEnabled(bool enabled);
    static bool AreParallelWorkersEnabled();

    static void GetStats(FCpuCoreManagerStats &outStats);
};

} // namespace Fortress::Kernel

#endif