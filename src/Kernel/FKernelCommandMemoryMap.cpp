#include "Fortress/Kernel/FKernelCommandMemoryMap.hpp"

#include "Fortress/Memory/FDmaMemoryManager.hpp"
#include "Fortress/Memory/FPinnedMappingManager.hpp"
#include "Fortress/Memory/FPhysicalMemoryManager.hpp"
#include "Fortress/Memory/FVirtualMemoryManager.hpp"

namespace Fortress::Kernel {

using Fortress::Memory::FDmaBuffer;
using Fortress::Memory::FDmaMemoryManager;
using Fortress::Memory::FPinnedMapping;
using Fortress::Memory::FPinnedMappingManager;
using Fortress::Memory::FPhysicalMemoryManager;
using Fortress::Memory::FVirtualMemoryManager;

struct FAllocBlock {
    uint64_t Address;
    uint64_t PageCount;
};

struct FVirtualAllocBlock {
    uint64_t VirtualAddress;
    uint64_t PhysicalAddress;
    uint64_t PageCount;
    uint64_t Flags;
    bool InUse;
};

struct FDmaAllocRecord {
    FDmaBuffer Buffer;
    bool InUse;
};

struct FPinnedAllocRecord {
    FPinnedMapping Mapping;
    bool InUse;
};

static FAllocBlock GAllocatedBlocks[32] = {};
static size_t GAllocatedBlockCount = 0;

static FVirtualAllocBlock GVirtualAllocBlocks[64] = {};
static size_t GVirtualAllocBlockCount = 0;

static FDmaAllocRecord GDmaAllocRecords[64] = {};
static size_t GDmaAllocRecordCount = 0;

static FPinnedAllocRecord GPinnedAllocRecords[64] = {};
static size_t GPinnedAllocRecordCount = 0;

static bool StrEq(const char *left, const char *right) {
    if (left == nullptr || right == nullptr) {
        return false;
    }

    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return false;
        }
        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

static bool StartsWith(const char *value, const char *prefix) {
    if (value == nullptr || prefix == nullptr) {
        return false;
    }

    while (*prefix != '\0') {
        if (*value == '\0' || *value != *prefix) {
            return false;
        }
        value++;
        prefix++;
    }

    return true;
}

static bool ParseUInt(const char *value, uint64_t &out) {
    if (value == nullptr || *value == '\0') {
        return false;
    }

    uint64_t result = 0;
    for (const char *cursor = value; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
        result = result * 10 + static_cast<uint64_t>(*cursor - '0');
    }

    out = result;
    return true;
}

static bool ParseU64Auto(const char *value, uint64_t &out) {
    if (value == nullptr || *value == '\0') {
        return false;
    }

    uint64_t result = 0;
    if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        value += 2;
        if (*value == '\0') {
            return false;
        }
        while (*value != '\0') {
            const char c = *value;
            uint8_t digit = 0;
            if (c >= '0' && c <= '9') {
                digit = static_cast<uint8_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                digit = static_cast<uint8_t>(10 + c - 'a');
            } else if (c >= 'A' && c <= 'F') {
                digit = static_cast<uint8_t>(10 + c - 'A');
            } else {
                return false;
            }
            result = (result << 4) | static_cast<uint64_t>(digit);
            value++;
        }
        out = result;
        return true;
    }

    return ParseUInt(value, out);
}

static bool ReadToken(const char *&cursor, char *out, size_t outSize) {
    if (cursor == nullptr || out == nullptr || outSize < 2) {
        return false;
    }

    while (*cursor == ' ') {
        ++cursor;
    }

    if (*cursor == '\0') {
        out[0] = '\0';
        return false;
    }

    size_t index = 0;
    while (*cursor != '\0' && *cursor != ' ') {
        if (index + 1 >= outSize) {
            return false;
        }
        out[index++] = *cursor++;
    }

    out[index] = '\0';
    while (*cursor == ' ') {
        ++cursor;
    }

    return index > 0;
}

static void AppendString(char *dst, size_t dstSize, size_t &offset, const char *src) {
    if (dst == nullptr || src == nullptr || offset >= dstSize) {
        return;
    }

    while (*src != '\0' && offset + 1 < dstSize) {
        dst[offset++] = *src++;
    }

    if (offset < dstSize) {
        dst[offset] = '\0';
    } else {
        dst[dstSize - 1] = '\0';
    }
}

