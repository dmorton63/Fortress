#include "Fortress/Kernel/FServiceRegistry.hpp"

namespace Fortress::Kernel {

static constexpr Fortress::Core::uint32 GMaxServices = 32;

static bool GInitialized = false;
static FServiceRegistrationInfo GServices[GMaxServices] = {};
static Fortress::Core::uint32 GServiceCount = 0;
static Fortress::Core::uint64 GFailedStartCount = 0;
static Fortress::Core::uint64 GRestartAttemptCount = 0;
static Fortress::Core::uint64 GDependencyViolationCount = 0;

bool FServiceRegistry::Initialize() {
    GInitialized = true;
    GServiceCount = 0;
    GFailedStartCount = 0;
    GRestartAttemptCount = 0;
    GDependencyViolationCount = 0;
    for (Fortress::Core::uint32 i = 0; i < GMaxServices; i++) {
        GServices[i] = FServiceRegistrationInfo{};
    }
    return true;
}

bool FServiceRegistry::RegisterService(const FServiceRegistrationInfo &serviceInfo) {
    if (!GInitialized || serviceInfo.ServiceId == 0 || serviceInfo.Name == nullptr || serviceInfo.Name[0] == '\0') {
        GFailedStartCount++;
        return false;
    }

    for (Fortress::Core::uint32 i = 0; i < GServiceCount; i++) {
        if (GServices[i].ServiceId == serviceInfo.ServiceId) {
            GFailedStartCount++;
            return false;
        }
    }

    if (GServiceCount >= GMaxServices) {
        GFailedStartCount++;
        return false;
    }

    GServices[GServiceCount++] = serviceInfo;
    return true;
}

bool FServiceRegistry::FindServiceById(Fortress::Core::uint32 serviceId, FServiceRegistrationInfo &outServiceInfo) {
    if (!GInitialized || serviceId == 0) {
        return false;
    }

    for (Fortress::Core::uint32 i = 0; i < GServiceCount; i++) {
        if (GServices[i].ServiceId == serviceId) {
            outServiceInfo = GServices[i];
            return true;
        }
    }

    return false;
}

void FServiceRegistry::GetServices(FServiceRegistrationInfo *outServices,
                                   Fortress::Core::uint32 capacity,
                                   Fortress::Core::uint32 &outCount) {
    outCount = 0u;
    if (!GInitialized || outServices == nullptr || capacity == 0u) {
        return;
    }

    const Fortress::Core::uint32 copyCount = (GServiceCount < capacity) ? GServiceCount : capacity;
    for (Fortress::Core::uint32 i = 0; i < copyCount; i++) {
        outServices[i] = GServices[i];
    }
    outCount = copyCount;
}

void FServiceRegistry::RecordServiceRestartAttempt(Fortress::Core::uint32 serviceId) {
    if (!GInitialized || serviceId == 0u) {
        return;
    }

    GRestartAttemptCount++;
}

void FServiceRegistry::RecordServiceDependencyViolation(Fortress::Core::uint32 serviceId,
                                                        Fortress::Core::uint32 dependencyServiceId) {
    if (!GInitialized || serviceId == 0u || dependencyServiceId == 0u) {
        return;
    }

    GDependencyViolationCount++;
}

void FServiceRegistry::GetStats(FServiceRegistryStats &outStats) {
    outStats = FServiceRegistryStats{
        .ServiceCount = GServiceCount,
        .FailedStartCount = GFailedStartCount,
        .RestartAttemptCount = GRestartAttemptCount,
        .DependencyViolationCount = GDependencyViolationCount,
    };
}

} // namespace Fortress::Kernel
