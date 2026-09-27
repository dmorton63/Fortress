#ifndef FORTRESS_KERNEL_FKERNELIRQCONTROLPLANE_HPP
#define FORTRESS_KERNEL_FKERNELIRQCONTROLPLANE_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

struct FKernelIrqLease {
    bool InUse = false;
    Fortress::Core::uint8 Vector = 0;
    Fortress::Core::uint32 ServiceId = 0;
    const char *OwnerName = nullptr;
    bool Masked = true;
    Fortress::Core::uint64 HitCount = 0;
    Fortress::Core::uint64 DroppedCount = 0;
};

struct FKernelIrqControlStats {
    Fortress::Core::uint32 RegisteredCount = 0;
    Fortress::Core::uint32 MaskedCount = 0;
    Fortress::Core::uint64 DeliveredCount = 0;
    Fortress::Core::uint64 DroppedCount = 0;
};

class FKernelIrqControlPlane {
  public:
    static constexpr Fortress::Core::uint8 FirstManagedVector = 32u;
    static constexpr Fortress::Core::uint8 LastManagedVector = 255u;

    static bool Initialize();

    static bool RegisterVector(Fortress::Core::uint8 vector,
                               Fortress::Core::uint32 serviceId,
                               const char *ownerName,
                               bool maskedByDefault);
    static bool UnregisterVector(Fortress::Core::uint8 vector, Fortress::Core::uint32 serviceId);

    static bool SetMasked(Fortress::Core::uint8 vector, bool masked);
    static bool IsMasked(Fortress::Core::uint8 vector);

    // Records a routed interrupt delivery attempt for diagnostics.
    static bool RecordInterrupt(Fortress::Core::uint8 vector);

    static bool GetLease(Fortress::Core::uint8 vector, FKernelIrqLease &outLease);
    static void GetStats(FKernelIrqControlStats &outStats);
};

} // namespace Fortress::Kernel

#endif
