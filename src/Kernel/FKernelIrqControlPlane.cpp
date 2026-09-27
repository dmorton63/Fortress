#include "Fortress/Kernel/FKernelIrqControlPlane.hpp"

namespace Fortress::Kernel {

namespace {

static bool GInitialized = false;
static FKernelIrqLease GLeases[256] = {};
static FKernelIrqControlStats GStats = {};

static bool IsManagedVector(Fortress::Core::uint8 vector) {
    return vector >= FKernelIrqControlPlane::FirstManagedVector;
}

static void RecountMaskedVectors() {
    Fortress::Core::uint32 masked = 0;
    for (Fortress::Core::uint32 i = 0; i < 256u; i++) {
        if (GLeases[i].InUse && GLeases[i].Masked) {
            masked++;
        }
    }
    GStats.MaskedCount = masked;
}

} // namespace

bool FKernelIrqControlPlane::Initialize() {
    GInitialized = true;
    GStats = FKernelIrqControlStats{};

    for (Fortress::Core::uint32 i = 0; i < 256u; i++) {
        GLeases[i] = FKernelIrqLease{};
        GLeases[i].Vector = static_cast<Fortress::Core::uint8>(i);
        GLeases[i].Masked = true;
    }

    return true;
}

bool FKernelIrqControlPlane::RegisterVector(Fortress::Core::uint8 vector,
                                            Fortress::Core::uint32 serviceId,
                                            const char *ownerName,
                                            bool maskedByDefault) {
    if (!GInitialized || !IsManagedVector(vector) || serviceId == 0u || ownerName == nullptr || ownerName[0] == '\0') {
        return false;
    }

    FKernelIrqLease &lease = GLeases[vector];
    if (lease.InUse) {
        return false;
    }

    lease = FKernelIrqLease{
        .InUse = true,
        .Vector = vector,
        .ServiceId = serviceId,
        .OwnerName = ownerName,
        .Masked = maskedByDefault,
        .HitCount = 0,
        .DroppedCount = 0,
    };

    GStats.RegisteredCount++;
    RecountMaskedVectors();
    return true;
}

bool FKernelIrqControlPlane::UnregisterVector(Fortress::Core::uint8 vector, Fortress::Core::uint32 serviceId) {
    if (!GInitialized || !IsManagedVector(vector) || serviceId == 0u) {
        return false;
    }

    FKernelIrqLease &lease = GLeases[vector];
    if (!lease.InUse || lease.ServiceId != serviceId) {
        return false;
    }

    lease = FKernelIrqLease{.Vector = vector, .Masked = true};
    if (GStats.RegisteredCount > 0u) {
        GStats.RegisteredCount--;
    }

    RecountMaskedVectors();
    return true;
}

bool FKernelIrqControlPlane::SetMasked(Fortress::Core::uint8 vector, bool masked) {
    if (!GInitialized || !IsManagedVector(vector)) {
        return false;
    }

    FKernelIrqLease &lease = GLeases[vector];
    if (!lease.InUse) {
        return false;
    }

    lease.Masked = masked;
    RecountMaskedVectors();
    return true;
}

bool FKernelIrqControlPlane::IsMasked(Fortress::Core::uint8 vector) {
    if (!GInitialized || !IsManagedVector(vector)) {
        return true;
    }

    const FKernelIrqLease &lease = GLeases[vector];
    if (!lease.InUse) {
        return true;
    }

    return lease.Masked;
}

bool FKernelIrqControlPlane::RecordInterrupt(Fortress::Core::uint8 vector) {
    if (!GInitialized || !IsManagedVector(vector)) {
        return false;
    }

    FKernelIrqLease &lease = GLeases[vector];
    if (!lease.InUse) {
        GStats.DroppedCount++;
        return false;
    }

    if (lease.Masked) {
        lease.DroppedCount++;
        GStats.DroppedCount++;
        return false;
    }

    lease.HitCount++;
    GStats.DeliveredCount++;
    return true;
}

bool FKernelIrqControlPlane::GetLease(Fortress::Core::uint8 vector, FKernelIrqLease &outLease) {
    outLease = FKernelIrqLease{};
    if (!GInitialized || !IsManagedVector(vector)) {
        return false;
    }

    const FKernelIrqLease &lease = GLeases[vector];
    if (!lease.InUse) {
        return false;
    }

    outLease = lease;
    return true;
}

void FKernelIrqControlPlane::GetStats(FKernelIrqControlStats &outStats) {
    outStats = GStats;
}

} // namespace Fortress::Kernel
