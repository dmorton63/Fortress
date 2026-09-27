#include "Fortress/Memory/FMemoryArena.hpp"

namespace Fortress::Memory {

Fortress::Core::uint8 *FMemoryArena::GBase = nullptr;
Fortress::Core::usize FMemoryArena::GCapacity = 0;
Fortress::Core::usize FMemoryArena::GUsed = 0;
Fortress::Core::usize FMemoryArena::GPeak = 0;
Fortress::Core::usize FMemoryArena::GAllocationCount = 0;

static Fortress::Core::usize AlignUp(Fortress::Core::usize value, Fortress::Core::usize alignment) {
    const Fortress::Core::usize mask = alignment - 1;
    return (value + mask) & ~mask;
}

bool FMemoryArena::Initialize(void *baseAddress, Fortress::Core::usize capacityBytes) {
    if (baseAddress == nullptr || capacityBytes == 0) {
        return false;
    }

    GBase = static_cast<Fortress::Core::uint8 *>(baseAddress);
    GCapacity = capacityBytes;
    GUsed = 0;
    GPeak = 0;
    GAllocationCount = 0;
    return true;
}

void *FMemoryArena::Allocate(Fortress::Core::usize sizeBytes, Fortress::Core::usize alignment) {
    if (GBase == nullptr || sizeBytes == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return nullptr;
    }

    const Fortress::Core::usize alignedUsed = AlignUp(GUsed, alignment);
    if (alignedUsed > GCapacity || sizeBytes > (GCapacity - alignedUsed)) {
        return nullptr;
    }

    void *ptr = GBase + alignedUsed;
    GUsed = alignedUsed + sizeBytes;
    if (GUsed > GPeak) {
        GPeak = GUsed;
    }
    GAllocationCount++;
    return ptr;
}

void FMemoryArena::Reset() {
    GUsed = 0;
    GPeak = 0;
    GAllocationCount = 0;
}

FMemoryStats FMemoryArena::GetStats() {
    return FMemoryStats{
        .CapacityBytes = GCapacity,
        .UsedBytes = GUsed,
        .PeakBytes = GPeak,
        .AllocationCount = GAllocationCount,
    };
}

} // namespace Fortress::Memory
