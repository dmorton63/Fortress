#include "Fortress/Memory/FKernelHeap.hpp"

#include "Fortress/Kernel/FKernelConfig.hpp"
#include "Fortress/Memory/FPhysicalMemoryManager.hpp"
#include "Fortress/Memory/FVirtualMemoryManager.hpp"

namespace Fortress::Memory {

bool FKernelHeap::GInitialized = false;
Fortress::Core::uint64 FKernelHeap::GBaseVirtualAddress = 0;
Fortress::Core::uint64 FKernelHeap::GCapacityBytes = 0;
Fortress::Core::uint64 FKernelHeap::GUsedBytes = 0;
Fortress::Core::uint64 FKernelHeap::GMapFlags = 0;
FKernelHeap::FSegment FKernelHeap::GSegments[FKernelHeap::MaxSegments] = {};
Fortress::Core::uint64 FKernelHeap::GSegmentCount = 0;

static Fortress::Core::uint64 AlignUp(Fortress::Core::uint64 value, Fortress::Core::uint64 alignment) {
    if (alignment == 0) {
        return value;
    }
    const Fortress::Core::uint64 mask = alignment - 1;
    return (value + mask) & ~mask;
}

static Fortress::Core::uint64 CeilDivU64(Fortress::Core::uint64 value, Fortress::Core::uint64 divisor) {
    return (value + divisor - 1) / divisor;
}

bool FKernelHeap::InsertSegment(Fortress::Core::uint64 index, const FSegment &segment) {
    if (GSegmentCount >= MaxSegments || index > GSegmentCount) {
        return false;
    }

    for (Fortress::Core::uint64 i = GSegmentCount; i > index; i--) {
        GSegments[i] = GSegments[i - 1];
    }

    GSegments[index] = segment;
    GSegmentCount++;
    return true;
}

void FKernelHeap::MergeAround(Fortress::Core::uint64 index) {
    if (index >= GSegmentCount) {
        return;
    }

    while (index > 0) {
        FSegment &curr = GSegments[index];
        FSegment &prev = GSegments[index - 1];
        if (!curr.Free || !prev.Free) {
            break;
        }

        if (prev.Offset + prev.SizeBytes != curr.Offset) {
            break;
        }

        prev.SizeBytes += curr.SizeBytes;
        for (Fortress::Core::uint64 i = index; i + 1 < GSegmentCount; i++) {
            GSegments[i] = GSegments[i + 1];
        }
        GSegmentCount--;
        index--;
    }

    while (index + 1 < GSegmentCount) {
        FSegment &curr = GSegments[index];
        FSegment &next = GSegments[index + 1];
        if (!curr.Free || !next.Free) {
            break;
        }

        if (curr.Offset + curr.SizeBytes != next.Offset) {
            break;
        }

        curr.SizeBytes += next.SizeBytes;
        for (Fortress::Core::uint64 i = index + 1; i + 1 < GSegmentCount; i++) {
            GSegments[i] = GSegments[i + 1];
        }
        GSegmentCount--;
    }
}

bool FKernelHeap::GrowToFit(Fortress::Core::uint64 requiredBytes) {
    if (requiredBytes == 0 || !GInitialized) {
        return false;
    }

    const Fortress::Core::uint64 minPages = CeilDivU64(requiredBytes, FVirtualMemoryManager::PageSize);
    Fortress::Core::uint64 growPages = Fortress::Kernel::FKernelConfig::KernelHeapGrowPages;
    if (growPages < minPages) {
        growPages = minPages;
    }

    const Fortress::Core::uint64 growBytes = growPages * FVirtualMemoryManager::PageSize;
    const Fortress::Core::uint64 growOffset = GCapacityBytes;
    const Fortress::Core::uint64 growVirtual = GBaseVirtualAddress + growOffset;

    const Fortress::Core::uint64 physicalBase = FPhysicalMemoryManager::AllocatePages(growPages, 1);
    if (physicalBase == 0) {
        return false;
    }

    if (!FVirtualMemoryManager::MapPages(growVirtual, physicalBase, growPages, GMapFlags)) {
        (void)FPhysicalMemoryManager::FreePages(physicalBase, growPages);
        return false;
    }

    GCapacityBytes += growBytes;

    if (GSegmentCount > 0) {
        FSegment &tail = GSegments[GSegmentCount - 1];
        if (tail.Free && tail.Offset + tail.SizeBytes == growOffset) {
            tail.SizeBytes += growBytes;
            return true;
        }
    }

    return InsertSegment(GSegmentCount, FSegment{.Offset = growOffset, .SizeBytes = growBytes, .Free = true});
}

bool FKernelHeap::Initialize(Fortress::Core::uint64 baseVirtualAddress,
                             Fortress::Core::uint64 initialPages,
                             Fortress::Core::uint64 mapFlags) {
    if (initialPages == 0 || (baseVirtualAddress & (FVirtualMemoryManager::PageSize - 1ull)) != 0) {
        return false;
    }

    const Fortress::Core::uint64 physicalBase = FPhysicalMemoryManager::AllocatePages(initialPages, 1);
    if (physicalBase == 0) {
        return false;
    }

    if (!FVirtualMemoryManager::MapPages(baseVirtualAddress, physicalBase, initialPages, mapFlags)) {
        (void)FPhysicalMemoryManager::FreePages(physicalBase, initialPages);
        return false;
    }

    GBaseVirtualAddress = baseVirtualAddress;
    GCapacityBytes = initialPages * FVirtualMemoryManager::PageSize;
    GUsedBytes = 0;
    GMapFlags = mapFlags;
    GSegmentCount = 1;
    GSegments[0] = FSegment{.Offset = 0, .SizeBytes = GCapacityBytes, .Free = true};
    GInitialized = true;
    return true;
}

void *FKernelHeap::Allocate(Fortress::Core::uint64 sizeBytes, Fortress::Core::uint64 alignment) {
    if (!GInitialized || sizeBytes == 0) {
        return nullptr;
    }

    if (alignment == 0) {
        alignment = 1;
    }

    if ((alignment & (alignment - 1u)) != 0) {
        return nullptr;
    }

    for (int attempt = 0; attempt < 2; attempt++) {
        for (Fortress::Core::uint64 i = 0; i < GSegmentCount; i++) {
            FSegment &segment = GSegments[i];
            if (!segment.Free) {
                continue;
            }

            const Fortress::Core::uint64 segmentAddress = GBaseVirtualAddress + segment.Offset;
            const Fortress::Core::uint64 alignedAddress = AlignUp(segmentAddress, alignment);
            const Fortress::Core::uint64 leadBytes = alignedAddress - segmentAddress;
            if (segment.SizeBytes < leadBytes || (segment.SizeBytes - leadBytes) < sizeBytes) {
                continue;
            }

            const Fortress::Core::uint64 trailBytes = segment.SizeBytes - leadBytes - sizeBytes;

            if (leadBytes > 0) {
                const FSegment lead = FSegment{.Offset = segment.Offset, .SizeBytes = leadBytes, .Free = true};
                segment.Offset += leadBytes;
                segment.SizeBytes -= leadBytes;
                if (!InsertSegment(i, lead)) {
                    return nullptr;
                }
                i++;
            }

            segment.Free = false;
            segment.SizeBytes = sizeBytes;

            if (trailBytes > 0) {
                const FSegment trail = FSegment{
                    .Offset = segment.Offset + sizeBytes,
                    .SizeBytes = trailBytes,
                    .Free = true,
                };
                if (!InsertSegment(i + 1, trail)) {
                    segment.Free = true;
                    MergeAround(i);
                    return nullptr;
                }
            }

            GUsedBytes += sizeBytes;
            return reinterpret_cast<void *>(GBaseVirtualAddress + segment.Offset);
        }

        if (!GrowToFit(sizeBytes + alignment)) {
            break;
        }
    }

    return nullptr;
}

bool FKernelHeap::Free(void *address) {
    if (!GInitialized || address == nullptr) {
        return false;
    }

    const Fortress::Core::uint64 value = reinterpret_cast<Fortress::Core::uint64>(address);
    if (value < GBaseVirtualAddress || value >= (GBaseVirtualAddress + GCapacityBytes)) {
        return false;
    }

    const Fortress::Core::uint64 offset = value - GBaseVirtualAddress;
    for (Fortress::Core::uint64 i = 0; i < GSegmentCount; i++) {
        FSegment &segment = GSegments[i];
        if (segment.Offset != offset) {
            continue;
        }

        if (segment.Free) {
            return false;
        }

        segment.Free = true;
        if (GUsedBytes >= segment.SizeBytes) {
            GUsedBytes -= segment.SizeBytes;
        } else {
            GUsedBytes = 0;
        }
        MergeAround(i);
        return true;
    }

    return false;
}

FKernelHeapStats FKernelHeap::GetStats() {
    const Fortress::Core::uint64 freeBytes = (GUsedBytes <= GCapacityBytes) ? (GCapacityBytes - GUsedBytes) : 0;
    return FKernelHeapStats{
        .BaseVirtualAddress = GBaseVirtualAddress,
        .CapacityBytes = GCapacityBytes,
        .UsedBytes = GUsedBytes,
        .FreeBytes = freeBytes,
    };
}

} // namespace Fortress::Memory
