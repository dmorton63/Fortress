#include "Fortress/Memory/FDmaMemoryManager.hpp"

#include "Fortress/Memory/FPhysicalMemoryManager.hpp"
#include "Fortress/Memory/FVirtualMemoryManager.hpp"

namespace Fortress::Memory {

struct FFreeRange {
    Fortress::Core::uint64 VirtualAddress;
    Fortress::Core::uint64 PageCount;
};

struct FBufferRecord {
    Fortress::Core::uint64 VirtualAddress;
    Fortress::Core::uint64 PhysicalAddress;
    Fortress::Core::uint64 SizeBytes;
    Fortress::Core::uint64 PageCount;
    Fortress::Core::uint64 AlignmentBytes;
    bool Below4GiB;
    bool InUse;
};

static bool GInitialized = false;
static Fortress::Core::uint64 GRegionBase = 0;
static Fortress::Core::uint64 GRegionPages = 0;
static Fortress::Core::uint64 GReservedPages = 0;
static Fortress::Core::uint64 GNextVirtual = 0;

static FBufferRecord GBuffers[FDmaMemoryManager::MaxBuffers] = {};
static Fortress::Core::uint64 GActiveBuffers = 0;

static FFreeRange GFreeRanges[FDmaMemoryManager::MaxFreeRanges] = {};
static Fortress::Core::uint64 GFreeRangeCount = 0;

static Fortress::Core::uint64 CeilDivU64(Fortress::Core::uint64 value, Fortress::Core::uint64 divisor) {
    return (value + divisor - 1) / divisor;
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
    const Fortress::Core::uint64 bytes = pageCount * FVirtualMemoryManager::PageSize;
    if (GNextVirtual > regionEnd || bytes > (regionEnd - GNextVirtual)) {
        return false;
    }

    outVirtual = GNextVirtual;
    GNextVirtual += bytes;
    return true;
}

static bool ReleaseVirtualRange(Fortress::Core::uint64 virtualAddress, Fortress::Core::uint64 pageCount) {
    if (GFreeRangeCount >= FDmaMemoryManager::MaxFreeRanges) {
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

bool FDmaMemoryManager::Initialize(Fortress::Core::uint64 virtualRegionBase, Fortress::Core::uint64 virtualRegionPages) {
    if ((virtualRegionBase & (FVirtualMemoryManager::PageSize - 1ull)) != 0 || virtualRegionPages == 0) {
        return false;
    }

    GInitialized = true;
    GRegionBase = virtualRegionBase;
    GRegionPages = virtualRegionPages;
    GReservedPages = 0;
    GNextVirtual = virtualRegionBase;
    GActiveBuffers = 0;
    GFreeRangeCount = 0;

    for (Fortress::Core::uint64 i = 0; i < MaxBuffers; i++) {
        GBuffers[i] = FBufferRecord{};
    }

    return true;
}

bool FDmaMemoryManager::AllocateBuffer(Fortress::Core::uint64 sizeBytes,
                                       Fortress::Core::uint64 alignmentBytes,
                                       bool below4GiB,
                                       FDmaBuffer &outBuffer) {
    outBuffer = FDmaBuffer{};

    if (!GInitialized || sizeBytes == 0) {
        return false;
    }

    if (alignmentBytes == 0) {
        alignmentBytes = FVirtualMemoryManager::PageSize;
    }

    if ((alignmentBytes & (alignmentBytes - 1ull)) != 0) {
        return false;
    }

    const Fortress::Core::uint64 pageCount = CeilDivU64(sizeBytes, FVirtualMemoryManager::PageSize);
    Fortress::Core::uint64 alignmentPages = CeilDivU64(alignmentBytes, FVirtualMemoryManager::PageSize);
    if (alignmentPages == 0) {
        alignmentPages = 1;
    }

    Fortress::Core::uint64 physical = 0;
    if (below4GiB) {
        physical = FPhysicalMemoryManager::AllocatePagesBelow(pageCount, alignmentPages, 0xFFFFFFFFull);
    } else {
        physical = FPhysicalMemoryManager::AllocatePages(pageCount, alignmentPages);
    }

    if (physical == 0) {
        return false;
    }

    Fortress::Core::uint64 virtualAddress = 0;
    if (!ReserveVirtualRange(pageCount, virtualAddress)) {
        (void)FPhysicalMemoryManager::FreePages(physical, pageCount);
        return false;
    }

    if (!FVirtualMemoryManager::MapPages(virtualAddress, physical, pageCount, FVirtualMemoryManager::FlagsDeviceRWUCNX)) {
        (void)FPhysicalMemoryManager::FreePages(physical, pageCount);
        (void)ReleaseVirtualRange(virtualAddress, pageCount);
        return false;
    }

    Fortress::Core::uint64 slot = MaxBuffers;
    for (Fortress::Core::uint64 i = 0; i < MaxBuffers; i++) {
        if (!GBuffers[i].InUse) {
            slot = i;
            break;
        }
    }

    if (slot == MaxBuffers) {
        (void)UnmapPagesWithRollback(virtualAddress,
                                     physical,
                                     pageCount,
                                     FVirtualMemoryManager::FlagsDeviceRWUCNX);
        (void)FPhysicalMemoryManager::FreePages(physical, pageCount);
        (void)ReleaseVirtualRange(virtualAddress, pageCount);
        return false;
    }

    GBuffers[slot] = FBufferRecord{
        .VirtualAddress = virtualAddress,
        .PhysicalAddress = physical,
        .SizeBytes = sizeBytes,
        .PageCount = pageCount,
        .AlignmentBytes = alignmentBytes,
        .Below4GiB = below4GiB,
        .InUse = true,
    };

    GActiveBuffers++;
    GReservedPages += pageCount;

    outBuffer = FDmaBuffer{
        .VirtualAddress = virtualAddress,
        .PhysicalAddress = physical,
        .SizeBytes = sizeBytes,
        .PageCount = pageCount,
        .AlignmentBytes = alignmentBytes,
        .Below4GiB = below4GiB,
        .Valid = true,
    };

    return true;
}

bool FDmaMemoryManager::FreeBuffer(FDmaBuffer &buffer) {
    if (!GInitialized || !buffer.Valid) {
        return false;
    }

    Fortress::Core::uint64 slot = MaxBuffers;
    for (Fortress::Core::uint64 i = 0; i < MaxBuffers; i++) {
        if (!GBuffers[i].InUse) {
            continue;
        }

        if (GBuffers[i].VirtualAddress == buffer.VirtualAddress &&
            GBuffers[i].PhysicalAddress == buffer.PhysicalAddress &&
            GBuffers[i].PageCount == buffer.PageCount) {
            slot = i;
            break;
        }
    }

    if (slot == MaxBuffers) {
        return false;
    }

    const FBufferRecord record = GBuffers[slot];
    if (!UnmapPagesWithRollback(record.VirtualAddress,
                                record.PhysicalAddress,
                                record.PageCount,
                                FVirtualMemoryManager::FlagsDeviceRWUCNX)) {
        return false;
    }

    if (!FPhysicalMemoryManager::FreePages(record.PhysicalAddress, record.PageCount)) {
        (void)FVirtualMemoryManager::MapPages(record.VirtualAddress,
                                              record.PhysicalAddress,
                                              record.PageCount,
                                              FVirtualMemoryManager::FlagsDeviceRWUCNX);
        return false;
    }

    (void)ReleaseVirtualRange(record.VirtualAddress, record.PageCount);
    GBuffers[slot] = FBufferRecord{};

    if (GActiveBuffers > 0) {
        GActiveBuffers--;
    }

    if (GReservedPages >= record.PageCount) {
        GReservedPages -= record.PageCount;
    } else {
        GReservedPages = 0;
    }

    buffer = FDmaBuffer{};
    return true;
}

FDmaMemoryStats FDmaMemoryManager::GetStats() {
    return FDmaMemoryStats{
        .RegionBase = GRegionBase,
        .RegionPages = GRegionPages,
        .ActiveBuffers = GActiveBuffers,
        .ReservedPages = GReservedPages,
    };
}

} // namespace Fortress::Memory
