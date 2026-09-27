#ifndef FORTRESS_MEMORY_FDMAMEMORYMANAGER_HPP
#define FORTRESS_MEMORY_FDMAMEMORYMANAGER_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Memory {

struct FDmaBuffer {
    Fortress::Core::uint64 VirtualAddress;
    Fortress::Core::uint64 PhysicalAddress;
    Fortress::Core::uint64 SizeBytes;
    Fortress::Core::uint64 PageCount;
    Fortress::Core::uint64 AlignmentBytes;
    bool Below4GiB;
    bool Valid;
};

struct FDmaMemoryStats {
    Fortress::Core::uint64 RegionBase;
    Fortress::Core::uint64 RegionPages;
    Fortress::Core::uint64 ActiveBuffers;
    Fortress::Core::uint64 ReservedPages;
};

class FDmaMemoryManager {
  public:
    static constexpr Fortress::Core::uint64 MaxBuffers = 128;
    static constexpr Fortress::Core::uint64 MaxFreeRanges = 128;

    static bool Initialize(Fortress::Core::uint64 virtualRegionBase, Fortress::Core::uint64 virtualRegionPages);
    static bool AllocateBuffer(Fortress::Core::uint64 sizeBytes,
                               Fortress::Core::uint64 alignmentBytes,
                               bool below4GiB,
                               FDmaBuffer &outBuffer);
    static bool FreeBuffer(FDmaBuffer &buffer);
    static FDmaMemoryStats GetStats();
};

} // namespace Fortress::Memory

#endif
