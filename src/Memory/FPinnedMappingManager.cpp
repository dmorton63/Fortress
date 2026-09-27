#include "Fortress/Memory/FPinnedMappingManager.hpp"

#include "Fortress/Memory/FVirtualMemoryManager.hpp"

namespace Fortress::Memory {

struct FFreeRange {
    Fortress::Core::uint64 VirtualAddress;
    Fortress::Core::uint64 PageCount;
};

struct FMappingRecord {
    Fortress::Core::uint64 VirtualAddress;
    Fortress::Core::uint64 PhysicalAddress;
    Fortress::Core::uint64 SizeBytes;
    Fortress::Core::uint64 PageCount;
    Fortress::Core::uint64 Flags;
    bool InUse;
};

static bool GInitialized = false;
static Fortress::Core::uint64 GRegionBase = 0;
static Fortress::Core::uint64 GRegionPages = 0;
static Fortress::Core::uint64 GReservedPages = 0;

static FMappingRecord GMappings[FPinnedMappingManager::MaxMappings] = {};
static Fortress::Core::uint64 GActiveMappings = 0;

static FFreeRange GFreeRanges[FPinnedMappingManager::MaxFreeRanges] = {};
static Fortress::Core::uint64 GFreeRangeCount = 0;
static Fortress::Core::uint64 GNextVirtual = 0;

static Fortress::Core::uint64 AlignDown(Fortress::Core::uint64 value, Fortress::Core::uint64 alignment) {
    return value & ~(alignment - 1);
}

static Fortress::Core::uint64 AlignUp(Fortress::Core::uint64 value, Fortress::Core::uint64 alignment) {
    const Fortress::Core::uint64 mask = alignment - 1;
    return (value + mask) & ~mask;
}

static bool RangesOverlap(Fortress::Core::uint64 aStart,
                          Fortress::Core::uint64 aEndExclusive,
                          Fortress::Core::uint64 bStart,
                          Fortress::Core::uint64 bEndExclusive) {
    return aStart < bEndExclusive && bStart < aEndExclusive;
}

static bool ReserveVirtualRange(Fortress::Core::uint64 pageCount, Fortress::Core::uint64 &outVirtual) {
    for (Fortress::Core::uint64 i = 0; i < GFreeRangeCount; i++) {
        FFreeRange &range = GFreeRanges[i];
        if (range.PageCount < pageCount) {
            continue;
        }

        outVirtual = range.VirtualAddress;
        range.VirtualAddress += pageCount * FVirtualMemoryManager::PageSize;
        range.PageCount -= pageCount;

        if (range.PageCount == 0) {
            for (Fortress::Core::uint64 j = i; j + 1 < GFreeRangeCount; j++) {
                GFreeRanges[j] = GFreeRanges[j + 1];
            }
            GFreeRangeCount--;
        }
        return true;
    }

    const Fortress::Core::uint64 regionEnd = GRegionBase + GRegionPages * FVirtualMemoryManager::PageSize;
    const Fortress::Core::uint64 sizeBytes = pageCount * FVirtualMemoryManager::PageSize;
    if (GNextVirtual > regionEnd || sizeBytes > (regionEnd - GNextVirtual)) {
        return false;
    }

    outVirtual = GNextVirtual;
    GNextVirtual += sizeBytes;
    return true;
}

static bool ReleaseVirtualRange(Fortress::Core::uint64 virtualAddress, Fortress::Core::uint64 pageCount) {
    if (GFreeRangeCount >= FPinnedMappingManager::MaxFreeRanges) {
        return false;
    }

    Fortress::Core::uint64 insertAt = 0;
    while (insertAt < GFreeRangeCount && GFreeRanges[insertAt].VirtualAddress < virtualAddress) {
        insertAt++;
    }

    for (Fortress::Core::uint64 i = GFreeRangeCount; i > insertAt; i--) {
        GFreeRanges[i] = GFreeRanges[i - 1];
    }

    GFreeRanges[insertAt] = FFreeRange{.VirtualAddress = virtualAddress, .PageCount = pageCount};
    GFreeRangeCount++;

    if (insertAt > 0) {
        FFreeRange &prev = GFreeRanges[insertAt - 1];
        FFreeRange &curr = GFreeRanges[insertAt];
        const Fortress::Core::uint64 prevEnd = prev.VirtualAddress + prev.PageCount * FVirtualMemoryManager::PageSize;
        if (prevEnd == curr.VirtualAddress) {
            prev.PageCount += curr.PageCount;
            for (Fortress::Core::uint64 i = insertAt; i + 1 < GFreeRangeCount; i++) {
                GFreeRanges[i] = GFreeRanges[i + 1];
            }
            GFreeRangeCount--;
            insertAt--;
        }
    }

    if (insertAt + 1 < GFreeRangeCount) {
        FFreeRange &curr = GFreeRanges[insertAt];
        FFreeRange &next = GFreeRanges[insertAt + 1];
        const Fortress::Core::uint64 currEnd = curr.VirtualAddress + curr.PageCount * FVirtualMemoryManager::PageSize;
        if (currEnd == next.VirtualAddress) {
            curr.PageCount += next.PageCount;
            for (Fortress::Core::uint64 i = insertAt + 1; i + 1 < GFreeRangeCount; i++) {
                GFreeRanges[i] = GFreeRanges[i + 1];
            }
            GFreeRangeCount--;
        }
    }

    return true;
}

static bool UnmapPagesWithRollback(Fortress::Core::uint64 virtualAddress,
                                   Fortress::Core::uint64 physicalAddress,
                                   Fortress::Core::uint64 pageCount,
                                   Fortress::Core::uint64 flags) {
    Fortress::Core::uint64 unmapped = 0;
    for (Fortress::Core::uint64 i = 0; i < pageCount; i++) {
        if (!FVirtualMemoryManager::UnmapPage(virtualAddress + i * FVirtualMemoryManager::PageSize)) {
            for (Fortress::Core::uint64 r = 0; r < unmapped; r++) {
                (void)FVirtualMemoryManager::MapPage(virtualAddress + r * FVirtualMemoryManager::PageSize,
                                                     physicalAddress + r * FVirtualMemoryManager::PageSize,
                                                     flags);
            }
            return false;
        }
        unmapped++;
    }
    return true;
}

bool FPinnedMappingManager::Initialize(Fortress::Core::uint64 virtualRegionBase, Fortress::Core::uint64 virtualRegionPages) {
    if ((virtualRegionBase & (FVirtualMemoryManager::PageSize - 1ull)) != 0 || virtualRegionPages == 0) {
        return false;
    }

    GInitialized = true;
    GRegionBase = virtualRegionBase;
    GRegionPages = virtualRegionPages;
    GReservedPages = 0;
    GActiveMappings = 0;
    GFreeRangeCount = 0;
    GNextVirtual = virtualRegionBase;

    for (Fortress::Core::uint64 i = 0; i < MaxMappings; i++) {
        GMappings[i] = FMappingRecord{};
    }

    return true;
}

bool FPinnedMappingManager::MapPhysicalRange(Fortress::Core::uint64 physicalAddress,
                                             Fortress::Core::uint64 sizeBytes,
                                             Fortress::Core::uint64 flags,
                                             FPinnedMapping &outMapping) {
    outMapping = FPinnedMapping{};

    if (!GInitialized || sizeBytes == 0) {
        return false;
    }

    const Fortress::Core::uint64 mapStart = AlignDown(physicalAddress, FVirtualMemoryManager::PageSize);
    const Fortress::Core::uint64 mapEnd = AlignUp(physicalAddress + sizeBytes, FVirtualMemoryManager::PageSize);
    if (mapEnd <= mapStart) {
        return false;
    }

    for (Fortress::Core::uint64 i = 0; i < MaxMappings; i++) {
        if (!GMappings[i].InUse) {
            continue;
        }

        const Fortress::Core::uint64 inUseStart = GMappings[i].PhysicalAddress;
        const Fortress::Core::uint64 inUseEnd = inUseStart + GMappings[i].PageCount * FVirtualMemoryManager::PageSize;
        if (RangesOverlap(mapStart, mapEnd, inUseStart, inUseEnd)) {
            return false;
        }
    }

    const Fortress::Core::uint64 pageCount = (mapEnd - mapStart) / FVirtualMemoryManager::PageSize;
    Fortress::Core::uint64 virtualBase = 0;
    if (!ReserveVirtualRange(pageCount, virtualBase)) {
        return false;
    }

    if (!FVirtualMemoryManager::MapPages(virtualBase, mapStart, pageCount, flags)) {
        (void)ReleaseVirtualRange(virtualBase, pageCount);
        return false;
    }

    Fortress::Core::uint64 slot = MaxMappings;
    for (Fortress::Core::uint64 i = 0; i < MaxMappings; i++) {
        if (!GMappings[i].InUse) {
            slot = i;
            break;
        }
    }

    if (slot == MaxMappings) {
        (void)UnmapPagesWithRollback(virtualBase, mapStart, pageCount, flags);
        (void)ReleaseVirtualRange(virtualBase, pageCount);
        return false;
    }

    GMappings[slot] = FMappingRecord{
        .VirtualAddress = virtualBase,
        .PhysicalAddress = mapStart,
        .SizeBytes = mapEnd - mapStart,
        .PageCount = pageCount,
        .Flags = flags,
        .InUse = true,
    };

    GActiveMappings++;
    GReservedPages += pageCount;

    outMapping = FPinnedMapping{
        .VirtualAddress = virtualBase + (physicalAddress - mapStart),
        .PhysicalAddress = physicalAddress,
        .SizeBytes = sizeBytes,
        .PageCount = pageCount,
        .Flags = flags,
        .Valid = true,
    };

    return true;
}

bool FPinnedMappingManager::UnmapPhysicalRange(FPinnedMapping &mapping) {
    if (!GInitialized || !mapping.Valid) {
        return false;
    }

    const Fortress::Core::uint64 mapStart = AlignDown(mapping.PhysicalAddress, FVirtualMemoryManager::PageSize);
    const Fortress::Core::uint64 mapEnd = AlignUp(mapping.PhysicalAddress + mapping.SizeBytes, FVirtualMemoryManager::PageSize);
    if (mapEnd <= mapStart) {
        return false;
    }

    const Fortress::Core::uint64 pageCount = (mapEnd - mapStart) / FVirtualMemoryManager::PageSize;
    Fortress::Core::uint64 recordIndex = MaxMappings;
    for (Fortress::Core::uint64 i = 0; i < MaxMappings; i++) {
        if (!GMappings[i].InUse) {
            continue;
        }

        const Fortress::Core::uint64 recordVirtualAddress = GMappings[i].VirtualAddress + (mapping.PhysicalAddress - mapStart);
        if (GMappings[i].PhysicalAddress == mapStart && GMappings[i].PageCount == pageCount &&
            GMappings[i].Flags == mapping.Flags && recordVirtualAddress == mapping.VirtualAddress) {
            recordIndex = i;
            break;
        }
    }

    if (recordIndex == MaxMappings) {
        return false;
    }

    FMappingRecord record = GMappings[recordIndex];
    if (!UnmapPagesWithRollback(record.VirtualAddress, record.PhysicalAddress, record.PageCount, record.Flags)) {
        return false;
    }

    (void)ReleaseVirtualRange(record.VirtualAddress, record.PageCount);
    GMappings[recordIndex] = FMappingRecord{};

    if (GActiveMappings > 0) {
        GActiveMappings--;
    }

    if (GReservedPages >= record.PageCount) {
        GReservedPages -= record.PageCount;
    } else {
        GReservedPages = 0;
    }

    mapping = FPinnedMapping{};
    return true;
}

FPinnedMappingStats FPinnedMappingManager::GetStats() {
    return FPinnedMappingStats{
        .RegionBase = GRegionBase,
        .RegionPages = GRegionPages,
        .ActiveMappings = GActiveMappings,
        .ReservedPages = GReservedPages,
    };
}

} // namespace Fortress::Memory
