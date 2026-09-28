#ifndef FORTRESS_KERNEL_FPORTMANAGER_HPP
#define FORTRESS_KERNEL_FPORTMANAGER_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

enum class EPortAuditAction : Fortress::Core::uint8 {
    Register = 0u,
    LeaseOpen,
    LeaseClose,
    AccessCheck,
};

enum class EPortAuditResult : Fortress::Core::uint8 {
    Allowed = 0u,
    Denied,
    Error,
};

struct FPortLease {
    bool InUse = false;
    Fortress::Core::uint16 PortId = 0;
    Fortress::Core::uint32 LeaseId = 0;
    Fortress::Core::uint32 ServiceId = 0;
    const char *OwnerName = nullptr;
};

struct FPortRecordSnapshot {
    Fortress::Core::uint16 PortId = 0;
    const char *Name = nullptr;
    FPortLease Lease = {};
};

struct FPortAuditEntry {
    Fortress::Core::uint64 Sequence = 0;
    Fortress::Core::uint16 PortId = 0;
    Fortress::Core::uint32 ServiceId = 0;
    Fortress::Core::uint32 LeaseId = 0;
    EPortAuditAction Action = EPortAuditAction::AccessCheck;
    EPortAuditResult Result = EPortAuditResult::Error;
};

struct FPortManagerStats {
    Fortress::Core::uint32 RegisteredPortCount = 0;
    Fortress::Core::uint32 ActiveLeaseCount = 0;
    Fortress::Core::uint64 LeaseOpenCount = 0;
    Fortress::Core::uint64 LeaseCloseCount = 0;
    Fortress::Core::uint64 DeniedAccessCount = 0;
};

class FPortManager {
  public:
    static bool Initialize();
    static bool RegisterPort(Fortress::Core::uint16 portId, const char *name);
    static bool OpenLease(Fortress::Core::uint16 portId,
                          Fortress::Core::uint32 serviceId,
                          const char *ownerName,
                          Fortress::Core::uint32 &outLeaseId);
    static bool CloseLease(Fortress::Core::uint16 portId,
                           Fortress::Core::uint32 serviceId,
                           Fortress::Core::uint32 leaseId);
    static bool CanServiceAccessPort(Fortress::Core::uint16 portId, Fortress::Core::uint32 serviceId);
    static bool IsPortRegistered(Fortress::Core::uint16 portId);
    static bool IsPortOpen(Fortress::Core::uint16 portId);
    static bool GetPortLease(Fortress::Core::uint16 portId, FPortLease &outLease);
    static bool GetPortName(Fortress::Core::uint16 portId, const char *&outName);
    static void GetPorts(FPortRecordSnapshot *outPorts,
                         Fortress::Core::uint32 capacity,
                         Fortress::Core::uint32 &outCount);
    static bool GetLastAuditEntry(FPortAuditEntry &outEntry);
    static bool GetLastDeniedAuditEntry(FPortAuditEntry &outEntry);
    static void GetStats(FPortManagerStats &outStats);
    static void GetAuditEntries(FPortAuditEntry *outEntries,
                                Fortress::Core::uint32 capacity,
                                Fortress::Core::uint32 &outCount);
};

} // namespace Fortress::Kernel

#endif