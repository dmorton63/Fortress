#include "Fortress/Storage/FSimpleFileSystemDriver.hpp"

#include "Fortress/Runtime/FRuntime.hpp"
#include "Fortress/Storage/IBlockDevice.hpp"

namespace Fortress::Storage {

static Fortress::Core::uint32 StringLengthBounded(const char *value, Fortress::Core::uint32 maxLen) {
    if (value == nullptr) {
        return 0u;
    }

    Fortress::Core::uint32 len = 0u;
    while (len < maxLen && value[len] != '\0') {
        len++;
    }
    return len;
}

const char *FSimpleFileSystemDriver::GetDriverName() const {
    return "SFS";
}

bool FSimpleFileSystemDriver::ProbeAndBind(IBlockDevice &device, bool mountReadOnly) {
    const Fortress::Core::uint32 blockSize = device.GetBlockSizeBytes();
    const Fortress::Core::uint64 totalBlocks64 = device.GetBlockCount();
    if (blockSize == 0u || blockSize > MaxBlockSizeBytes || totalBlocks64 == 0u || totalBlocks64 > 0xFFFFFFFFull) {
        return false;
    }

    const bool effectiveReadOnly = mountReadOnly || device.IsReadOnly();
    if (!LoadOrFormat(device, effectiveReadOnly)) {
        return false;
    }

    GBound = true;
    GBoundReadOnly = effectiveReadOnly;
    return true;
}

bool FSimpleFileSystemDriver::LoadOrFormat(IBlockDevice &device, bool mountReadOnly) {
    FSuperblock superblock{};
    if (ReadSuperblock(device, superblock) &&
        superblock.Magic == SuperblockMagic &&
        superblock.Version == SuperblockVersion &&
        superblock.BlockSizeBytes == device.GetBlockSizeBytes() &&
        superblock.TotalBlocks == device.GetBlockCount() &&
        superblock.EntrySizeBytes == sizeof(FDirectoryEntry) &&
        superblock.DirectoryBlockCount > 0u &&
        superblock.DirectoryBlockCount <= MaxDirectoryBlocks &&
        superblock.DataStartBlock > superblock.DirectoryStartBlock &&
        superblock.NextFreeBlock >= superblock.DataStartBlock &&
        superblock.NextFreeBlock <= superblock.TotalBlocks) {
        GSuperblock = superblock;
        return true;
    }

    if (mountReadOnly) {
        return false;
    }

    if (!FormatEmpty(device)) {
        return false;
    }

    return ReadSuperblock(device, GSuperblock);
}

bool FSimpleFileSystemDriver::FormatEmpty(IBlockDevice &device) {
    const Fortress::Core::uint32 blockSize = device.GetBlockSizeBytes();
    const Fortress::Core::uint32 totalBlocks = static_cast<Fortress::Core::uint32>(device.GetBlockCount());

    Fortress::Core::uint32 directoryBlocks = totalBlocks / 32u;
    if (directoryBlocks < 1u) {
        directoryBlocks = 1u;
    }
    if (directoryBlocks > MaxDirectoryBlocks) {
        directoryBlocks = MaxDirectoryBlocks;
    }

    const Fortress::Core::uint32 directoryStart = 1u;
    const Fortress::Core::uint32 dataStart = directoryStart + directoryBlocks;
    if (dataStart >= totalBlocks) {
        return false;
    }

    FSuperblock superblock{};
    superblock.Magic = SuperblockMagic;
    superblock.Version = SuperblockVersion;
    superblock.BlockSizeBytes = blockSize;
    superblock.TotalBlocks = totalBlocks;
    superblock.DirectoryStartBlock = directoryStart;
    superblock.DirectoryBlockCount = directoryBlocks;
    superblock.DataStartBlock = dataStart;
    superblock.NextFreeBlock = dataStart;
    superblock.EntrySizeBytes = sizeof(FDirectoryEntry);
    superblock.MaxEntries = (directoryBlocks * blockSize) / sizeof(FDirectoryEntry);
    superblock.FileCount = 0u;

    if (!WriteSuperblock(device, superblock)) {
        return false;
    }

    Fortress::Core::uint8 zeroBlock[MaxBlockSizeBytes] = {};
    for (Fortress::Core::uint32 i = 0u; i < blockSize; i++) {
        zeroBlock[i] = 0u;
    }

    for (Fortress::Core::uint32 b = 0u; b < directoryBlocks; b++) {
        if (!device.WriteBlocks(static_cast<Fortress::Core::uint64>(directoryStart + b), 1u, zeroBlock, blockSize)) {
            return false;
        }
    }

    GSuperblock = superblock;
    return true;
}

bool FSimpleFileSystemDriver::ReadSuperblock(IBlockDevice &device, FSuperblock &outSuperblock) const {
    Fortress::Core::uint8 block[MaxBlockSizeBytes] = {};
    const Fortress::Core::uint32 blockSize = device.GetBlockSizeBytes();
    if (blockSize < sizeof(FSuperblock)) {
        return false;
    }

    if (!device.ReadBlocks(0u, 1u, block, blockSize)) {
        return false;
    }

    Fortress::Runtime::Memcpy(&outSuperblock, block, sizeof(FSuperblock));
    return true;
}

bool FSimpleFileSystemDriver::WriteSuperblock(IBlockDevice &device, const FSuperblock &superblock) const {
    Fortress::Core::uint8 block[MaxBlockSizeBytes] = {};
    const Fortress::Core::uint32 blockSize = device.GetBlockSizeBytes();
    if (blockSize < sizeof(FSuperblock)) {
        return false;
    }

    for (Fortress::Core::uint32 i = 0u; i < blockSize; i++) {
        block[i] = 0u;
    }
    Fortress::Runtime::Memcpy(block, &superblock, sizeof(FSuperblock));
    return device.WriteBlocks(0u, 1u, block, blockSize);
}

bool FSimpleFileSystemDriver::NormalizeRelativePath(const char *relativePath,
                                                    char *outPath,
                                                    Fortress::Core::uint32 outSize) const {
    if (relativePath == nullptr || outPath == nullptr || outSize < 2u) {
        return false;
    }

    Fortress::Core::uint32 src = 0u;
    if (relativePath[0] == '/') {
        src = 1u;
    }

    Fortress::Core::uint32 dst = 0u;
    Fortress::Core::uint32 segmentLen = 0u;
    bool segmentOnlyDots = true;
    char lastChar = '\0';
    while (relativePath[src] != '\0') {
        const char c = relativePath[src++];
        if (c == '\\') {
            return false;
        }
        if (c == '/') {
            if (segmentLen == 0u || segmentOnlyDots) {
                return false;
            }
            segmentLen = 0u;
            segmentOnlyDots = true;
            lastChar = c;
            if (dst + 1u >= outSize) {
                return false;
            }
            outPath[dst++] = c;
            continue;
        }
        const bool allowed = (c >= 'a' && c <= 'z') ||
                             (c >= 'A' && c <= 'Z') ||
                             (c >= '0' && c <= '9') ||
                             c == '_' || c == '-' || c == '.';
        if (!allowed) {
            return false;
        }
        if (dst + 1u >= outSize) {
            return false;
        }
        outPath[dst++] = c;
        segmentLen++;
        if (c != '.') {
            segmentOnlyDots = false;
        }
        lastChar = c;
    }

    if (segmentLen == 0u || segmentOnlyDots || lastChar == '/') {
        return false;
    }

    outPath[dst] = '\0';
    return dst > 0u;
}

bool FSimpleFileSystemDriver::ReadDirectoryEntry(IBlockDevice &device,
                                                 Fortress::Core::uint32 entryIndex,
                                                 FDirectoryEntry &outEntry) const {
    if (entryIndex >= GSuperblock.MaxEntries) {
        return false;
    }

    const Fortress::Core::uint32 entriesPerBlock = GSuperblock.BlockSizeBytes / sizeof(FDirectoryEntry);
    if (entriesPerBlock == 0u) {
        return false;
    }

    const Fortress::Core::uint32 blockOffset = entryIndex / entriesPerBlock;
    const Fortress::Core::uint32 entryOffset = entryIndex % entriesPerBlock;
    if (blockOffset >= GSuperblock.DirectoryBlockCount) {
        return false;
    }

    Fortress::Core::uint8 block[MaxBlockSizeBytes] = {};
    if (!device.ReadBlocks(static_cast<Fortress::Core::uint64>(GSuperblock.DirectoryStartBlock + blockOffset),
                           1u,
                           block,
                           GSuperblock.BlockSizeBytes)) {
        return false;
    }

    const Fortress::Core::uint32 byteOffset = entryOffset * sizeof(FDirectoryEntry);
    Fortress::Runtime::Memcpy(&outEntry, block + byteOffset, sizeof(FDirectoryEntry));
    return true;
}

bool FSimpleFileSystemDriver::WriteDirectoryEntry(IBlockDevice &device,
                                                  Fortress::Core::uint32 entryIndex,
                                                  const FDirectoryEntry &entry) const {
    if (entryIndex >= GSuperblock.MaxEntries) {
        return false;
    }

    const Fortress::Core::uint32 entriesPerBlock = GSuperblock.BlockSizeBytes / sizeof(FDirectoryEntry);
    if (entriesPerBlock == 0u) {
        return false;
    }

    const Fortress::Core::uint32 blockOffset = entryIndex / entriesPerBlock;
    const Fortress::Core::uint32 entryOffset = entryIndex % entriesPerBlock;
    if (blockOffset >= GSuperblock.DirectoryBlockCount) {
        return false;
    }

    Fortress::Core::uint8 block[MaxBlockSizeBytes] = {};
    const Fortress::Core::uint64 blockIndex = static_cast<Fortress::Core::uint64>(GSuperblock.DirectoryStartBlock + blockOffset);
    if (!device.ReadBlocks(blockIndex, 1u, block, GSuperblock.BlockSizeBytes)) {
        return false;
    }

    const Fortress::Core::uint32 byteOffset = entryOffset * sizeof(FDirectoryEntry);
    Fortress::Runtime::Memcpy(block + byteOffset, &entry, sizeof(FDirectoryEntry));
    return device.WriteBlocks(blockIndex, 1u, block, GSuperblock.BlockSizeBytes);
}

bool FSimpleFileSystemDriver::FindEntry(IBlockDevice &device,
                                        const char *normalizedPath,
                                        Fortress::Core::uint32 &outEntryIndex,
                                        FDirectoryEntry &outEntry) const {
    outEntryIndex = 0u;
    for (Fortress::Core::uint32 i = 0u; i < GSuperblock.MaxEntries; i++) {
        FDirectoryEntry entry{};
        if (!ReadDirectoryEntry(device, i, entry)) {
            return false;
        }
        if (entry.InUse == 0u) {
            continue;
        }

        bool same = true;
        for (Fortress::Core::uint32 c = 0u; c < sizeof(entry.Name); c++) {
            const char left = entry.Name[c];
            const char right = normalizedPath[c];
            if (left != right) {
                same = false;
                break;
            }
            if (left == '\0') {
                break;
            }
        }

        if (same) {
            outEntryIndex = i;
            outEntry = entry;
            return true;
        }
    }

    return false;
}

bool FSimpleFileSystemDriver::FindFreeEntry(IBlockDevice &device, Fortress::Core::uint32 &outEntryIndex) const {
    outEntryIndex = 0u;
    for (Fortress::Core::uint32 i = 0u; i < GSuperblock.MaxEntries; i++) {
        FDirectoryEntry entry{};
        if (!ReadDirectoryEntry(device, i, entry)) {
            return false;
        }
        if (entry.InUse == 0u) {
            outEntryIndex = i;
            return true;
        }
    }

    return false;
}

Fortress::Core::uint32 FSimpleFileSystemDriver::ComputeHighestUsedEndBlock(IBlockDevice &device) const {
    Fortress::Core::uint32 highest = GSuperblock.DataStartBlock;
    for (Fortress::Core::uint32 i = 0u; i < GSuperblock.MaxEntries; i++) {
        FDirectoryEntry entry{};
        if (!ReadDirectoryEntry(device, i, entry)) {
            continue;
        }
        if (entry.InUse == 0u || entry.BlockCount == 0u) {
            continue;
        }
        const Fortress::Core::uint32 end = entry.StartBlock + entry.BlockCount;
        if (end > highest && end <= GSuperblock.TotalBlocks) {
            highest = end;
        }
    }
    return highest;
}

bool FSimpleFileSystemDriver::WriteFileData(IBlockDevice &device,
                                            Fortress::Core::uint32 startBlock,
                                            const void *inBuffer,
                                            Fortress::Core::uint32 inBufferBytes,
                                            Fortress::Core::uint32 blockCount) const {
    if (inBuffer == nullptr || blockCount == 0u) {
        return false;
    }

    const Fortress::Core::uint8 *src = static_cast<const Fortress::Core::uint8 *>(inBuffer);
    Fortress::Core::uint8 block[MaxBlockSizeBytes] = {};
    Fortress::Core::uint32 srcOffset = 0u;

    for (Fortress::Core::uint32 i = 0u; i < blockCount; i++) {
        for (Fortress::Core::uint32 b = 0u; b < GSuperblock.BlockSizeBytes; b++) {
            block[b] = 0u;
        }

        const Fortress::Core::uint32 remaining = (srcOffset < inBufferBytes) ? (inBufferBytes - srcOffset) : 0u;
        const Fortress::Core::uint32 copyBytes =
            (remaining > GSuperblock.BlockSizeBytes) ? GSuperblock.BlockSizeBytes : remaining;
        if (copyBytes > 0u) {
            Fortress::Runtime::Memcpy(block, src + srcOffset, copyBytes);
            srcOffset += copyBytes;
        }

        if (!device.WriteBlocks(static_cast<Fortress::Core::uint64>(startBlock + i), 1u, block, GSuperblock.BlockSizeBytes)) {
            return false;
        }
    }

    return true;
}

bool FSimpleFileSystemDriver::ReadFileData(IBlockDevice &device,
                                           Fortress::Core::uint32 startBlock,
                                           void *outBuffer,
                                           Fortress::Core::uint32 outBufferBytes,
                                           Fortress::Core::uint32 sizeBytes,
                                           Fortress::Core::uint32 blockCount) const {
    if (outBuffer == nullptr || outBufferBytes < sizeBytes) {
        return false;
    }

    Fortress::Core::uint8 *dst = static_cast<Fortress::Core::uint8 *>(outBuffer);
    Fortress::Core::uint8 block[MaxBlockSizeBytes] = {};
    Fortress::Core::uint32 dstOffset = 0u;

    for (Fortress::Core::uint32 i = 0u; i < blockCount; i++) {
        if (!device.ReadBlocks(static_cast<Fortress::Core::uint64>(startBlock + i), 1u, block, GSuperblock.BlockSizeBytes)) {
            return false;
        }

        const Fortress::Core::uint32 remaining = (dstOffset < sizeBytes) ? (sizeBytes - dstOffset) : 0u;
        if (remaining == 0u) {
            break;
        }

        const Fortress::Core::uint32 copyBytes =
            (remaining > GSuperblock.BlockSizeBytes) ? GSuperblock.BlockSizeBytes : remaining;
        Fortress::Runtime::Memcpy(dst + dstOffset, block, copyBytes);
        dstOffset += copyBytes;
    }

    return true;
}

bool FSimpleFileSystemDriver::ReadFile(IBlockDevice &device,
                                       const char *relativePath,
                                       void *outBuffer,
                                       Fortress::Core::uint32 outBufferBytes,
                                       Fortress::Core::uint32 &outReadBytes) {
    outReadBytes = 0u;
    if (!GBound || outBuffer == nullptr) {
        return false;
    }

    char normalized[44] = {};
    if (!NormalizeRelativePath(relativePath, normalized, sizeof(normalized))) {
        return false;
    }

    Fortress::Core::uint32 entryIndex = 0u;
    FDirectoryEntry entry{};
    if (!FindEntry(device, normalized, entryIndex, entry) || entry.InUse == 0u) {
        return false;
    }

    if (entry.SizeBytes == 0u) {
        outReadBytes = 0u;
        return true;
    }

    if (outBufferBytes < entry.SizeBytes) {
        return false;
    }

    if (entry.StartBlock < GSuperblock.DataStartBlock ||
        entry.BlockCount == 0u ||
        entry.StartBlock + entry.BlockCount > GSuperblock.TotalBlocks) {
        return false;
    }

    if (!ReadFileData(device, entry.StartBlock, outBuffer, outBufferBytes, entry.SizeBytes, entry.BlockCount)) {
        return false;
    }

    outReadBytes = entry.SizeBytes;
    return true;
}

bool FSimpleFileSystemDriver::WriteFile(IBlockDevice &device,
                                        const char *relativePath,
                                        const void *inBuffer,
                                        Fortress::Core::uint32 inBufferBytes,
                                        Fortress::Core::uint32 &outWrittenBytes) {
    outWrittenBytes = 0u;
    if (!GBound || GBoundReadOnly || inBuffer == nullptr) {
        return false;
    }

    char normalized[44] = {};
    if (!NormalizeRelativePath(relativePath, normalized, sizeof(normalized))) {
        return false;
    }

    const Fortress::Core::uint32 neededBlocks =
        (inBufferBytes == 0u) ? 1u : ((inBufferBytes + GSuperblock.BlockSizeBytes - 1u) / GSuperblock.BlockSizeBytes);

    Fortress::Core::uint32 entryIndex = 0u;
    FDirectoryEntry entry{};
    const bool exists = FindEntry(device, normalized, entryIndex, entry);

    if (exists) {
        if (entry.InUse == 0u) {
            return false;
        }

        if (entry.BlockCount < neededBlocks) {
            const Fortress::Core::uint32 entryEndBlock = entry.StartBlock + entry.BlockCount;
            const Fortress::Core::uint32 additionalBlocks = neededBlocks - entry.BlockCount;
            const bool canExtendInPlace =
                entryEndBlock == GSuperblock.NextFreeBlock &&
                (GSuperblock.NextFreeBlock + additionalBlocks) <= GSuperblock.TotalBlocks;
            if (canExtendInPlace) {
                entry.BlockCount = neededBlocks;
                GSuperblock.NextFreeBlock += additionalBlocks;
            } else {
                Fortress::Core::uint32 allocatedStart = 0u;
                bool found = false;
                for (Fortress::Core::uint32 candidate = GSuperblock.DataStartBlock;
                     candidate + neededBlocks <= GSuperblock.TotalBlocks;
                     candidate++) {
                    bool overlaps = false;
                    for (Fortress::Core::uint32 i = 0u; i < GSuperblock.MaxEntries; i++) {
                        FDirectoryEntry other{};
                        if (!ReadDirectoryEntry(device, i, other)) {
                            return false;
                        }
                        if (other.InUse == 0u || other.BlockCount == 0u || i == entryIndex) {
                            continue;
                        }

                        const Fortress::Core::uint32 entryStart = other.StartBlock;
                        const Fortress::Core::uint32 entryEnd = other.StartBlock + other.BlockCount;
                        const Fortress::Core::uint32 candStart = candidate;
                        const Fortress::Core::uint32 candEnd = candidate + neededBlocks;
                        if (!(candEnd <= entryStart || candStart >= entryEnd)) {
                            overlaps = true;
                            candidate = entryEnd - 1u;
                            break;
                        }
                    }

                    if (!overlaps) {
                        allocatedStart = candidate;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    return false;
                }
                entry.StartBlock = allocatedStart;
                entry.BlockCount = neededBlocks;
            }
        } else if (entry.BlockCount > neededBlocks) {
            entry.BlockCount = neededBlocks;
        }
    } else {
        if (!FindFreeEntry(device, entryIndex)) {
            return false;
        }

        Fortress::Core::uint32 allocatedStart = 0u;
        bool found = false;
        for (Fortress::Core::uint32 candidate = GSuperblock.DataStartBlock;
             candidate + neededBlocks <= GSuperblock.TotalBlocks;
             candidate++) {
            bool overlaps = false;
            for (Fortress::Core::uint32 i = 0u; i < GSuperblock.MaxEntries; i++) {
                FDirectoryEntry other{};
                if (!ReadDirectoryEntry(device, i, other)) {
                    return false;
                }
                if (other.InUse == 0u || other.BlockCount == 0u) {
                    continue;
                }

                const Fortress::Core::uint32 entryStart = other.StartBlock;
                const Fortress::Core::uint32 entryEnd = other.StartBlock + other.BlockCount;
                const Fortress::Core::uint32 candStart = candidate;
                const Fortress::Core::uint32 candEnd = candidate + neededBlocks;
                if (!(candEnd <= entryStart || candStart >= entryEnd)) {
                    overlaps = true;
                    candidate = entryEnd - 1u;
                    break;
                }
            }

            if (!overlaps) {
                allocatedStart = candidate;
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }

        entry = FDirectoryEntry{};
        entry.InUse = 1u;
        entry.StartBlock = allocatedStart;
        entry.BlockCount = neededBlocks;
        GSuperblock.FileCount++;

        const Fortress::Core::uint32 nameLen = StringLengthBounded(normalized, sizeof(entry.Name) - 1u);
        for (Fortress::Core::uint32 i = 0u; i < nameLen; i++) {
            entry.Name[i] = normalized[i];
        }
        entry.Name[nameLen] = '\0';
    }

    entry.SizeBytes = inBufferBytes;

    if (!WriteFileData(device, entry.StartBlock, inBuffer, inBufferBytes, entry.BlockCount)) {
        return false;
    }

    if (!WriteDirectoryEntry(device, entryIndex, entry)) {
        return false;
    }

    GSuperblock.NextFreeBlock = ComputeHighestUsedEndBlock(device);
    if (GSuperblock.NextFreeBlock < GSuperblock.DataStartBlock) {
        GSuperblock.NextFreeBlock = GSuperblock.DataStartBlock;
    }

    if (!WriteSuperblock(device, GSuperblock)) {
        return false;
    }

    outWrittenBytes = inBufferBytes;
    return true;
}

bool FSimpleFileSystemDriver::DeleteFile(IBlockDevice &device, const char *relativePath) {
    if (!GBound || GBoundReadOnly) {
        return false;
    }

    char normalized[44] = {};
    if (!NormalizeRelativePath(relativePath, normalized, sizeof(normalized))) {
        return false;
    }

    Fortress::Core::uint32 entryIndex = 0u;
    FDirectoryEntry entry{};
    if (!FindEntry(device, normalized, entryIndex, entry) || entry.InUse == 0u) {
        return false;
    }

    entry = FDirectoryEntry{};
    if (!WriteDirectoryEntry(device, entryIndex, entry)) {
        return false;
    }

    if (GSuperblock.FileCount > 0u) {
        GSuperblock.FileCount--;
    }
    GSuperblock.NextFreeBlock = ComputeHighestUsedEndBlock(device);
    if (GSuperblock.NextFreeBlock < GSuperblock.DataStartBlock) {
        GSuperblock.NextFreeBlock = GSuperblock.DataStartBlock;
    }

    return WriteSuperblock(device, GSuperblock);
}

} // namespace Fortress::Storage
