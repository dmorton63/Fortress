#include "Fortress/Kernel/FPortManager.hpp"

namespace Fortress::Kernel {

namespace {

static constexpr Fortress::Core::uint32 GMaxPorts = 32u;
static constexpr Fortress::Core::uint32 GMaxAudits = 64u;

struct FPortRecord {
    bool InUse = false;
    Fortress::Core::uint16 PortId = 0;
    const char *Name = nullptr;
    FPortLease Lease = {};
};

static bool GInitialized = false;
static FPortRecord GPorts[GMaxPorts] = {};
static Fortress::Core::uint32 GNextLeaseId = 1u;
static FPortAuditEntry GAudits[GMaxAudits] = {};
static Fortress::Core::uint32 GAuditCount = 0u;
static Fortress::Core::uint64 GNextAuditSequence = 1u;
static FPortManagerStats GStats = {};

static Fortress::Core::int32 FindPortIndex(Fortress::Core::uint16 portId) {
    for (Fortress::Core::uint32 i = 0; i < GMaxPorts; i++) {
        if (GPorts[i].InUse && GPorts[i].PortId == portId) {
            return static_cast<Fortress::Core::int32>(i);
        }
    }

    return -1;
}

static Fortress::Core::int32 FindFreePortIndex() {
    for (Fortress::Core::uint32 i = 0; i < GMaxPorts; i++) {
        if (!GPorts[i].InUse) {
            return static_cast<Fortress::Core::int32>(i);
        }
    }

    return -1;
}

static void RecordAudit(Fortress::Core::uint16 portId,
                        Fortress::Core::uint32 serviceId,
                        Fortress::Core::uint32 leaseId,
                        EPortAuditAction action,
                        EPortAuditResult result) {
    const FPortAuditEntry entry{
        .Sequence = GNextAuditSequence++,
        .PortId = portId,
        .ServiceId = serviceId,
        .LeaseId = leaseId,
        .Action = action,
        .Result = result,
    };

    if (GAuditCount < GMaxAudits) {
        GAudits[GAuditCount++] = entry;
        return;
    }

    for (Fortress::Core::uint32 i = 1; i < GMaxAudits; i++) {
        GAudits[i - 1u] = GAudits[i];
    }
    GAudits[GMaxAudits - 1u] = entry;
}

} // namespace

bool FPortManager::Initialize() {
    GInitialized = true;
    GNextLeaseId = 1u;
    GAuditCount = 0u;
    GNextAuditSequence = 1u;
    GStats = FPortManagerStats{};

    for (Fortress::Core::uint32 i = 0; i < GMaxPorts; i++) {
        GPorts[i] = FPortRecord{};
    }
    for (Fortress::Core::uint32 i = 0; i < GMaxAudits; i++) {
        GAudits[i] = FPortAuditEntry{};
    }

    return true;
}

bool FPortManager::RegisterPort(Fortress::Core::uint16 portId, const char *name) {
    if (!GInitialized || portId == 0u || name == nullptr || name[0] == '\0') {
        RecordAudit(portId, 0u, 0u, EPortAuditAction::Register, EPortAuditResult::Error);
        return false;
    }

    if (FindPortIndex(portId) >= 0) {
        RecordAudit(portId, 0u, 0u, EPortAuditAction::Register, EPortAuditResult::Denied);
        return false;
    }

    const Fortress::Core::int32 freeIndex = FindFreePortIndex();
    if (freeIndex < 0) {
        RecordAudit(portId, 0u, 0u, EPortAuditAction::Register, EPortAuditResult::Error);
        return false;
    }

    GPorts[freeIndex] = FPortRecord{
        .InUse = true,
        .PortId = portId,
        .Name = name,
        .Lease = FPortLease{},
    };

    GStats.RegisteredPortCount++;
    RecordAudit(portId, 0u, 0u, EPortAuditAction::Register, EPortAuditResult::Allowed);
    return true;
}

bool FPortManager::OpenLease(Fortress::Core::uint16 portId,
                             Fortress::Core::uint32 serviceId,
                             const char *ownerName,
                             Fortress::Core::uint32 &outLeaseId) {
    outLeaseId = 0u;
    if (!GInitialized || serviceId == 0u || ownerName == nullptr || ownerName[0] == '\0') {
        RecordAudit(portId, serviceId, 0u, EPortAuditAction::LeaseOpen, EPortAuditResult::Error);
        return false;
    }

    const Fortress::Core::int32 portIndex = FindPortIndex(portId);
    if (portIndex < 0) {
        RecordAudit(portId, serviceId, 0u, EPortAuditAction::LeaseOpen, EPortAuditResult::Denied);
        GStats.DeniedAccessCount++;
        return false;
    }

    FPortRecord &port = GPorts[portIndex];
    if (port.Lease.InUse) {
        RecordAudit(portId, serviceId, port.Lease.LeaseId, EPortAuditAction::LeaseOpen, EPortAuditResult::Denied);
        GStats.DeniedAccessCount++;
        return false;
    }

    const Fortress::Core::uint32 leaseId = GNextLeaseId++;
    port.Lease = FPortLease{
        .InUse = true,
        .PortId = portId,
        .LeaseId = leaseId,
        .ServiceId = serviceId,
        .OwnerName = ownerName,
    };

    GStats.ActiveLeaseCount++;
    GStats.LeaseOpenCount++;
    outLeaseId = leaseId;
    RecordAudit(portId, serviceId, leaseId, EPortAuditAction::LeaseOpen, EPortAuditResult::Allowed);
    return true;
}

bool FPortManager::CloseLease(Fortress::Core::uint16 portId,
                              Fortress::Core::uint32 serviceId,
                              Fortress::Core::uint32 leaseId) {
    if (!GInitialized || serviceId == 0u || leaseId == 0u) {
        RecordAudit(portId, serviceId, leaseId, EPortAuditAction::LeaseClose, EPortAuditResult::Error);
        return false;
    }

    const Fortress::Core::int32 portIndex = FindPortIndex(portId);
    if (portIndex < 0) {
        RecordAudit(portId, serviceId, leaseId, EPortAuditAction::LeaseClose, EPortAuditResult::Denied);
        GStats.DeniedAccessCount++;
        return false;
    }

    FPortRecord &port = GPorts[portIndex];
    if (!port.Lease.InUse || port.Lease.ServiceId != serviceId || port.Lease.LeaseId != leaseId) {
        RecordAudit(portId, serviceId, leaseId, EPortAuditAction::LeaseClose, EPortAuditResult::Denied);
        GStats.DeniedAccessCount++;
        return false;
    }

    port.Lease = FPortLease{};
    if (GStats.ActiveLeaseCount > 0u) {
        GStats.ActiveLeaseCount--;
    }
    GStats.LeaseCloseCount++;
    RecordAudit(portId, serviceId, leaseId, EPortAuditAction::LeaseClose, EPortAuditResult::Allowed);
    return true;
}

bool FPortManager::CanServiceAccessPort(Fortress::Core::uint16 portId, Fortress::Core::uint32 serviceId) {
    if (!GInitialized || serviceId == 0u) {
        RecordAudit(portId, serviceId, 0u, EPortAuditAction::AccessCheck, EPortAuditResult::Error);
        return false;
    }

    const Fortress::Core::int32 portIndex = FindPortIndex(portId);
    if (portIndex < 0) {
        RecordAudit(portId, serviceId, 0u, EPortAuditAction::AccessCheck, EPortAuditResult::Denied);
        GStats.DeniedAccessCount++;
        return false;
    }

    const FPortRecord &port = GPorts[portIndex];
    const bool allowed = port.Lease.InUse && port.Lease.ServiceId == serviceId;
    if (!allowed) {
        GStats.DeniedAccessCount++;
    }
    RecordAudit(portId,
                serviceId,
                port.Lease.InUse ? port.Lease.LeaseId : 0u,
                EPortAuditAction::AccessCheck,
                allowed ? EPortAuditResult::Allowed : EPortAuditResult::Denied);
    return allowed;
}

bool FPortManager::IsPortRegistered(Fortress::Core::uint16 portId) {
    return GInitialized && FindPortIndex(portId) >= 0;
}

bool FPortManager::IsPortOpen(Fortress::Core::uint16 portId) {
    if (!GInitialized) {
        return false;
    }

    const Fortress::Core::int32 portIndex = FindPortIndex(portId);
    if (portIndex < 0) {
        return false;
    }

    return GPorts[portIndex].Lease.InUse;
}

bool FPortManager::GetPortLease(Fortress::Core::uint16 portId, FPortLease &outLease) {
    outLease = FPortLease{};
    if (!GInitialized) {
        return false;
    }

    const Fortress::Core::int32 portIndex = FindPortIndex(portId);
    if (portIndex < 0 || !GPorts[portIndex].Lease.InUse) {
        return false;
    }

    outLease = GPorts[portIndex].Lease;
    return true;
}

bool FPortManager::GetPortName(Fortress::Core::uint16 portId, const char *&outName) {
    outName = nullptr;
    if (!GInitialized) {
        return false;
    }

    const Fortress::Core::int32 portIndex = FindPortIndex(portId);
    if (portIndex < 0) {
        return false;
    }

    outName = GPorts[portIndex].Name;
    return true;
}

void FPortManager::GetPorts(FPortRecordSnapshot *outPorts,
                            Fortress::Core::uint32 capacity,
                            Fortress::Core::uint32 &outCount) {
    outCount = 0u;
    if (!GInitialized || outPorts == nullptr || capacity == 0u) {
        return;
    }

    for (Fortress::Core::uint32 i = 0; i < GMaxPorts; i++) {
        if (!GPorts[i].InUse) {
            continue;
        }

        if (outCount >= capacity) {
            break;
        }

        outPorts[outCount++] = FPortRecordSnapshot{
            .PortId = GPorts[i].PortId,
            .Name = GPorts[i].Name,
            .Lease = GPorts[i].Lease,
        };
    }
}

bool FPortManager::GetLastAuditEntry(FPortAuditEntry &outEntry) {
    outEntry = FPortAuditEntry{};
    if (!GInitialized || GAuditCount == 0u) {
        return false;
    }

    outEntry = GAudits[GAuditCount - 1u];
    return true;
}

bool FPortManager::GetLastDeniedAuditEntry(FPortAuditEntry &outEntry) {
    outEntry = FPortAuditEntry{};
    if (!GInitialized || GAuditCount == 0u) {
        return false;
    }

    for (Fortress::Core::uint32 i = GAuditCount; i > 0u; i--) {
        const FPortAuditEntry &entry = GAudits[i - 1u];
        if (entry.Result == EPortAuditResult::Denied) {
            outEntry = entry;
            return true;
        }
    }

    return false;
}

void FPortManager::GetStats(FPortManagerStats &outStats) {
    outStats = GStats;
}

void FPortManager::GetAuditEntries(FPortAuditEntry *outEntries,
                                   Fortress::Core::uint32 capacity,
                                   Fortress::Core::uint32 &outCount) {
    outCount = 0u;
    if (!GInitialized || outEntries == nullptr || capacity == 0u || GAuditCount == 0u) {
        return;
    }

    const Fortress::Core::uint32 copyCount = (GAuditCount < capacity) ? GAuditCount : capacity;
    for (Fortress::Core::uint32 i = 0; i < copyCount; i++) {
        outEntries[i] = GAudits[i];
    }
    outCount = copyCount;
}

} // namespace Fortress::Kernel