static void AppendUInt(char *dst, size_t dstSize, size_t &offset, uint64_t value) {
    char digits[32] = {};
    size_t count = 0;
    do {
        digits[count++] = static_cast<char>('0' + (value % 10ull));
        value /= 10ull;
    } while (value != 0 && count < sizeof(digits));

    while (count > 0) {
        const char c = digits[--count];
        if (offset + 1 >= dstSize) {
            break;
        }
        dst[offset++] = c;
    }

    if (offset < dstSize) {
        dst[offset] = '\0';
    }
}

static void AppendHex(char *dst, size_t dstSize, size_t &offset, uint64_t value) {
    static const char *kHex = "0123456789ABCDEF";
    char digits[16] = {};
    size_t count = 0;

    do {
        digits[count++] = kHex[value & 0xFull];
        value >>= 4;
    } while (value != 0 && count < sizeof(digits));

    AppendString(dst, dstSize, offset, "0x");
    while (count > 0) {
        const char c = digits[--count];
        if (offset + 1 >= dstSize) {
            break;
        }
        dst[offset++] = c;
    }

    if (offset < dstSize) {
        dst[offset] = '\0';
    }
}

static bool ParseVmMapMode(const char *token, uint64_t &outFlags) {
    if (StrEq(token, "rw")) {
        outFlags = FVirtualMemoryManager::FlagsKernelRW;
        return true;
    }
    if (StrEq(token, "rwnx")) {
        outFlags = FVirtualMemoryManager::FlagsKernelRWNX;
        return true;
    }
    if (StrEq(token, "rx")) {
        outFlags = FVirtualMemoryManager::FlagsKernelRX;
        return true;
    }
    if (StrEq(token, "dev")) {
        outFlags = FVirtualMemoryManager::FlagsDeviceRWUCNX;
        return true;
    }
    return false;
}

static int64_t FindVmallocBlockByVA(uint64_t virtualAddress) {
    for (size_t i = 0; i < GVirtualAllocBlockCount; i++) {
        if (GVirtualAllocBlocks[i].InUse && GVirtualAllocBlocks[i].VirtualAddress == virtualAddress) {
            return static_cast<int64_t>(i);
        }
    }
    return -1;
}

static int64_t FindLatestVmallocBlock() {
    int64_t found = -1;
    uint64_t bestVA = 0;
    for (size_t i = 0; i < GVirtualAllocBlockCount; i++) {
        if (!GVirtualAllocBlocks[i].InUse) {
            continue;
        }

        if (found < 0 || GVirtualAllocBlocks[i].VirtualAddress > bestVA) {
            bestVA = GVirtualAllocBlocks[i].VirtualAddress;
            found = static_cast<int64_t>(i);
        }
    }

    return found;
}

static int64_t FindDmaRecordByVirtualAddress(uint64_t virtualAddress) {
    for (size_t i = 0; i < GDmaAllocRecordCount; i++) {
        if (GDmaAllocRecords[i].InUse && GDmaAllocRecords[i].Buffer.VirtualAddress == virtualAddress) {
            return static_cast<int64_t>(i);
        }
    }
    return -1;
}

static int64_t FindLatestDmaRecord() {
    int64_t found = -1;
    uint64_t bestVA = 0;
    for (size_t i = 0; i < GDmaAllocRecordCount; i++) {
        if (!GDmaAllocRecords[i].InUse) {
            continue;
        }

        if (found < 0 || GDmaAllocRecords[i].Buffer.VirtualAddress > bestVA) {
            bestVA = GDmaAllocRecords[i].Buffer.VirtualAddress;
            found = static_cast<int64_t>(i);
        }
    }
    return found;
}

static int64_t FindPinnedRecordByVirtualAddress(uint64_t virtualAddress) {
    for (size_t i = 0; i < GPinnedAllocRecordCount; i++) {
        if (GPinnedAllocRecords[i].InUse && GPinnedAllocRecords[i].Mapping.VirtualAddress == virtualAddress) {
            return static_cast<int64_t>(i);
        }
    }
    return -1;
}

static int64_t FindLatestPinnedRecord() {
    int64_t found = -1;
    uint64_t bestVA = 0;
    for (size_t i = 0; i < GPinnedAllocRecordCount; i++) {
        if (!GPinnedAllocRecords[i].InUse) {
            continue;
        }

        if (found < 0 || GPinnedAllocRecords[i].Mapping.VirtualAddress > bestVA) {
            bestVA = GPinnedAllocRecords[i].Mapping.VirtualAddress;
            found = static_cast<int64_t>(i);
        }
    }
    return found;
}

