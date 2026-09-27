#ifndef FORTRESS_MEMORY_FVIRTUALMEMORYMANAGER_HPP
#define FORTRESS_MEMORY_FVIRTUALMEMORYMANAGER_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Memory {

struct FVirtualMemoryStats {
    Fortress::Core::uint64 RootTablePhysical;
    Fortress::Core::uint64 HhdmOffset;
    Fortress::Core::uint64 MappedPages;
    Fortress::Core::uint64 TablePages;
};

class FVirtualMemoryManager {
  public:
    static constexpr Fortress::Core::uint64 PageSize = 4096;

    static constexpr Fortress::Core::uint64 FlagPresent = 1ull << 0;
    static constexpr Fortress::Core::uint64 FlagWritable = 1ull << 1;
    static constexpr Fortress::Core::uint64 FlagUser = 1ull << 2;
    static constexpr Fortress::Core::uint64 FlagWriteThrough = 1ull << 3;
    static constexpr Fortress::Core::uint64 FlagCacheDisable = 1ull << 4;
    static constexpr Fortress::Core::uint64 FlagNoExecute = 1ull << 63;

    static constexpr Fortress::Core::uint64 FlagsKernelRW = FlagPresent | FlagWritable;
    static constexpr Fortress::Core::uint64 FlagsKernelRWNX = FlagPresent | FlagWritable | FlagNoExecute;
    static constexpr Fortress::Core::uint64 FlagsKernelRX = FlagPresent;
    static constexpr Fortress::Core::uint64 FlagsDeviceRWUCNX = FlagPresent | FlagWritable | FlagCacheDisable | FlagNoExecute;

    static bool Initialize(Fortress::Core::uint64 hhdmOffset);
    static bool MapPage(Fortress::Core::uint64 virtualAddress, Fortress::Core::uint64 physicalAddress, Fortress::Core::uint64 flags);
    static bool MapPages(Fortress::Core::uint64 virtualBase,
                         Fortress::Core::uint64 physicalBase,
                         Fortress::Core::uint64 pageCount,
                         Fortress::Core::uint64 flags);
    static bool UnmapPage(Fortress::Core::uint64 virtualAddress);
    static Fortress::Core::uint64 Translate(Fortress::Core::uint64 virtualAddress);
    static FVirtualMemoryStats GetStats();

  private:
    static constexpr Fortress::Core::uint64 EntriesPerTable = 512;
    static constexpr Fortress::Core::uint64 AddressMask = 0x000ffffffffff000ull;
    static constexpr Fortress::Core::uint64 HugePageFlag = 1ull << 7;

    static bool GInitialized;
    static Fortress::Core::uint64 GHhdmOffset;
    static Fortress::Core::uint64 GRootTablePhysical;
    static Fortress::Core::uint64 GMappedPages;
    static Fortress::Core::uint64 GTablePages;
};

} // namespace Fortress::Memory

#endif
