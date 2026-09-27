#include "Fortress/Memory/FVirtualMemoryManager.hpp"

#include "Fortress/Memory/FPhysicalMemoryManager.hpp"
#include "Fortress/Runtime/FRuntime.hpp"

namespace Fortress::Memory {

bool FVirtualMemoryManager::GInitialized = false;
Fortress::Core::uint64 FVirtualMemoryManager::GHhdmOffset = 0;
Fortress::Core::uint64 FVirtualMemoryManager::GRootTablePhysical = 0;
Fortress::Core::uint64 FVirtualMemoryManager::GMappedPages = 0;
Fortress::Core::uint64 FVirtualMemoryManager::GTablePages = 0;

static inline Fortress::Core::uint64 ReadCR3() {
    Fortress::Core::uint64 value;
    __asm__ volatile("mov %%cr3, %0" : "=r"(value));
    return value;
}

static inline void InvalidatePage(Fortress::Core::uint64 virtualAddress) {
    __asm__ volatile("invlpg (%0)" : : "r"(virtualAddress) : "memory");
}

static inline Fortress::Core::uint64 *PhysToVirt(Fortress::Core::uint64 physicalAddress, Fortress::Core::uint64 hhdmOffset) {
    return reinterpret_cast<Fortress::Core::uint64 *>(physicalAddress + hhdmOffset);
}

static Fortress::Core::uint64 PageTableIndex(Fortress::Core::uint64 virtualAddress, Fortress::Core::uint64 shift) {
    return (virtualAddress >> shift) & 0x1ffull;
}

static Fortress::Core::uint64 AllocatePageTable(Fortress::Core::uint64 hhdmOffset, Fortress::Core::uint64 &tablePagesCounter) {
    const Fortress::Core::uint64 tablePhysical = FPhysicalMemoryManager::AllocatePage();
    if (tablePhysical == 0) {
        return 0;
    }

    auto *tableVirtual = PhysToVirt(tablePhysical, hhdmOffset);
    Fortress::Runtime::Memset(tableVirtual, 0, FVirtualMemoryManager::PageSize);
    tablePagesCounter++;
    return tablePhysical;
}

bool FVirtualMemoryManager::Initialize(Fortress::Core::uint64 hhdmOffset) {
    GHhdmOffset = hhdmOffset;
    GRootTablePhysical = ReadCR3() & AddressMask;
    GMappedPages = 0;
    GTablePages = 0;
    GInitialized = (GRootTablePhysical != 0);
    return GInitialized;
}

bool FVirtualMemoryManager::MapPage(Fortress::Core::uint64 virtualAddress,
                                    Fortress::Core::uint64 physicalAddress,
                                    Fortress::Core::uint64 flags) {
    if (!GInitialized) {
        return false;
    }

    if ((virtualAddress & (PageSize - 1ull)) != 0 || (physicalAddress & (PageSize - 1ull)) != 0) {
        return false;
    }

    Fortress::Core::uint64 *table = PhysToVirt(GRootTablePhysical, GHhdmOffset);
    const Fortress::Core::uint64 indices[4] = {
        PageTableIndex(virtualAddress, 39),
        PageTableIndex(virtualAddress, 30),
        PageTableIndex(virtualAddress, 21),
        PageTableIndex(virtualAddress, 12),
    };

    for (Fortress::Core::uint64 level = 0; level < 3; level++) {
        Fortress::Core::uint64 &entry = table[indices[level]];
        if ((entry & FlagPresent) == 0) {
            const Fortress::Core::uint64 newTable = AllocatePageTable(GHhdmOffset, GTablePages);
            if (newTable == 0) {
                return false;
            }
            entry = (newTable & AddressMask) | FlagPresent | FlagWritable;
        } else if ((entry & HugePageFlag) != 0) {
            return false;
        }

        table = PhysToVirt(entry & AddressMask, GHhdmOffset);
    }

    Fortress::Core::uint64 &pte = table[indices[3]];
    if ((pte & FlagPresent) != 0) {
        return false;
    }

    pte = (physicalAddress & AddressMask) | (flags | FlagPresent);
    GMappedPages++;
    InvalidatePage(virtualAddress);
    return true;
}

bool FVirtualMemoryManager::MapPages(Fortress::Core::uint64 virtualBase,
                                     Fortress::Core::uint64 physicalBase,
                                     Fortress::Core::uint64 pageCount,
                                     Fortress::Core::uint64 flags) {
    if (pageCount == 0) {
        return false;
    }

    Fortress::Core::uint64 mappedCount = 0;
    for (Fortress::Core::uint64 i = 0; i < pageCount; i++) {
        const Fortress::Core::uint64 virtualAddress = virtualBase + i * PageSize;
        const Fortress::Core::uint64 physicalAddress = physicalBase + i * PageSize;
        if (!MapPage(virtualAddress, physicalAddress, flags)) {
            for (Fortress::Core::uint64 rollback = 0; rollback < mappedCount; rollback++) {
                (void)UnmapPage(virtualBase + rollback * PageSize);
            }
            return false;
        }
        mappedCount++;
    }

    return true;
}

bool FVirtualMemoryManager::UnmapPage(Fortress::Core::uint64 virtualAddress) {
    if (!GInitialized) {
        return false;
    }

    if ((virtualAddress & (PageSize - 1ull)) != 0) {
        return false;
    }

    Fortress::Core::uint64 *table = PhysToVirt(GRootTablePhysical, GHhdmOffset);
    const Fortress::Core::uint64 indices[4] = {
        PageTableIndex(virtualAddress, 39),
        PageTableIndex(virtualAddress, 30),
        PageTableIndex(virtualAddress, 21),
        PageTableIndex(virtualAddress, 12),
    };

    for (Fortress::Core::uint64 level = 0; level < 3; level++) {
        const Fortress::Core::uint64 entry = table[indices[level]];
        if ((entry & FlagPresent) == 0 || (entry & HugePageFlag) != 0) {
            return false;
        }
        table = PhysToVirt(entry & AddressMask, GHhdmOffset);
    }

    Fortress::Core::uint64 &pte = table[indices[3]];
    if ((pte & FlagPresent) == 0) {
        return false;
    }

    pte = 0;
    if (GMappedPages > 0) {
        GMappedPages--;
    }
    InvalidatePage(virtualAddress);
    return true;
}

Fortress::Core::uint64 FVirtualMemoryManager::Translate(Fortress::Core::uint64 virtualAddress) {
    if (!GInitialized) {
        return 0;
    }

    Fortress::Core::uint64 *table = PhysToVirt(GRootTablePhysical, GHhdmOffset);
    const Fortress::Core::uint64 pml4Index = PageTableIndex(virtualAddress, 39);
    const Fortress::Core::uint64 pdptIndex = PageTableIndex(virtualAddress, 30);
    const Fortress::Core::uint64 pdIndex = PageTableIndex(virtualAddress, 21);
    const Fortress::Core::uint64 ptIndex = PageTableIndex(virtualAddress, 12);

    const Fortress::Core::uint64 pml4e = table[pml4Index];
    if ((pml4e & FlagPresent) == 0) {
        return 0;
    }

    table = PhysToVirt(pml4e & AddressMask, GHhdmOffset);
    const Fortress::Core::uint64 pdpte = table[pdptIndex];
    if ((pdpte & FlagPresent) == 0) {
        return 0;
    }

    if ((pdpte & HugePageFlag) != 0) {
        const Fortress::Core::uint64 base = pdpte & 0x000fffffc0000000ull;
        const Fortress::Core::uint64 offset = virtualAddress & 0x3fffffffull;
        return base + offset;
    }

    table = PhysToVirt(pdpte & AddressMask, GHhdmOffset);
    const Fortress::Core::uint64 pde = table[pdIndex];
    if ((pde & FlagPresent) == 0) {
        return 0;
    }

    if ((pde & HugePageFlag) != 0) {
        const Fortress::Core::uint64 base = pde & 0x000fffffffe00000ull;
        const Fortress::Core::uint64 offset = virtualAddress & 0x1fffffull;
        return base + offset;
    }

    table = PhysToVirt(pde & AddressMask, GHhdmOffset);
    const Fortress::Core::uint64 pte = table[ptIndex];
    if ((pte & FlagPresent) == 0) {
        return 0;
    }

    const Fortress::Core::uint64 pageBase = pte & AddressMask;
    return pageBase + (virtualAddress & (PageSize - 1ull));
}

FVirtualMemoryStats FVirtualMemoryManager::GetStats() {
    return FVirtualMemoryStats{
        .RootTablePhysical = GRootTablePhysical,
        .HhdmOffset = GHhdmOffset,
        .MappedPages = GMappedPages,
        .TablePages = GTablePages,
    };
}

} // namespace Fortress::Memory
