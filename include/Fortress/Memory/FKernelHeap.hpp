#ifndef FORTRESS_MEMORY_FKERNELHEAP_HPP
#define FORTRESS_MEMORY_FKERNELHEAP_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Memory {

struct FKernelHeapStats {
    Fortress::Core::uint64 BaseVirtualAddress;
    Fortress::Core::uint64 CapacityBytes;
    Fortress::Core::uint64 UsedBytes;
    Fortress::Core::uint64 FreeBytes;
};

class FKernelHeap {
  public:
    static bool Initialize(Fortress::Core::uint64 baseVirtualAddress,
                           Fortress::Core::uint64 initialPages,
                           Fortress::Core::uint64 mapFlags);
    static void *Allocate(Fortress::Core::uint64 sizeBytes, Fortress::Core::uint64 alignment = 16);
    static bool Free(void *address);
    static FKernelHeapStats GetStats();

  private:
    struct FSegment {
        Fortress::Core::uint64 Offset;
        Fortress::Core::uint64 SizeBytes;
        bool Free;
    };

    static constexpr Fortress::Core::uint64 MaxSegments = 1024;

    static bool GrowToFit(Fortress::Core::uint64 requiredBytes);
    static bool InsertSegment(Fortress::Core::uint64 index, const FSegment &segment);
    static void MergeAround(Fortress::Core::uint64 index);

    static bool GInitialized;
    static Fortress::Core::uint64 GBaseVirtualAddress;
    static Fortress::Core::uint64 GCapacityBytes;
    static Fortress::Core::uint64 GUsedBytes;
    static Fortress::Core::uint64 GMapFlags;
    static FSegment GSegments[MaxSegments];
    static Fortress::Core::uint64 GSegmentCount;
};

} // namespace Fortress::Memory

#endif
