#ifndef FORTRESS_KERNEL_FKERNELNETWORKTELEMETRY_HPP
#define FORTRESS_KERNEL_FKERNELNETWORKTELEMETRY_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

struct FKernelNetworkStats {
    Fortress::Core::uint32 InterfaceCount = 0;
    Fortress::Core::uint64 RxCount = 0;
    Fortress::Core::uint64 TxCount = 0;
    Fortress::Core::uint64 DropCount = 0;
    bool LinkUp = false;
};

class FKernelNetworkTelemetry {
  public:
    static bool Initialize();
    static void SetInterfaceCount(Fortress::Core::uint32 interfaceCount);
    static void SetLinkState(bool linkUp);
    static void RecordRx(Fortress::Core::uint64 packetCount);
    static void RecordTx(Fortress::Core::uint64 packetCount);
    static void RecordDrop(Fortress::Core::uint64 packetCount);
    static void GetStats(FKernelNetworkStats &outStats);
};

} // namespace Fortress::Kernel

#endif
