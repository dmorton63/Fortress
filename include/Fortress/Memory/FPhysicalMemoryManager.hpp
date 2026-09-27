#ifndef FORTRESS_MEMORY_FPHYSICALMEMORYMANAGER_HPP
#define FORTRESS_MEMORY_FPHYSICALMEMORYMANAGER_HPP

#include "Fortress/Core/FTypes.hpp"
#include "limine.h"

namespace Fortress::Memory {

struct FPhysicalMemoryStats {
    Fortress::Core::uint64 TotalUsableBytes;
    Fortress::Core::uint64 TotalPages;
    Fortress::Core::uint64 FreePages;
    Fortress::Core::uint64 ReservedPages;
    Fortress::Core::uint64 RangeCount;
    Fortress::Core::uint64 BitmapBytes;
};

class FPhysicalMemoryManager {
  public:
    static bool Initialize(const limine_memmap_response *memmapResponse);
    static Fortress::Core::uint64 AllocatePage();
    static Fortress::Core::uint64 AllocatePages(Fortress::Core::uint64 pageCount, Fortress::Core::uint64 alignmentPages = 1);
    static Fortress::Core::uint64 AllocatePagesBelow(Fortress::Core::uint64 pageCount,
                             Fortress::Core::uint64 alignmentPages,
                             Fortress::Core::uint64 maxPhysicalAddressInclusive);
    static bool FreePage(Fortress::Core::uint64 physicalAddress);
    static bool FreePages(Fortress::Core::uint64 physicalAddress, Fortress::Core::uint64 pageCount);
    static Fortress::Core::uint64 ReserveRange(Fortress::Core::uint64 physicalBase, Fortress::Core::uint64 lengthBytes);
    static FPhysicalMemoryStats GetStats();

  private:
    struct FRange {
        Fortress::Core::uint64 Base;
        Fortress::Core::uint64 PageCount;
        Fortress::Core::uint64 FreePages;
        Fortress::Core::uint64 NextHint;
        Fortress::Core::uint8 *Bitmap;
        Fortress::Core::uint64 BitmapBytes;
    };

    static constexpr Fortress::Core::uint64 PageSize = 4096;
    static constexpr Fortress::Core::uint64 MaxRanges = 128;

    static FRange GRanges[MaxRanges];
    static Fortress::Core::uint64 GRangeCount;
    static Fortress::Core::uint64 GTotalUsableBytes;
    static Fortress::Core::uint64 GTotalPages;
    static Fortress::Core::uint64 GFreePages;
    static Fortress::Core::uint64 GBitmapBytes;
    static Fortress::Core::uint64 GNextRangeHint;
};

} // namespace Fortress::Memory

#endif