static void ClearCommandInput(FKernelMemoryMapCommandContext &context) {
    if (context.CommandLength != nullptr) {
        *context.CommandLength = 0;
    }
    if (context.CommandBuffer != nullptr) {
        context.CommandBuffer[0] = '\0';
    }
}

void ResetKernelCommandMemoryMapState() {
    GAllocatedBlockCount = 0;
    GVirtualAllocBlockCount = 0;
    GDmaAllocRecordCount = 0;
    GPinnedAllocRecordCount = 0;
    for (size_t i = 0; i < (sizeof(GAllocatedBlocks) / sizeof(GAllocatedBlocks[0])); i++) {
        GAllocatedBlocks[i] = FAllocBlock{};
    }
    for (size_t i = 0; i < (sizeof(GVirtualAllocBlocks) / sizeof(GVirtualAllocBlocks[0])); i++) {
        GVirtualAllocBlocks[i] = FVirtualAllocBlock{};
    }
    for (size_t i = 0; i < (sizeof(GDmaAllocRecords) / sizeof(GDmaAllocRecords[0])); i++) {
        GDmaAllocRecords[i] = FDmaAllocRecord{};
    }
    for (size_t i = 0; i < (sizeof(GPinnedAllocRecords) / sizeof(GPinnedAllocRecords[0])); i++) {
        GPinnedAllocRecords[i] = FPinnedAllocRecord{};
    }
}

