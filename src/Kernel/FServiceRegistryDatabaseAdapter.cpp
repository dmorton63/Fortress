#include "Fortress/Kernel/FServiceRegistryDatabaseAdapter.hpp"

#include "Fortress/Kernel/FServiceRegistry.hpp"

namespace Fortress::Kernel {

namespace {

static constexpr Fortress::Core::uint32 GMaxServiceDbRecords = 32u;

static bool GInitialized = false;
static FServiceRegistryDatabaseRecord GRecords[GMaxServiceDbRecords] = {};
static Fortress::Core::uint32 GRecordCount = 0u;
static Fortress::Core::uint64 GRefreshCount = 0u;
static Fortress::Core::uint64 GFailedLookupCount = 0u;

} // namespace

bool FServiceRegistryDatabaseAdapter::Initialize() {
    GInitialized = true;
    GRecordCount = 0u;
    GRefreshCount = 0u;
    GFailedLookupCount = 0u;
    for (Fortress::Core::uint32 i = 0; i < GMaxServiceDbRecords; i++) {
        GRecords[i] = FServiceRegistryDatabaseRecord{};
    }
    return true;
}

bool FServiceRegistryDatabaseAdapter::RefreshFromServiceRegistry() {
    if (!GInitialized) {
        return false;
    }

    FServiceRegistrationInfo services[GMaxServiceDbRecords] = {};
    Fortress::Core::uint32 serviceCount = 0u;
    FServiceRegistry::GetServices(services, GMaxServiceDbRecords, serviceCount);

    for (Fortress::Core::uint32 i = 0; i < GMaxServiceDbRecords; i++) {
        GRecords[i] = FServiceRegistryDatabaseRecord{};
    }

    GRecordCount = serviceCount;
    for (Fortress::Core::uint32 i = 0; i < serviceCount; i++) {
        GRecords[i] = FServiceRegistryDatabaseRecord{
            .ServiceId = services[i].ServiceId,
            .EndpointChannelId = services[i].EndpointChannelId,
            .Name = services[i].Name,
            .Active = true,
        };
    }

    GRefreshCount++;
    return true;
}

bool FServiceRegistryDatabaseAdapter::FindRecordByServiceId(Fortress::Core::uint32 serviceId,
                                                            FServiceRegistryDatabaseRecord &outRecord) {
    outRecord = FServiceRegistryDatabaseRecord{};
    if (!GInitialized || serviceId == 0u) {
        GFailedLookupCount++;
        return false;
    }

    for (Fortress::Core::uint32 i = 0; i < GRecordCount; i++) {
        if (GRecords[i].ServiceId == serviceId) {
            outRecord = GRecords[i];
            return true;
        }
    }

    GFailedLookupCount++;
    return false;
}

void FServiceRegistryDatabaseAdapter::GetRecords(FServiceRegistryDatabaseRecord *outRecords,
                                                 Fortress::Core::uint32 capacity,
                                                 Fortress::Core::uint32 &outCount) {
    outCount = 0u;
    if (!GInitialized || outRecords == nullptr || capacity == 0u) {
        return;
    }

    const Fortress::Core::uint32 copyCount = (GRecordCount < capacity) ? GRecordCount : capacity;
    for (Fortress::Core::uint32 i = 0; i < copyCount; i++) {
        outRecords[i] = GRecords[i];
    }
    outCount = copyCount;
}

void FServiceRegistryDatabaseAdapter::GetStats(FServiceRegistryDatabaseAdapterStats &outStats) {
    outStats = FServiceRegistryDatabaseAdapterStats{
        .RecordCount = GRecordCount,
        .RefreshCount = GRefreshCount,
        .FailedLookupCount = GFailedLookupCount,
    };
}

} // namespace Fortress::Kernel