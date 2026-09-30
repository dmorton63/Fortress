#ifndef FORTRESS_KERNEL_FSERVICEREGISTRY_HPP
#define FORTRESS_KERNEL_FSERVICEREGISTRY_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

struct FServiceRegistrationInfo {
    Fortress::Core::uint32 ServiceId = 0;
    Fortress::Core::uint32 EndpointChannelId = 0;
    const char *Name = nullptr;
};

struct FServiceRegistryStats {
    Fortress::Core::uint32 ServiceCount = 0;
    Fortress::Core::uint64 FailedStartCount = 0;
    Fortress::Core::uint64 RestartAttemptCount = 0;
    Fortress::Core::uint64 DependencyViolationCount = 0;
};

class FServiceRegistry {
  public:
    static bool Initialize();
    static bool RegisterService(const FServiceRegistrationInfo &serviceInfo);
    static bool FindServiceById(Fortress::Core::uint32 serviceId, FServiceRegistrationInfo &outServiceInfo);
        static void GetServices(FServiceRegistrationInfo *outServices,
                                                        Fortress::Core::uint32 capacity,
                                                        Fortress::Core::uint32 &outCount);
    static void RecordServiceRestartAttempt(Fortress::Core::uint32 serviceId);
    static void RecordServiceDependencyViolation(Fortress::Core::uint32 serviceId,
                                                 Fortress::Core::uint32 dependencyServiceId);
    static void GetStats(FServiceRegistryStats &outStats);
};

} // namespace Fortress::Kernel

#endif