bool TryProcessMemoryMapCommand(FKernelMemoryMapCommandContext &context) {
    if (context.CommandBuffer == nullptr || context.CommandLength == nullptr || context.PushLogFn == nullptr ||
        context.ReserveVirtualRangeFn == nullptr || context.ReleaseVirtualRangeFn == nullptr) {
        return false;
    }

    if (StrEq(context.CommandBuffer, "palloc") || StartsWith(context.CommandBuffer, "palloc ")) {
        uint64_t pageCount = 1;
        if (StartsWith(context.CommandBuffer, "palloc ")) {
            const char *arg = context.CommandBuffer + 7;
            if (!ParseUInt(arg, pageCount) || pageCount == 0) {
                context.PushLogFn("PALLOC ARG INVALID");
                ClearCommandInput(context);
                return true;
            }
        }

        const uint64_t page = FPhysicalMemoryManager::AllocatePages(pageCount, 1);
        if (page == 0) {
            context.PushLogFn("PMM ALLOC FAILED");
        } else {
            if (GAllocatedBlockCount < (sizeof(GAllocatedBlocks) / sizeof(GAllocatedBlocks[0]))) {
                GAllocatedBlocks[GAllocatedBlockCount++] = FAllocBlock{.Address = page, .PageCount = pageCount};
            }
            char line[96] = {};
            size_t pos = 0;
            AppendString(line, sizeof(line), pos, "PMM ALLOC ");
            AppendUInt(line, sizeof(line), pos, pageCount);
            AppendString(line, sizeof(line), pos, "P ");
            AppendHex(line, sizeof(line), pos, page);
            context.PushLogFn(line);
        }
        return true;
    }

    if (StrEq(context.CommandBuffer, "pfree")) {
        if (GAllocatedBlockCount == 0) {
            context.PushLogFn("PMM FREE EMPTY");
        } else {
            const FAllocBlock block = GAllocatedBlocks[--GAllocatedBlockCount];
            if (FPhysicalMemoryManager::FreePages(block.Address, block.PageCount)) {
                char line[96] = {};
                size_t pos = 0;
                AppendString(line, sizeof(line), pos, "PMM FREE ");
                AppendUInt(line, sizeof(line), pos, block.PageCount);
                AppendString(line, sizeof(line), pos, "P ");
                AppendHex(line, sizeof(line), pos, block.Address);
                context.PushLogFn(line);
            } else {
                context.PushLogFn("PMM FREE FAILED");
            }
        }
        return true;
    }

    if (StrEq(context.CommandBuffer, "preserve low")) {
        const uint64_t reserved = FPhysicalMemoryManager::ReserveRange(0, 0x100000);
        char line[96] = {};
        size_t pos = 0;
        AppendString(line, sizeof(line), pos, "PMM RESERVED LOW PAGES ");
        AppendUInt(line, sizeof(line), pos, reserved);
        context.PushLogFn(line);
        return true;
    }

    if (StartsWith(context.CommandBuffer, "vmmap ")) {
        const char *cursor = context.CommandBuffer + 6;
        char tokenVA[32] = {};
        char tokenPA[32] = {};
        char token3[32] = {};
        char token4[32] = {};

        if (!ReadToken(cursor, tokenVA, sizeof(tokenVA)) || !ReadToken(cursor, tokenPA, sizeof(tokenPA))) {
            context.PushLogFn("USAGE: VMMAP VA PA [N] [MODE]");
            ClearCommandInput(context);
            return true;
        }

        uint64_t virtualAddress = 0;
        uint64_t physicalAddress = 0;
        uint64_t pageCount = 1;
        uint64_t mapFlags = FVirtualMemoryManager::FlagsKernelRW;
        if (!ParseU64Auto(tokenVA, virtualAddress) || !ParseU64Auto(tokenPA, physicalAddress)) {
            context.PushLogFn("VMMAP ARG INVALID");
            ClearCommandInput(context);
            return true;
        }

        if (ReadToken(cursor, token3, sizeof(token3))) {
            uint64_t parsedCount = 0;
            if (ParseUInt(token3, parsedCount)) {
                if (parsedCount == 0) {
                    context.PushLogFn("VMMAP COUNT INVALID");
                    ClearCommandInput(context);
                    return true;
                }
                pageCount = parsedCount;
                if (ReadToken(cursor, token4, sizeof(token4)) && !ParseVmMapMode(token4, mapFlags)) {
                    context.PushLogFn("VMMAP MODE INVALID");
                    ClearCommandInput(context);
                    return true;
                }
            } else {
                if (!ParseVmMapMode(token3, mapFlags)) {
                    context.PushLogFn("VMMAP MODE INVALID");
                    ClearCommandInput(context);
                    return true;
                }
            }
        }

        char extraToken[8] = {};
        if (ReadToken(cursor, extraToken, sizeof(extraToken))) {
            context.PushLogFn("VMMAP TOO MANY ARGS");
            ClearCommandInput(context);
            return true;
        }

        if (!FVirtualMemoryManager::MapPages(virtualAddress, physicalAddress, pageCount, mapFlags)) {
            context.PushLogFn("VMM MAP FAILED");
        } else {
            char line[96] = {};
            size_t pos = 0;
            AppendString(line, sizeof(line), pos, "VMM MAP ");
            AppendUInt(line, sizeof(line), pos, pageCount);
            AppendString(line, sizeof(line), pos, "P V:");
            AppendHex(line, sizeof(line), pos, virtualAddress);
            AppendString(line, sizeof(line), pos, " P:");
            AppendHex(line, sizeof(line), pos, physicalAddress);
            context.PushLogFn(line);
        }
        return true;
    }

    if (StartsWith(context.CommandBuffer, "vmalloc ")) {
        const char *cursor = context.CommandBuffer + 8;
        char tokenCount[32] = {};
        char tokenMode[32] = {};

        if (!ReadToken(cursor, tokenCount, sizeof(tokenCount))) {
            context.PushLogFn("USAGE: VMALLOC N [MODE]");
            ClearCommandInput(context);
            return true;
        }

        uint64_t pageCount = 0;
        uint64_t mapFlags = FVirtualMemoryManager::FlagsKernelRW;
        if (!ParseUInt(tokenCount, pageCount) || pageCount == 0) {
            context.PushLogFn("VMALLOC COUNT INVALID");
            ClearCommandInput(context);
            return true;
        }

        if (ReadToken(cursor, tokenMode, sizeof(tokenMode)) && !ParseVmMapMode(tokenMode, mapFlags)) {
            context.PushLogFn("VMALLOC MODE INVALID");
            ClearCommandInput(context);
            return true;
        }

        char extraToken[8] = {};
        if (ReadToken(cursor, extraToken, sizeof(extraToken))) {
            context.PushLogFn("VMALLOC TOO MANY ARGS");
            ClearCommandInput(context);
            return true;
        }

        if (GVirtualAllocBlockCount >= (sizeof(GVirtualAllocBlocks) / sizeof(GVirtualAllocBlocks[0]))) {
            context.PushLogFn("VMALLOC TRACK FULL");
            ClearCommandInput(context);
            return true;
        }

        const uint64_t physicalAddress = FPhysicalMemoryManager::AllocatePages(pageCount, 1);
        if (physicalAddress == 0) {
            context.PushLogFn("VMALLOC PMM FAIL");
            ClearCommandInput(context);
            return true;
        }

        uint64_t virtualAddress = 0;
        if (!context.ReserveVirtualRangeFn(pageCount, virtualAddress)) {
            (void)FPhysicalMemoryManager::FreePages(physicalAddress, pageCount);
            context.PushLogFn("VMALLOC VA FAIL");
            ClearCommandInput(context);
            return true;
        }

        if (!FVirtualMemoryManager::MapPages(virtualAddress, physicalAddress, pageCount, mapFlags)) {
            (void)FPhysicalMemoryManager::FreePages(physicalAddress, pageCount);
            (void)context.ReleaseVirtualRangeFn(virtualAddress, pageCount);
            context.PushLogFn("VMALLOC MAP FAIL");
            ClearCommandInput(context);
            return true;
        }

        size_t blockIndex = GVirtualAllocBlockCount;
        for (size_t i = 0; i < GVirtualAllocBlockCount; i++) {
            if (!GVirtualAllocBlocks[i].InUse) {
                blockIndex = i;
                break;
            }
        }
        if (blockIndex == GVirtualAllocBlockCount) {
            GVirtualAllocBlockCount++;
        }

        GVirtualAllocBlocks[blockIndex] = FVirtualAllocBlock{
            .VirtualAddress = virtualAddress,
            .PhysicalAddress = physicalAddress,
            .PageCount = pageCount,
            .Flags = mapFlags,
            .InUse = true,
        };

        char line[96] = {};
        size_t pos = 0;
        AppendString(line, sizeof(line), pos, "VMALLOC ");
        AppendUInt(line, sizeof(line), pos, pageCount);
        AppendString(line, sizeof(line), pos, "P V:");
        AppendHex(line, sizeof(line), pos, virtualAddress);
        AppendString(line, sizeof(line), pos, " P:");
        AppendHex(line, sizeof(line), pos, physicalAddress);
        context.PushLogFn(line);
        return true;
    }

    if (StrEq(context.CommandBuffer, "vmfree") || StartsWith(context.CommandBuffer, "vmfree ")) {
        int64_t blockIndex = -1;
        if (StartsWith(context.CommandBuffer, "vmfree ")) {
            const char *cursor = context.CommandBuffer + 7;
            char tokenVA[32] = {};
            if (!ReadToken(cursor, tokenVA, sizeof(tokenVA))) {
                context.PushLogFn("USAGE: VMFREE [VA]");
                ClearCommandInput(context);
                return true;
            }

            uint64_t virtualAddress = 0;
            if (!ParseU64Auto(tokenVA, virtualAddress)) {
                context.PushLogFn("VMFREE ARG INVALID");
                ClearCommandInput(context);
                return true;
            }

            blockIndex = FindVmallocBlockByVA(virtualAddress);
        } else {
            blockIndex = FindLatestVmallocBlock();
        }

        if (blockIndex < 0) {
            context.PushLogFn("VMFREE EMPTY");
            ClearCommandInput(context);
            return true;
        }

        FVirtualAllocBlock block = GVirtualAllocBlocks[static_cast<size_t>(blockIndex)];
        uint64_t unmappedPages = 0;
        bool unmapOk = true;
        for (uint64_t i = 0; i < block.PageCount; i++) {
            if (!FVirtualMemoryManager::UnmapPage(block.VirtualAddress + i * FVirtualMemoryManager::PageSize)) {
                unmapOk = false;
                break;
            }
            unmappedPages++;
        }

        if (!unmapOk) {
            if (unmappedPages > 0) {
                (void)FVirtualMemoryManager::MapPages(block.VirtualAddress, block.PhysicalAddress, unmappedPages, block.Flags);
            }
            context.PushLogFn("VMFREE UNMAP FAIL");
            ClearCommandInput(context);
            return true;
        }

        if (!FPhysicalMemoryManager::FreePages(block.PhysicalAddress, block.PageCount)) {
            (void)FVirtualMemoryManager::MapPages(block.VirtualAddress, block.PhysicalAddress, block.PageCount, block.Flags);
            context.PushLogFn("VMFREE PMM FAIL");
            ClearCommandInput(context);
            return true;
        }

        GVirtualAllocBlocks[static_cast<size_t>(blockIndex)].InUse = false;
        (void)context.ReleaseVirtualRangeFn(block.VirtualAddress, block.PageCount);

        char line[96] = {};
        size_t pos = 0;
        AppendString(line, sizeof(line), pos, "VMFREE ");
        AppendUInt(line, sizeof(line), pos, block.PageCount);
        AppendString(line, sizeof(line), pos, "P V:");
        AppendHex(line, sizeof(line), pos, block.VirtualAddress);
        AppendString(line, sizeof(line), pos, " P:");
        AppendHex(line, sizeof(line), pos, block.PhysicalAddress);
        context.PushLogFn(line);
        return true;
    }

    if (StartsWith(context.CommandBuffer, "vmunmap ")) {
        const char *cursor = context.CommandBuffer + 8;
        char tokenVA[32] = {};
        char tokenCount[32] = {};

        if (!ReadToken(cursor, tokenVA, sizeof(tokenVA))) {
            context.PushLogFn("USAGE: VMUNMAP VA [N]");
            ClearCommandInput(context);
            return true;
        }

        uint64_t virtualAddress = 0;
        uint64_t pageCount = 1;
        if (!ParseU64Auto(tokenVA, virtualAddress)) {
            context.PushLogFn("VMUNMAP ARG INVALID");
            ClearCommandInput(context);
            return true;
        }

        if (ReadToken(cursor, tokenCount, sizeof(tokenCount))) {
            if (!ParseUInt(tokenCount, pageCount) || pageCount == 0) {
                context.PushLogFn("VMUNMAP COUNT INVALID");
                ClearCommandInput(context);
                return true;
            }
        }

        bool ok = true;
        for (uint64_t i = 0; i < pageCount; i++) {
            if (!FVirtualMemoryManager::UnmapPage(virtualAddress + i * FVirtualMemoryManager::PageSize)) {
                ok = false;
                break;
            }
        }

        if (!ok) {
            context.PushLogFn("VMM UNMAP FAILED");
        } else {
            char line[96] = {};
            size_t pos = 0;
            AppendString(line, sizeof(line), pos, "VMM UNMAP ");
            AppendUInt(line, sizeof(line), pos, pageCount);
            AppendString(line, sizeof(line), pos, "P V:");
            AppendHex(line, sizeof(line), pos, virtualAddress);
            context.PushLogFn(line);
        }
        return true;
    }

    if (StartsWith(context.CommandBuffer, "vmtranslate ")) {
        const char *cursor = context.CommandBuffer + 12;
        char tokenVA[32] = {};
        if (!ReadToken(cursor, tokenVA, sizeof(tokenVA))) {
            context.PushLogFn("USAGE: VMTRANSLATE VA");
            ClearCommandInput(context);
            return true;
        }

        uint64_t virtualAddress = 0;
        if (!ParseU64Auto(tokenVA, virtualAddress)) {
            context.PushLogFn("VMTRANSLATE ARG INVALID");
            ClearCommandInput(context);
            return true;
        }

        const uint64_t physicalAddress = FVirtualMemoryManager::Translate(virtualAddress);
        if (physicalAddress == 0) {
            context.PushLogFn("VMM TRANSLATE MISS");
        } else {
            char line[96] = {};
            size_t pos = 0;
            AppendString(line, sizeof(line), pos, "VMM XLT V:");
            AppendHex(line, sizeof(line), pos, virtualAddress);
            AppendString(line, sizeof(line), pos, " P:");
            AppendHex(line, sizeof(line), pos, physicalAddress);
            context.PushLogFn(line);
        }
        return true;
    }

    if (StartsWith(context.CommandBuffer, "dmaalloc ")) {
        const char *cursor = context.CommandBuffer + 9;
        char tokenSize[32] = {};
        char token2[32] = {};
        char token3[32] = {};

        if (!ReadToken(cursor, tokenSize, sizeof(tokenSize))) {
            context.PushLogFn("USAGE: DMAALLOC BYTES [ALIGN] [LOW4G]");
            ClearCommandInput(context);
            return true;
        }

        uint64_t sizeBytes = 0;
        uint64_t alignmentBytes = FVirtualMemoryManager::PageSize;
        bool below4GiB = false;

        if (!ParseUInt(tokenSize, sizeBytes) || sizeBytes == 0) {
            context.PushLogFn("DMAALLOC SIZE INVALID");
            ClearCommandInput(context);
            return true;
        }

        if (ReadToken(cursor, token2, sizeof(token2))) {
            uint64_t parsedAlign = 0;
            if (ParseU64Auto(token2, parsedAlign)) {
                if (parsedAlign == 0) {
                    context.PushLogFn("DMAALLOC ALIGN INVALID");
                    ClearCommandInput(context);
                    return true;
                }

                alignmentBytes = parsedAlign;
                if (ReadToken(cursor, token3, sizeof(token3))) {
                    if (!StrEq(token3, "low4g")) {
                        context.PushLogFn("DMAALLOC ARG INVALID");
                        ClearCommandInput(context);
                        return true;
                    }
                    below4GiB = true;
                }
            } else if (StrEq(token2, "low4g")) {
                below4GiB = true;
            } else {
                context.PushLogFn("DMAALLOC ARG INVALID");
                ClearCommandInput(context);
                return true;
            }
        }

        char extraToken[8] = {};
        if (ReadToken(cursor, extraToken, sizeof(extraToken))) {
            context.PushLogFn("DMAALLOC TOO MANY ARGS");
            ClearCommandInput(context);
            return true;
        }

        if (GDmaAllocRecordCount >= (sizeof(GDmaAllocRecords) / sizeof(GDmaAllocRecords[0]))) {
            context.PushLogFn("DMAALLOC TRACK FULL");
            ClearCommandInput(context);
            return true;
        }

        FDmaBuffer buffer{};
        if (!FDmaMemoryManager::AllocateBuffer(sizeBytes, alignmentBytes, below4GiB, buffer)) {
            context.PushLogFn("DMAALLOC FAILED");
            ClearCommandInput(context);
            return true;
        }

        size_t slot = GDmaAllocRecordCount;
        for (size_t i = 0; i < GDmaAllocRecordCount; i++) {
            if (!GDmaAllocRecords[i].InUse) {
                slot = i;
                break;
            }
        }
        if (slot == GDmaAllocRecordCount) {
            GDmaAllocRecordCount++;
        }

        GDmaAllocRecords[slot] = FDmaAllocRecord{.Buffer = buffer, .InUse = true};

        char line[96] = {};
        size_t pos = 0;
        AppendString(line, sizeof(line), pos, "DMA ALLOC ");
        AppendUInt(line, sizeof(line), pos, buffer.PageCount);
        AppendString(line, sizeof(line), pos, "P V:");
        AppendHex(line, sizeof(line), pos, buffer.VirtualAddress);
        AppendString(line, sizeof(line), pos, " P:");
        AppendHex(line, sizeof(line), pos, buffer.PhysicalAddress);
        context.PushLogFn(line);
        return true;
    }

    if (StrEq(context.CommandBuffer, "dmafree") || StartsWith(context.CommandBuffer, "dmafree ")) {
        int64_t slot = -1;
        if (StartsWith(context.CommandBuffer, "dmafree ")) {
            const char *cursor = context.CommandBuffer + 8;
            char tokenVA[32] = {};
            if (!ReadToken(cursor, tokenVA, sizeof(tokenVA))) {
                context.PushLogFn("USAGE: DMAFREE [VA]");
                ClearCommandInput(context);
                return true;
            }

            uint64_t virtualAddress = 0;
            if (!ParseU64Auto(tokenVA, virtualAddress)) {
                context.PushLogFn("DMAFREE ARG INVALID");
                ClearCommandInput(context);
                return true;
            }

            slot = FindDmaRecordByVirtualAddress(virtualAddress);
        } else {
            slot = FindLatestDmaRecord();
        }

        if (slot < 0) {
            context.PushLogFn("DMAFREE EMPTY");
            ClearCommandInput(context);
            return true;
        }

        FDmaBuffer buffer = GDmaAllocRecords[static_cast<size_t>(slot)].Buffer;
        const uint64_t freedVirtualAddress = buffer.VirtualAddress;
        if (!FDmaMemoryManager::FreeBuffer(buffer)) {
            context.PushLogFn("DMAFREE FAILED");
            ClearCommandInput(context);
            return true;
        }

        GDmaAllocRecords[static_cast<size_t>(slot)] = FDmaAllocRecord{};

        char line[96] = {};
        size_t pos = 0;
        AppendString(line, sizeof(line), pos, "DMA FREE V:");
        AppendHex(line, sizeof(line), pos, freedVirtualAddress);
        context.PushLogFn(line);
        return true;
    }

    if (StartsWith(context.CommandBuffer, "mmpin ")) {
        const char *cursor = context.CommandBuffer + 6;
        char tokenPA[32] = {};
        char tokenSize[32] = {};
        char tokenMode[32] = {};

        if (!ReadToken(cursor, tokenPA, sizeof(tokenPA)) || !ReadToken(cursor, tokenSize, sizeof(tokenSize))) {
            context.PushLogFn("USAGE: MMPIN PA BYTES [rw|rwnx|rx|dev]");
            ClearCommandInput(context);
            return true;
        }

        uint64_t physicalAddress = 0;
        uint64_t sizeBytes = 0;
        uint64_t mapFlags = FVirtualMemoryManager::FlagsDeviceRWUCNX;
        if (!ParseU64Auto(tokenPA, physicalAddress) || !ParseUInt(tokenSize, sizeBytes) || sizeBytes == 0) {
            context.PushLogFn("MMPIN ARG INVALID");
            ClearCommandInput(context);
            return true;
        }

        if (ReadToken(cursor, tokenMode, sizeof(tokenMode)) && !ParseVmMapMode(tokenMode, mapFlags)) {
            context.PushLogFn("MMPIN MODE INVALID");
            ClearCommandInput(context);
            return true;
        }

        char extraToken[8] = {};
        if (ReadToken(cursor, extraToken, sizeof(extraToken))) {
            context.PushLogFn("MMPIN TOO MANY ARGS");
            ClearCommandInput(context);
            return true;
        }

        if (GPinnedAllocRecordCount >= (sizeof(GPinnedAllocRecords) / sizeof(GPinnedAllocRecords[0]))) {
            context.PushLogFn("MMPIN TRACK FULL");
            ClearCommandInput(context);
            return true;
        }

        FPinnedMapping mapping{};
        if (!FPinnedMappingManager::MapPhysicalRange(physicalAddress, sizeBytes, mapFlags, mapping)) {
            context.PushLogFn("MMPIN FAILED");
            ClearCommandInput(context);
            return true;
        }

        size_t slot = GPinnedAllocRecordCount;
        for (size_t i = 0; i < GPinnedAllocRecordCount; i++) {
            if (!GPinnedAllocRecords[i].InUse) {
                slot = i;
                break;
            }
        }
        if (slot == GPinnedAllocRecordCount) {
            GPinnedAllocRecordCount++;
        }

        GPinnedAllocRecords[slot] = FPinnedAllocRecord{.Mapping = mapping, .InUse = true};

        char line[96] = {};
        size_t pos = 0;
        AppendString(line, sizeof(line), pos, "MMPIN V:");
        AppendHex(line, sizeof(line), pos, mapping.VirtualAddress);
        AppendString(line, sizeof(line), pos, " P:");
        AppendHex(line, sizeof(line), pos, mapping.PhysicalAddress);
        context.PushLogFn(line);
        return true;
    }

    if (StrEq(context.CommandBuffer, "mmunpin") || StartsWith(context.CommandBuffer, "mmunpin ")) {
        int64_t slot = -1;
        if (StartsWith(context.CommandBuffer, "mmunpin ")) {
            const char *cursor = context.CommandBuffer + 8;
            char tokenVA[32] = {};
            if (!ReadToken(cursor, tokenVA, sizeof(tokenVA))) {
                context.PushLogFn("USAGE: MMUNPIN [VA]");
                ClearCommandInput(context);
                return true;
            }

            uint64_t virtualAddress = 0;
            if (!ParseU64Auto(tokenVA, virtualAddress)) {
                context.PushLogFn("MMUNPIN ARG INVALID");
                ClearCommandInput(context);
                return true;
            }

            slot = FindPinnedRecordByVirtualAddress(virtualAddress);
        } else {
            slot = FindLatestPinnedRecord();
        }

        if (slot < 0) {
            context.PushLogFn("MMUNPIN EMPTY");
            ClearCommandInput(context);
            return true;
        }

        FPinnedMapping mapping = GPinnedAllocRecords[static_cast<size_t>(slot)].Mapping;
        if (!FPinnedMappingManager::UnmapPhysicalRange(mapping)) {
            context.PushLogFn("MMUNPIN FAILED");
            ClearCommandInput(context);
            return true;
        }

        GPinnedAllocRecords[static_cast<size_t>(slot)] = FPinnedAllocRecord{};
        context.PushLogFn("MMUNPIN OK");
        return true;
    }

    return false;
}

} // namespace Fortress::Kernel
