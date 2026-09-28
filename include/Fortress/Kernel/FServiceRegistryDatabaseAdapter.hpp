#ifndef FORTRESS_KERNEL_FSERVICEREGISTRYDATABASEADAPTER_HPP
#define FORTRESS_KERNEL_FSERVICEREGISTRYDATABASEADAPTER_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

struct FServiceRegistryDatabaseRecord {
    Fortress::Core::uint32 ServiceId = 0;
    Fortress::Core::uint32 EndpointChannelId = 0;
    const char *Name = nullptr;
    bool Active = false;
};

struct FServiceRegistryDatabaseAdapterStats {
    Fortress::Core::uint32 RecordCount = 0;
    Fortress::Core::uint64 RefreshCount = 0;
    Fortress::Core::uint64 FailedLookupCount = 0;
};

class FServiceRegistryDatabaseAdapter {
  public:
    static bool Initialize();
    static bool RefreshFromServiceRegistry();
    static bool FindRecordByServiceId(Fortress::Core::uint32 serviceId, FServiceRegistryDatabaseRecord &outRecord);
    static void GetRecords(FServiceRegistryDatabaseRecord *outRecords,
                           Fortress::Core::uint32 capacity,
                           Fortress::Core::uint32 &outCount);
    static void GetStats(FServiceRegistryDatabaseAdapterStats &outStats);
};

} // namespace Fortress::Kernel

#endif