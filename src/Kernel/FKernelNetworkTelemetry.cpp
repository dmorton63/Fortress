#include "Fortress/Kernel/FKernelNetworkTelemetry.hpp"

namespace Fortress::Kernel {

static bool GInitialized = false;
static FKernelNetworkStats GNetworkStats = {};

bool FKernelNetworkTelemetry::Initialize() {
    GInitialized = true;
    GNetworkStats = FKernelNetworkStats{};
    return true;
}

void FKernelNetworkTelemetry::SetInterfaceCount(Fortress::Core::uint32 interfaceCount) {
    if (!GInitialized) {
        return;
    }

    GNetworkStats.InterfaceCount = interfaceCount;
}

void FKernelNetworkTelemetry::SetLinkState(bool linkUp) {
    if (!GInitialized) {
        return;
    }

    GNetworkStats.LinkUp = linkUp;
}

void FKernelNetworkTelemetry::RecordRx(Fortress::Core::uint64 packetCount) {
    if (!GInitialized) {
        return;
    }

    GNetworkStats.RxCount += packetCount;
}

void FKernelNetworkTelemetry::RecordTx(Fortress::Core::uint64 packetCount) {
    if (!GInitialized) {
        return;
    }

    GNetworkStats.TxCount += packetCount;
}

void FKernelNetworkTelemetry::RecordDrop(Fortress::Core::uint64 packetCount) {
    if (!GInitialized) {
        return;
    }

    GNetworkStats.DropCount += packetCount;
}

void FKernelNetworkTelemetry::GetStats(FKernelNetworkStats &outStats) {
    outStats = GNetworkStats;
}

} // namespace Fortress::Kernel
