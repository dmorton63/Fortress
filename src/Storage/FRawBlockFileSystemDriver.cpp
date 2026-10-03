#include "Fortress/Storage/FRawBlockFileSystemDriver.hpp"

#include "Fortress/Storage/IBlockDevice.hpp"

namespace Fortress::Storage {

static bool ParseBlockPath(const char *relativePath, Fortress::Core::uint64 &outBlockIndex) {
    if (relativePath == nullptr ||
        relativePath[0] != 'b' ||
        relativePath[1] != 'l' ||
        relativePath[2] != 'k' ||
        relativePath[3] != '/') {
        return false;
    }

    Fortress::Core::uint64 value = 0;
    bool haveDigit = false;
    for (Fortress::Core::uint32 i = 4u; relativePath[i] != '\0'; i++) {
        const char c = relativePath[i];
        if (c < '0' || c > '9') {
            return false;
        }

        haveDigit = true;
        value = value * 10u + static_cast<Fortress::Core::uint64>(c - '0');
    }

    if (!haveDigit) {
        return false;
    }

    outBlockIndex = value;
    return true;
}

const char *FRawBlockFileSystemDriver::GetDriverName() const {
    return "RAWFS";
}

bool FRawBlockFileSystemDriver::ProbeAndBind(IBlockDevice &device, bool mountReadOnly) {
    const Fortress::Core::uint32 blockSize = device.GetBlockSizeBytes();
    const Fortress::Core::uint64 blockCount = device.GetBlockCount();
    if (blockSize == 0 || blockCount == 0) {
        return false;
    }

    if (!mountReadOnly && device.IsReadOnly()) {
        return false;
    }

    GBound = true;
    GBoundReadOnly = mountReadOnly || device.IsReadOnly();
    GBoundBlockSizeBytes = blockSize;
    GBoundBlockCount = blockCount;
    return true;
}

bool FRawBlockFileSystemDriver::ReadFile(IBlockDevice &device,
                                         const char *relativePath,
                                         void *outBuffer,
                                         Fortress::Core::uint32 outBufferBytes,
                                         Fortress::Core::uint32 &outReadBytes) {
    outReadBytes = 0u;
    if (!GBound || outBuffer == nullptr || outBufferBytes < GBoundBlockSizeBytes) {
        return false;
    }

    Fortress::Core::uint64 blockIndex = 0u;
    if (!ParseBlockPath(relativePath, blockIndex) || blockIndex >= GBoundBlockCount) {
        return false;
    }

    if (!device.ReadBlocks(blockIndex, 1u, outBuffer, GBoundBlockSizeBytes)) {
        return false;
    }

    outReadBytes = GBoundBlockSizeBytes;
    return true;
}

bool FRawBlockFileSystemDriver::WriteFile(IBlockDevice &device,
                                          const char *relativePath,
                                          const void *inBuffer,
                                          Fortress::Core::uint32 inBufferBytes,
                                          Fortress::Core::uint32 &outWrittenBytes) {
    outWrittenBytes = 0u;
    if (!GBound || GBoundReadOnly || inBuffer == nullptr || inBufferBytes < GBoundBlockSizeBytes) {
        return false;
    }

    Fortress::Core::uint64 blockIndex = 0u;
    if (!ParseBlockPath(relativePath, blockIndex) || blockIndex >= GBoundBlockCount) {
        return false;
    }

    if (!device.WriteBlocks(blockIndex, 1u, inBuffer, GBoundBlockSizeBytes)) {
        return false;
    }

    outWrittenBytes = GBoundBlockSizeBytes;
    return true;
}

bool FRawBlockFileSystemDriver::DeleteFile(IBlockDevice &device, const char *relativePath) {
    if (!GBound || GBoundReadOnly) {
        return false;
    }

    Fortress::Core::uint64 blockIndex = 0u;
    if (!ParseBlockPath(relativePath, blockIndex) || blockIndex >= GBoundBlockCount) {
        return false;
    }

    Fortress::Core::uint8 zeroBlock[4096] = {};
    if (GBoundBlockSizeBytes > sizeof(zeroBlock)) {
        return false;
    }

    for (Fortress::Core::uint32 i = 0u; i < GBoundBlockSizeBytes; i++) {
        zeroBlock[i] = 0u;
    }
    return device.WriteBlocks(blockIndex, 1u, zeroBlock, GBoundBlockSizeBytes);
}

Fortress::Core::uint32 FRawBlockFileSystemDriver::GetBoundBlockSizeBytes() const {
    return GBound ? GBoundBlockSizeBytes : 0;
}

Fortress::Core::uint64 FRawBlockFileSystemDriver::GetBoundBlockCount() const {
    return GBound ? GBoundBlockCount : 0;
}

bool FRawBlockFileSystemDriver::IsBoundReadOnly() const {
    return GBound ? GBoundReadOnly : true;
}

} // namespace Fortress::Storage
