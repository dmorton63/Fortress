#include "Fortress/Kernel/FServiceRegistry.hpp"

namespace Fortress::Kernel {

static constexpr Fortress::Core::uint32 GMaxServices = 32;

static bool GInitialized = false;
static FServiceRegistrationInfo GServices[GMaxServices] = {};
static Fortress::Core::uint32 GServiceCount = 0;

bool FServiceRegistry::Initialize() {
    GInitialized = true;
    GServiceCount = 0;
    for (Fortress::Core::uint32 i = 0; i < GMaxServices; i++) {
        GServices[i] = FServiceRegistrationInfo{};
    }
    return true;
}

bool FServiceRegistry::RegisterService(const FServiceRegistrationInfo &serviceInfo) {
    if (!GInitialized || serviceInfo.ServiceId == 0 || serviceInfo.Name == nullptr || serviceInfo.Name[0] == '\0') {
        return false;
    }

    for (Fortress::Core::uint32 i = 0; i < GServiceCount; i++) {
        if (GServices[i].ServiceId == serviceInfo.ServiceId) {
            return false;
        }
    }

    if (GServiceCount >= GMaxServices) {
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

void FServiceRegistry::GetStats(FServiceRegistryStats &outStats) {
    outStats = FServiceRegistryStats{
        .ServiceCount = GServiceCount,
    };
}

} // namespace Fortress::Kernel
