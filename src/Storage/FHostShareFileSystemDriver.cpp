#include "Fortress/Storage/FHostShareFileSystemDriver.hpp"

#include "Fortress/Kernel/FKernelCommandConsole.hpp"
#include "Fortress/Storage/FHostShareBridgeBlockDevice.hpp"
#include "Fortress/Storage/IBlockDevice.hpp"

namespace Fortress::Storage {

static bool StartsWith(const char *value, const char *prefix) {
    if (value == nullptr || prefix == nullptr) {
        return false;
    }

    Fortress::Core::uint32 i = 0u;
    while (prefix[i] != '\0') {
        if (value[i] != prefix[i]) {
            return false;
        }
        i++;
    }
    return true;
}

const char *FHostShareFileSystemDriver::GetDriverName() const {
    return "HS9P";
}

bool FHostShareFileSystemDriver::ProbeAndBind(IBlockDevice &device, bool mountReadOnly) {
    const char *deviceName = device.GetDeviceName();
    if (!StartsWith(deviceName, "HSHARE-")) {
        return false;
    }

    const auto &bridgeDevice = static_cast<const FHostShareBridgeBlockDevice &>(device);
    GMountReadOnly = mountReadOnly;
    GShareIndex = bridgeDevice.GetShareIndex();
    GBound = true;
    return true;
}

bool FHostShareFileSystemDriver::NormalizeRelativePath(const char *relativePath,
                                                       char *outPath,
                                                       Fortress::Core::uint32 outSize) const {
    if (relativePath == nullptr || outPath == nullptr || outSize < 2u) {
        return false;
    }

    Fortress::Core::uint32 src = (relativePath[0] == '/') ? 1u : 0u;
    Fortress::Core::uint32 dst = 0u;

    while (relativePath[src] != '\0') {
        const char c = relativePath[src++];
        if (c == '\\') {
            return false;
        }
        const bool allowed =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
            c == '.' || c == '/';
        if (!allowed) {
            return false;
        }
        if (dst + 1u >= outSize) {
            return false;
        }
        outPath[dst++] = c;
    }

    if (dst == 0u) {
        outPath[0] = '.';
        outPath[1] = '\0';
        return true;
    }

    outPath[dst] = '\0';
    return true;
}

bool FHostShareFileSystemDriver::ReadFile(IBlockDevice &,
                                          const char *relativePath,
                                          void *outBuffer,
                                          Fortress::Core::uint32 outBufferBytes,
                                          Fortress::Core::uint32 &outReadBytes) {
    outReadBytes = 0u;
    if (!GBound || outBuffer == nullptr || outBufferBytes == 0u) {
        return false;
    }

    char normalizedPath[96] = {};
    if (!NormalizeRelativePath(relativePath, normalizedPath, sizeof(normalizedPath))) {
        return false;
    }

    return Fortress::Kernel::FKernelCommandConsole::TryReadHostShareFile(
        GShareIndex, normalizedPath, outBuffer, outBufferBytes, outReadBytes);
}

bool FHostShareFileSystemDriver::WriteFile(IBlockDevice &,
                                           const char *relativePath,
                                           const void *inBuffer,
                                           Fortress::Core::uint32 inBufferBytes,
                                           Fortress::Core::uint32 &outWrittenBytes) {
    outWrittenBytes = 0u;
    if (!GBound || GMountReadOnly || inBuffer == nullptr || inBufferBytes == 0u) {
        return false;
    }

    char normalizedPath[96] = {};
    if (!NormalizeRelativePath(relativePath, normalizedPath, sizeof(normalizedPath))) {
        return false;
    }

    return Fortress::Kernel::FKernelCommandConsole::TryWriteHostShareFile(
        GShareIndex, normalizedPath, inBuffer, inBufferBytes, outWrittenBytes);
}

bool FHostShareFileSystemDriver::DeleteFile(IBlockDevice &, const char *) {
    return false;
}

} // namespace Fortress::Storage
