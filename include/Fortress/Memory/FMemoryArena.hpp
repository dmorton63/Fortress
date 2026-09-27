#ifndef FORTRESS_MEMORY_FMEMORYARENA_HPP
#define FORTRESS_MEMORY_FMEMORYARENA_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Memory {

struct FMemoryStats {
    Fortress::Core::usize CapacityBytes;
    Fortress::Core::usize UsedBytes;
    Fortress::Core::usize PeakBytes;
    Fortress::Core::usize AllocationCount;
};

class FMemoryArena {
  public:
    static bool Initialize(void *baseAddress, Fortress::Core::usize capacityBytes);
    static void *Allocate(Fortress::Core::usize sizeBytes, Fortress::Core::usize alignment = 16);
    static void Reset();
    static FMemoryStats GetStats();

  private:
    static Fortress::Core::uint8 *GBase;
    static Fortress::Core::usize GCapacity;
    static Fortress::Core::usize GUsed;
    static Fortress::Core::usize GPeak;
    static Fortress::Core::usize GAllocationCount;
};

} // namespace Fortress::Memory

#endif
