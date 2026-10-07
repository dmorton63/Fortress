#include "Fortress/Storage/FHostShareBridgeBlockDevice.hpp"

namespace Fortress::Storage {

static Fortress::Core::uint32 UIntToAscii(Fortress::Core::uint32 value, char *out, Fortress::Core::uint32 outSize) {
    if (out == nullptr || outSize == 0u) {
        return 0u;
    }

    if (value == 0u) {
        if (outSize < 2u) {
            return 0u;
        }
        out[0] = '0';
        out[1] = '\0';
        return 1u;
    }

    char rev[16] = {};
    Fortress::Core::uint32 n = 0u;
    while (value > 0u && n < sizeof(rev)) {
        rev[n++] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    }

    if (n + 1u > outSize) {
        out[0] = '\0';
        return 0u;
    }

    for (Fortress::Core::uint32 i = 0u; i < n; i++) {
        out[i] = rev[n - i - 1u];
    }
    out[n] = '\0';
    return n;
}

bool FHostShareBridgeBlockDevice::Initialize(Fortress::Core::uint32 shareIndex) {
    GInitialized = true;
    GShareIndex = shareIndex;

    const char prefix[] = "HSHARE";
    Fortress::Core::uint32 pos = 0u;
    for (Fortress::Core::uint32 i = 0u; i + 1u < sizeof(prefix) && pos + 1u < sizeof(GDeviceName); i++) {
        GDeviceName[pos++] = prefix[i];
    }
    if (pos + 1u < sizeof(GDeviceName)) {
        GDeviceName[pos++] = '-';
    }
    pos += UIntToAscii(shareIndex, GDeviceName + pos, static_cast<Fortress::Core::uint32>(sizeof(GDeviceName) - pos));
    (void)pos;

    return true;
}

Fortress::Core::uint32 FHostShareBridgeBlockDevice::GetShareIndex() const {
    return GShareIndex;
}

Fortress::Core::uint32 FHostShareBridgeBlockDevice::GetBlockSizeBytes() const {
    return GInitialized ? BridgeBlockSizeBytes : 0u;
}

Fortress::Core::uint64 FHostShareBridgeBlockDevice::GetBlockCount() const {
    return GInitialized ? BridgeBlockCount : 0u;
}

bool FHostShareBridgeBlockDevice::IsReadOnly() const {
    return true;
}

bool FHostShareBridgeBlockDevice::ReadBlocks(Fortress::Core::uint64,
                                             Fortress::Core::uint32,
                                             void *,
                                             Fortress::Core::uint32) {
    return false;
}

bool FHostShareBridgeBlockDevice::WriteBlocks(Fortress::Core::uint64,
                                              Fortress::Core::uint32,
                                              const void *,
                                              Fortress::Core::uint32) {
    return false;
}

const char *FHostShareBridgeBlockDevice::GetDeviceName() const {
    return GInitialized ? GDeviceName : "HSHARE-UNINIT";
}

} // namespace Fortress::Storage
