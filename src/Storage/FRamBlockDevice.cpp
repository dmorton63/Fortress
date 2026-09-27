#include "Fortress/Storage/FRamBlockDevice.hpp"

namespace Fortress::Storage {

static bool ComputeCopyBounds(Fortress::Core::uint64 startBlock,
                              Fortress::Core::uint32 blockCount,
                              Fortress::Core::uint32 bytesPerBlock,
                              Fortress::Core::uint64 totalBlocks,
                              Fortress::Core::uint32 bufferBytes,
                              Fortress::Core::uint64 &outOffset,
                              Fortress::Core::uint32 &outBytes) {
    if (blockCount == 0 || bytesPerBlock == 0 || startBlock >= totalBlocks) {
        return false;
    }

    const Fortress::Core::uint64 endBlockExclusive = startBlock + static_cast<Fortress::Core::uint64>(blockCount);
    if (endBlockExclusive < startBlock || endBlockExclusive > totalBlocks) {
        return false;
    }

    const Fortress::Core::uint64 totalBytes64 = static_cast<Fortress::Core::uint64>(blockCount) *
                                                static_cast<Fortress::Core::uint64>(bytesPerBlock);
    if (totalBytes64 > 0xFFFFFFFFull) {
        return false;
    }

    const Fortress::Core::uint32 totalBytes = static_cast<Fortress::Core::uint32>(totalBytes64);
    if (bufferBytes < totalBytes) {
        return false;
    }

    outOffset = startBlock * static_cast<Fortress::Core::uint64>(bytesPerBlock);
    outBytes = totalBytes;
    return true;
}

bool FRamBlockDevice::Initialize(Fortress::Core::uint32 blockSizeBytes,
                                 Fortress::Core::uint32 blockCount,
                                 bool readOnly) {
    if (blockSizeBytes != SupportedBlockSizeBytes || blockCount == 0 || blockCount > MaxBlocks) {
        return false;
    }

    GInitialized = true;
    GReadOnly = readOnly;
    GBlockSizeBytes = blockSizeBytes;
    GBlockCount = blockCount;

    const Fortress::Core::uint32 capacityBytes = GBlockCount * GBlockSizeBytes;
    for (Fortress::Core::uint32 i = 0; i < capacityBytes; i++) {
        GStorage[i] = 0;
    }

    return true;
}

Fortress::Core::uint32 FRamBlockDevice::GetBlockSizeBytes() const {
    return GBlockSizeBytes;
}

Fortress::Core::uint64 FRamBlockDevice::GetBlockCount() const {
    return GBlockCount;
}

bool FRamBlockDevice::IsReadOnly() const {
    return GReadOnly;
}

bool FRamBlockDevice::ReadBlocks(Fortress::Core::uint64 startBlock,
                                 Fortress::Core::uint32 blockCount,
                                 void *outBuffer,
                                 Fortress::Core::uint32 outBufferBytes) {
    if (!GInitialized || outBuffer == nullptr) {
        return false;
    }

    Fortress::Core::uint64 offset = 0;
    Fortress::Core::uint32 bytesToCopy = 0;
    if (!ComputeCopyBounds(startBlock,
                           blockCount,
                           GBlockSizeBytes,
                           GBlockCount,
                           outBufferBytes,
                           offset,
                           bytesToCopy)) {
        return false;
    }

    Fortress::Core::uint8 *dst = static_cast<Fortress::Core::uint8 *>(outBuffer);
    for (Fortress::Core::uint32 i = 0; i < bytesToCopy; i++) {
        dst[i] = GStorage[offset + i];
    }

    return true;
}

bool FRamBlockDevice::WriteBlocks(Fortress::Core::uint64 startBlock,
                                  Fortress::Core::uint32 blockCount,
                                  const void *inBuffer,
                                  Fortress::Core::uint32 inBufferBytes) {
    if (!GInitialized || GReadOnly || inBuffer == nullptr) {
        return false;
    }

    Fortress::Core::uint64 offset = 0;
    Fortress::Core::uint32 bytesToCopy = 0;
    if (!ComputeCopyBounds(startBlock,
                           blockCount,
                           GBlockSizeBytes,
                           GBlockCount,
                           inBufferBytes,
                           offset,
                           bytesToCopy)) {
        return false;
    }

    const Fortress::Core::uint8 *src = static_cast<const Fortress::Core::uint8 *>(inBuffer);
    for (Fortress::Core::uint32 i = 0; i < bytesToCopy; i++) {
        GStorage[offset + i] = src[i];
    }

    return true;
}

const char *FRamBlockDevice::GetDeviceName() const {
    return "RAMBLOCK";
}

} // namespace Fortress::Storage