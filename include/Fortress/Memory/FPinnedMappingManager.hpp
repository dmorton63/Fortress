#ifndef FORTRESS_MEMORY_FPINNEDMAPPINGMANAGER_HPP
#define FORTRESS_MEMORY_FPINNEDMAPPINGMANAGER_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Memory {

struct FPinnedMapping {
    Fortress::Core::uint64 VirtualAddress;
    Fortress::Core::uint64 PhysicalAddress;
    Fortress::Core::uint64 SizeBytes;
    Fortress::Core::uint64 PageCount;
    Fortress::Core::uint64 Flags;
    bool Valid;
};

struct FPinnedMappingStats {
    Fortress::Core::uint64 RegionBase;
    Fortress::Core::uint64 RegionPages;
    Fortress::Core::uint64 ActiveMappings;
    Fortress::Core::uint64 ReservedPages;
};

class FPinnedMappingManager {
  public:
    static constexpr Fortress::Core::uint64 MaxMappings = 128;
    static constexpr Fortress::Core::uint64 MaxFreeRanges = 128;

    static bool Initialize(Fortress::Core::uint64 virtualRegionBase, Fortress::Core::uint64 virtualRegionPages);
    static bool MapPhysicalRange(Fortress::Core::uint64 physicalAddress,
                                 Fortress::Core::uint64 sizeBytes,
                                 Fortress::Core::uint64 flags,
                                 FPinnedMapping &outMapping);
    static bool UnmapPhysicalRange(FPinnedMapping &mapping);
    static FPinnedMappingStats GetStats();
};

} // namespace Fortress::Memory

#endif
