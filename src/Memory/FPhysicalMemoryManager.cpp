#include "Fortress/Memory/FPhysicalMemoryManager.hpp"

#include "Fortress/Memory/FMemoryArena.hpp"
#include "Fortress/Runtime/FRuntime.hpp"

namespace Fortress::Memory {

FPhysicalMemoryManager::FRange FPhysicalMemoryManager::GRanges[FPhysicalMemoryManager::MaxRanges] = {};
Fortress::Core::uint64 FPhysicalMemoryManager::GRangeCount = 0;
Fortress::Core::uint64 FPhysicalMemoryManager::GTotalUsableBytes = 0;
Fortress::Core::uint64 FPhysicalMemoryManager::GTotalPages = 0;
Fortress::Core::uint64 FPhysicalMemoryManager::GFreePages = 0;
Fortress::Core::uint64 FPhysicalMemoryManager::GBitmapBytes = 0;
Fortress::Core::uint64 FPhysicalMemoryManager::GNextRangeHint = 0;

static Fortress::Core::uint64 AlignUp(Fortress::Core::uint64 value, Fortress::Core::uint64 alignment) {
    const Fortress::Core::uint64 mask = alignment - 1;
    return (value + mask) & ~mask;
}

static Fortress::Core::uint64 AlignDown(Fortress::Core::uint64 value, Fortress::Core::uint64 alignment) {
    return value & ~(alignment - 1);
}

static inline bool TestBit(const Fortress::Core::uint8 *bitmap, Fortress::Core::uint64 bit) {
    return (bitmap[bit >> 3] & static_cast<Fortress::Core::uint8>(1u << (bit & 7u))) != 0;
}

static inline void SetBit(Fortress::Core::uint8 *bitmap, Fortress::Core::uint64 bit) {
    bitmap[bit >> 3] |= static_cast<Fortress::Core::uint8>(1u << (bit & 7u));
}

static inline void ClearBit(Fortress::Core::uint8 *bitmap, Fortress::Core::uint64 bit) {
    bitmap[bit >> 3] &= static_cast<Fortress::Core::uint8>(~(1u << (bit & 7u)));
}

static Fortress::Core::uint64 BitmapBytesForPages(Fortress::Core::uint64 pages) {
    return (pages + 7u) / 8u;
}

static Fortress::Core::uint64 MinU64(Fortress::Core::uint64 a, Fortress::Core::uint64 b) {
    return (a < b) ? a : b;
}

static Fortress::Core::uint64 MaxU64(Fortress::Core::uint64 a, Fortress::Core::uint64 b) {
    return (a > b) ? a : b;
}

static bool FindContiguousFree(const Fortress::Core::uint8 *bitmap,
                               Fortress::Core::uint64 totalPages,
                               Fortress::Core::uint64 nextHint,
                               Fortress::Core::uint64 pageCount,
                               Fortress::Core::uint64 alignmentPages,
                               Fortress::Core::uint64 &outStartIndex) {
    if (bitmap == nullptr || pageCount == 0 || alignmentPages == 0 || pageCount > totalPages) {
        return false;
    }

    for (Fortress::Core::uint64 pass = 0; pass < totalPages; pass++) {
        const Fortress::Core::uint64 start = (nextHint + pass) % totalPages;
        if ((start % alignmentPages) != 0) {
            continue;
        }

        if (start + pageCount > totalPages) {
            continue;
        }

        bool allFree = true;
        for (Fortress::Core::uint64 i = 0; i < pageCount; i++) {
            if (TestBit(bitmap, start + i)) {
                allFree = false;
                break;
            }
        }

        if (allFree) {
            outStartIndex = start;
            return true;
        }
    }

    return false;
}

static bool FindContiguousFreeInBoundedRange(const Fortress::Core::uint8 *bitmap,
                                             Fortress::Core::uint64 startPage,
                                             Fortress::Core::uint64 endPageExclusive,
                                             Fortress::Core::uint64 nextHint,
                                             Fortress::Core::uint64 pageCount,
                                             Fortress::Core::uint64 alignmentPages,
                                             Fortress::Core::uint64 &outStartIndex) {
    if (bitmap == nullptr || pageCount == 0 || alignmentPages == 0 || endPageExclusive <= startPage) {
        return false;
    }

    const Fortress::Core::uint64 totalSpan = endPageExclusive - startPage;
    if (pageCount > totalSpan) {
        return false;
    }

    Fortress::Core::uint64 localHint = nextHint;
    if (localHint < startPage || localHint >= endPageExclusive) {
        localHint = startPage;
    }

    const Fortress::Core::uint64 maxStart = endPageExclusive - pageCount;
    Fortress::Core::uint64 cursor = localHint;
    bool wrapped = false;

    while (true) {
        if (cursor <= maxStart && (cursor % alignmentPages) == 0) {
            bool allFree = true;
            for (Fortress::Core::uint64 i = 0; i < pageCount; i++) {
                if (TestBit(bitmap, cursor + i)) {
                    allFree = false;
                    break;
                }
            }

            if (allFree) {
                outStartIndex = cursor;
                return true;
            }
        }

        cursor++;
        if (cursor > maxStart) {
            if (wrapped) {
                break;
            }
            cursor = startPage;
            wrapped = true;
        }

        if (wrapped && cursor >= localHint) {
            break;
        }
    }

    return false;
}

bool FPhysicalMemoryManager::Initialize(const limine_memmap_response *memmapResponse) {
    if (memmapResponse == nullptr || memmapResponse->entries == nullptr || memmapResponse->entry_count == 0) {
        return false;
    }

    GRangeCount = 0;
    GTotalUsableBytes = 0;
    GTotalPages = 0;
    GFreePages = 0;
    GBitmapBytes = 0;
    GNextRangeHint = 0;

    for (Fortress::Core::uint64 i = 0; i < memmapResponse->entry_count; i++) {
        const limine_memmap_entry *entry = memmapResponse->entries[i];
        if (entry == nullptr || entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        Fortress::Core::uint64 start = AlignUp(entry->base, PageSize);
        Fortress::Core::uint64 end = AlignDown(entry->base + entry->length, PageSize);
        if (end <= start) {
            continue;
        }

        if (GRangeCount >= MaxRanges) {
            break;
        }

        const Fortress::Core::uint64 length = end - start;
        const Fortress::Core::uint64 pages = length / PageSize;
        const Fortress::Core::uint64 bitmapBytes = BitmapBytesForPages(pages);

        auto *bitmap = static_cast<Fortress::Core::uint8 *>(
            Fortress::Memory::FMemoryArena::Allocate(bitmapBytes, alignof(Fortress::Core::uint8)));
        if (bitmap == nullptr) {
            return false;
        }

        Fortress::Runtime::Memset(bitmap, 0, bitmapBytes);

        GRanges[GRangeCount] = FRange{
            .Base = start,
            .PageCount = pages,
            .FreePages = pages,
            .NextHint = 0,
            .Bitmap = bitmap,
            .BitmapBytes = bitmapBytes,
        };

        GTotalUsableBytes += length;
        GTotalPages += pages;
        GFreePages += pages;
        GBitmapBytes += bitmapBytes;
        GRangeCount++;
    }

    return GRangeCount > 0;
}

Fortress::Core::uint64 FPhysicalMemoryManager::AllocatePage() {
    return AllocatePages(1, 1);
}

Fortress::Core::uint64 FPhysicalMemoryManager::AllocatePages(Fortress::Core::uint64 pageCount, Fortress::Core::uint64 alignmentPages) {
    if (pageCount == 0 || alignmentPages == 0) {
        return 0;
    }

    if (GRangeCount == 0 || GFreePages == 0) {
        return 0;
    }

    if (pageCount > GFreePages) {
        return 0;
    }

    for (Fortress::Core::uint64 pass = 0; pass < GRangeCount; pass++) {
        const Fortress::Core::uint64 rangeIndex = (GNextRangeHint + pass) % GRangeCount;
        FRange &range = GRanges[rangeIndex];
        if (range.FreePages < pageCount || range.PageCount < pageCount) {
            continue;
        }

        Fortress::Core::uint64 startIndex = 0;
        if (!FindContiguousFree(range.Bitmap, range.PageCount, range.NextHint, pageCount, alignmentPages, startIndex)) {
            continue;
        }

        for (Fortress::Core::uint64 i = 0; i < pageCount; i++) {
            SetBit(range.Bitmap, startIndex + i);
        }

        range.FreePages -= pageCount;
        range.NextHint = (startIndex + pageCount) % range.PageCount;
        GNextRangeHint = rangeIndex;
        GFreePages -= pageCount;

        return range.Base + startIndex * PageSize;
    }

    return 0;
}

Fortress::Core::uint64 FPhysicalMemoryManager::AllocatePagesBelow(Fortress::Core::uint64 pageCount,
                                                                   Fortress::Core::uint64 alignmentPages,
                                                                   Fortress::Core::uint64 maxPhysicalAddressInclusive) {
    if (pageCount == 0 || alignmentPages == 0) {
        return 0;
    }

    if (GRangeCount == 0 || GFreePages == 0 || pageCount > GFreePages) {
        return 0;
    }

    for (Fortress::Core::uint64 pass = 0; pass < GRangeCount; pass++) {
        const Fortress::Core::uint64 rangeIndex = (GNextRangeHint + pass) % GRangeCount;
        FRange &range = GRanges[rangeIndex];
        if (range.FreePages < pageCount || range.PageCount < pageCount) {
            continue;
        }

        const Fortress::Core::uint64 rangeEndPhysical = range.Base + range.PageCount * PageSize;
        if (range.Base > maxPhysicalAddressInclusive) {
            continue;
        }

        const Fortress::Core::uint64 maxEndPhysicalExclusive =
            (maxPhysicalAddressInclusive == ~0ull) ? ~0ull : (maxPhysicalAddressInclusive + 1u);
        Fortress::Core::uint64 boundedEndPhysical = rangeEndPhysical;
        if (maxEndPhysicalExclusive < boundedEndPhysical) {
            boundedEndPhysical = maxEndPhysicalExclusive;
        }

        if (boundedEndPhysical <= range.Base) {
            continue;
        }

        const Fortress::Core::uint64 boundedEndPageExclusive = (boundedEndPhysical - range.Base) / PageSize;
        Fortress::Core::uint64 startIndex = 0;

        if (!FindContiguousFreeInBoundedRange(range.Bitmap,
                                              0,
                                              boundedEndPageExclusive,
                                              range.NextHint,
                                              pageCount,
                                              alignmentPages,
                                              startIndex)) {
            continue;
        }

        for (Fortress::Core::uint64 i = 0; i < pageCount; i++) {
            SetBit(range.Bitmap, startIndex + i);
        }

        range.FreePages -= pageCount;
        range.NextHint = (startIndex + pageCount) % range.PageCount;
        GNextRangeHint = rangeIndex;
        GFreePages -= pageCount;

        return range.Base + startIndex * PageSize;
    }

    return 0;
}

bool FPhysicalMemoryManager::FreePage(Fortress::Core::uint64 physicalAddress) {
    return FreePages(physicalAddress, 1);
}

bool FPhysicalMemoryManager::FreePages(Fortress::Core::uint64 physicalAddress, Fortress::Core::uint64 pageCount) {
    if (pageCount == 0) {
        return false;
    }

    if ((physicalAddress & (PageSize - 1u)) != 0) {
        return false;
    }

    for (Fortress::Core::uint64 i = 0; i < GRangeCount; i++) {
        FRange &range = GRanges[i];
        const Fortress::Core::uint64 end = range.Base + range.PageCount * PageSize;
        if (physicalAddress < range.Base || physicalAddress >= end) {
            continue;
        }

        const Fortress::Core::uint64 pageIndex = (physicalAddress - range.Base) / PageSize;
        if (pageIndex + pageCount > range.PageCount) {
            return false;
        }

        for (Fortress::Core::uint64 p = 0; p < pageCount; p++) {
            if (!TestBit(range.Bitmap, pageIndex + p)) {
                return false;
            }
        }

        for (Fortress::Core::uint64 p = 0; p < pageCount; p++) {
            ClearBit(range.Bitmap, pageIndex + p);
        }

        range.FreePages += pageCount;
        GFreePages += pageCount;
        if (pageIndex < range.NextHint) {
            range.NextHint = pageIndex;
        }
        return true;
    }

    return false;
}

Fortress::Core::uint64 FPhysicalMemoryManager::ReserveRange(Fortress::Core::uint64 physicalBase, Fortress::Core::uint64 lengthBytes) {
    if (lengthBytes == 0) {
        return 0;
    }

    const Fortress::Core::uint64 reserveStart = AlignDown(physicalBase, PageSize);
    const Fortress::Core::uint64 reserveEnd = AlignUp(physicalBase + lengthBytes, PageSize);
    if (reserveEnd <= reserveStart) {
        return 0;
    }

    Fortress::Core::uint64 reservedPages = 0;

    for (Fortress::Core::uint64 i = 0; i < GRangeCount; i++) {
        FRange &range = GRanges[i];
        const Fortress::Core::uint64 rangeStart = range.Base;
        const Fortress::Core::uint64 rangeEnd = range.Base + range.PageCount * PageSize;

        const Fortress::Core::uint64 overlapStart = MaxU64(rangeStart, reserveStart);
        const Fortress::Core::uint64 overlapEnd = MinU64(rangeEnd, reserveEnd);
        if (overlapEnd <= overlapStart) {
            continue;
        }

        const Fortress::Core::uint64 startIndex = (overlapStart - rangeStart) / PageSize;
        const Fortress::Core::uint64 endIndex = (overlapEnd - rangeStart) / PageSize;

        for (Fortress::Core::uint64 page = startIndex; page < endIndex; page++) {
            if (!TestBit(range.Bitmap, page)) {
                SetBit(range.Bitmap, page);
                range.FreePages--;
                if (GFreePages > 0) {
                    GFreePages--;
                }
                reservedPages++;
            }
        }

        if (startIndex <= range.NextHint && range.NextHint < endIndex) {
            range.NextHint = endIndex % range.PageCount;
        }
    }

    return reservedPages;
}

FPhysicalMemoryStats FPhysicalMemoryManager::GetStats() {
    const Fortress::Core::uint64 reserved = (GTotalPages >= GFreePages) ? (GTotalPages - GFreePages) : 0;

    return FPhysicalMemoryStats{
        .TotalUsableBytes = GTotalUsableBytes,
        .TotalPages = GTotalPages,
        .FreePages = GFreePages,
        .ReservedPages = reserved,
        .RangeCount = GRangeCount,
        .BitmapBytes = GBitmapBytes,
    };
}

} // namespace Fortress::Memory
