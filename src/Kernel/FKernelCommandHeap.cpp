#include "Fortress/Kernel/FKernelCommandHeap.hpp"

#include <cstdint>

#include "Fortress/Memory/FKernelHeap.hpp"

namespace Fortress::Kernel {

using Fortress::Memory::FKernelHeap;

struct FHeapAllocRecord {
    uint64_t Address;
    bool InUse;
};

static FHeapAllocRecord GHeapAllocRecords[128] = {};
static size_t GHeapAllocRecordCount = 0;

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

static int64_t FindHeapRecordByAddress(uint64_t address) {
    for (size_t i = 0; i < GHeapAllocRecordCount; i++) {
        if (GHeapAllocRecords[i].InUse && GHeapAllocRecords[i].Address == address) {
            return static_cast<int64_t>(i);
        }
    }
    return -1;
}

void ResetKernelCommandHeapState() {
    GHeapAllocRecordCount = 0;
    for (size_t i = 0; i < (sizeof(GHeapAllocRecords) / sizeof(GHeapAllocRecords[0])); i++) {
        GHeapAllocRecords[i] = FHeapAllocRecord{};
    }
}

bool TryProcessHeapCommand(FKernelHeapCommandContext &context) {
    if (context.CommandBuffer == nullptr || context.CommandLength == nullptr || context.PushLog == nullptr ||
        context.ClearCommandInput == nullptr || context.RunMemorySelfTest == nullptr ||
        context.RunXhciMemorySmokeTest == nullptr || context.RunMmioSmokeTest == nullptr) {
        return false;
    }

    if (StartsWith(context.CommandBuffer, "kalloc ")) {
        const char *cursor = context.CommandBuffer + 7;
        char tokenSize[32] = {};
        if (!ReadToken(cursor, tokenSize, sizeof(tokenSize))) {
            context.PushLog("USAGE: KALLOC BYTES");
            context.ClearCommandInput();
            return true;
        }

        uint64_t sizeBytes = 0;
        if (!ParseUInt(tokenSize, sizeBytes) || sizeBytes == 0) {
            context.PushLog("KALLOC SIZE INVALID");
            context.ClearCommandInput();
            return true;
        }

        void *ptr = FKernelHeap::Allocate(sizeBytes, 16);
        if (ptr == nullptr) {
            context.PushLog("KALLOC FAILED");
            return true;
        }

        if (GHeapAllocRecordCount < (sizeof(GHeapAllocRecords) / sizeof(GHeapAllocRecords[0]))) {
            size_t slot = GHeapAllocRecordCount;
            for (size_t i = 0; i < GHeapAllocRecordCount; i++) {
                if (!GHeapAllocRecords[i].InUse) {
                    slot = i;
                    break;
                }
            }
            if (slot == GHeapAllocRecordCount) {
                GHeapAllocRecordCount++;
            }
            GHeapAllocRecords[slot] = FHeapAllocRecord{
                .Address = reinterpret_cast<uint64_t>(ptr),
                .InUse = true,
            };
        }

        char line[96] = {};
        size_t pos = 0;
        AppendString(line, sizeof(line), pos, "KALLOC ");
        AppendUInt(line, sizeof(line), pos, sizeBytes);
        AppendString(line, sizeof(line), pos, "B @ ");
        AppendHex(line, sizeof(line), pos, reinterpret_cast<uint64_t>(ptr));
        context.PushLog(line);
        return true;
    }

    if (StartsWith(context.CommandBuffer, "kfree ")) {
        const char *cursor = context.CommandBuffer + 6;
        char tokenVA[32] = {};
        if (!ReadToken(cursor, tokenVA, sizeof(tokenVA))) {
            context.PushLog("USAGE: KFREE VA");
            context.ClearCommandInput();
            return true;
        }

        uint64_t address = 0;
        if (!ParseU64Auto(tokenVA, address)) {
            context.PushLog("KFREE ARG INVALID");
            context.ClearCommandInput();
            return true;
        }

        int64_t rec = FindHeapRecordByAddress(address);
        if (rec < 0) {
            context.PushLog("KFREE UNKNOWN");
            context.ClearCommandInput();
            return true;
        }

        if (!FKernelHeap::Free(reinterpret_cast<void *>(address))) {
            context.PushLog("KFREE FAILED");
            context.ClearCommandInput();
            return true;
        }

        GHeapAllocRecords[static_cast<size_t>(rec)].InUse = false;
        char line[96] = {};
        size_t pos = 0;
        AppendString(line, sizeof(line), pos, "KFREE ");
        AppendHex(line, sizeof(line), pos, address);
        context.PushLog(line);
        return true;
    }

    if (StrEq(context.CommandBuffer, "memtest")) {
        context.RunMemorySelfTest();
        return true;
    }

    if (StrEq(context.CommandBuffer, "xhcimemtest")) {
        context.RunXhciMemorySmokeTest();
        return true;
    }

    if (StrEq(context.CommandBuffer, "mmiotest")) {
        context.RunMmioSmokeTest();
        return true;
    }

    return false;
}

} // namespace Fortress::Kernel
