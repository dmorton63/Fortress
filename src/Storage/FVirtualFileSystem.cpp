#include "Fortress/Storage/FVirtualFileSystem.hpp"

#include "Fortress/Storage/IFileSystemDriver.hpp"
#include "Fortress/Storage/IBlockDevice.hpp"

namespace Fortress::Storage {

static constexpr Fortress::Core::uint32 GMountCapacity = 8u;
static constexpr Fortress::Core::uint32 GMaxMountPath = 31u;

struct FMountSlot {
    bool InUse = false;
    bool ReadOnly = false;
    IBlockDevice *Device = nullptr;
    IFileSystemDriver *Driver = nullptr;
    char MountPath[GMaxMountPath + 1] = {};
};

static bool GInitialized = false;
static FMountSlot GMounts[GMountCapacity] = {};

static Fortress::Core::uint32 StringLength(const char *value, Fortress::Core::uint32 maxLen) {
    if (value == nullptr) {
        return 0;
    }

    Fortress::Core::uint32 len = 0;
    while (len < maxLen && value[len] != '\0') {
        len++;
    }
    return len;
}

static bool CopyStringBounded(char *dst,
                              Fortress::Core::uint32 dstSize,
                              const char *src,
                              Fortress::Core::uint32 srcLen) {
    if (dst == nullptr || dstSize == 0 || src == nullptr || srcLen + 1 > dstSize) {
        return false;
    }

    for (Fortress::Core::uint32 i = 0; i < srcLen; i++) {
        dst[i] = src[i];
    }
    dst[srcLen] = '\0';
    return true;
}

static bool IsMountBoundary(const char *absolutePath, Fortress::Core::uint32 mountLen) {
    if (absolutePath == nullptr) {
        return false;
    }

    const char next = absolutePath[mountLen];
    return next == '\0' || next == '/';
}

static bool PrefixMatchesAtBoundary(const char *absolutePath,
                                    const char *mountPath,
                                    Fortress::Core::uint32 mountLen) {
    if (absolutePath == nullptr || mountPath == nullptr) {
        return false;
    }

    for (Fortress::Core::uint32 i = 0; i < mountLen; i++) {
        if (absolutePath[i] != mountPath[i]) {
            return false;
        }
    }

    return IsMountBoundary(absolutePath, mountLen);
}

static Fortress::Core::uint32 CountMounts() {
    Fortress::Core::uint32 count = 0;
    for (Fortress::Core::uint32 i = 0; i < GMountCapacity; i++) {
        if (GMounts[i].InUse) {
            count++;
        }
    }
    return count;
}

bool FVirtualFileSystem::Initialize() {
    GInitialized = true;
    for (Fortress::Core::uint32 i = 0; i < GMountCapacity; i++) {
        GMounts[i] = FMountSlot{};
    }
    return true;
}

bool FVirtualFileSystem::Mount(const char *mountPath,
                               IBlockDevice *device,
                               IFileSystemDriver *driver,
                               bool readOnly) {
    if (!GInitialized || mountPath == nullptr || mountPath[0] != '/' || device == nullptr || driver == nullptr) {
        return false;
    }

    const Fortress::Core::uint32 mountLen = StringLength(mountPath, GMaxMountPath + 1);
    if (mountLen == 0 || mountLen > GMaxMountPath || mountPath[mountLen] != '\0') {
        return false;
    }

    for (Fortress::Core::uint32 i = 0; i < GMountCapacity; i++) {
        if (!GMounts[i].InUse) {
            continue;
        }

        const Fortress::Core::uint32 existingLen = StringLength(GMounts[i].MountPath, GMaxMountPath);
        if (existingLen != mountLen) {
            continue;
        }

        bool same = true;
        for (Fortress::Core::uint32 j = 0; j < mountLen; j++) {
            if (GMounts[i].MountPath[j] != mountPath[j]) {
                same = false;
                break;
            }
        }
        if (same) {
            return false;
        }
    }

    if (!driver->ProbeAndBind(*device, readOnly)) {
        return false;
    }

    for (Fortress::Core::uint32 i = 0; i < GMountCapacity; i++) {
        if (GMounts[i].InUse) {
            continue;
        }

        if (!CopyStringBounded(GMounts[i].MountPath, GMaxMountPath + 1, mountPath, mountLen)) {
            return false;
        }

        GMounts[i] = FMountSlot{
            .InUse = true,
            .ReadOnly = readOnly,
            .Device = device,
            .Driver = driver,
        };
        CopyStringBounded(GMounts[i].MountPath, GMaxMountPath + 1, mountPath, mountLen);
        return true;
    }

    return false;
}

bool FVirtualFileSystem::ResolvePath(const char *absolutePath, FVirtualFileSystemRoute &outRoute) {
    outRoute = FVirtualFileSystemRoute{};

    if (!GInitialized || absolutePath == nullptr || absolutePath[0] != '/') {
        return false;
    }

    Fortress::Core::uint32 bestIndex = GMountCapacity;
    Fortress::Core::uint32 bestLen = 0;

    for (Fortress::Core::uint32 i = 0; i < GMountCapacity; i++) {
        if (!GMounts[i].InUse) {
            continue;
        }

        const Fortress::Core::uint32 mountLen = StringLength(GMounts[i].MountPath, GMaxMountPath);
        if (mountLen == 0 || mountLen < bestLen) {
            continue;
        }

        if (PrefixMatchesAtBoundary(absolutePath, GMounts[i].MountPath, mountLen)) {
            bestIndex = i;
            bestLen = mountLen;
        }
    }

    if (bestIndex >= GMountCapacity) {
        return false;
    }

    const FMountSlot &slot = GMounts[bestIndex];
    const char *relative = absolutePath + bestLen;
    if (*relative == '/') {
        relative++;
    }

    const Fortress::Core::uint32 relLen = StringLength(relative, static_cast<Fortress::Core::uint32>(sizeof(outRoute.RelativePath)));
    if (relative[relLen] != '\0') {
        return false;
    }

    if (relLen == 0) {
        outRoute.RelativePath[0] = '/';
        outRoute.RelativePath[1] = '\0';
    } else {
        if (!CopyStringBounded(outRoute.RelativePath,
                               static_cast<Fortress::Core::uint32>(sizeof(outRoute.RelativePath)),
                               relative,
                               relLen)) {
            return false;
        }
    }

    outRoute.Found = true;
    outRoute.ReadOnly = slot.ReadOnly;
    outRoute.Driver = slot.Driver;
    outRoute.DriverName = (slot.Driver == nullptr) ? "" : slot.Driver->GetDriverName();
    outRoute.Device = slot.Device;
    return true;
}

bool FVirtualFileSystem::ReadFile(const char *absolutePath,
                                  void *outBuffer,
                                  Fortress::Core::uint32 outBufferBytes,
                                  Fortress::Core::uint32 &outReadBytes) {
    outReadBytes = 0u;
    FVirtualFileSystemRoute route{};
    if (!ResolvePath(absolutePath, route) || !route.Found || route.Driver == nullptr || route.Device == nullptr) {
        return false;
    }

    return route.Driver->ReadFile(*route.Device,
                                  route.RelativePath,
                                  outBuffer,
                                  outBufferBytes,
                                  outReadBytes);
}

bool FVirtualFileSystem::WriteFile(const char *absolutePath,
                                   const void *inBuffer,
                                   Fortress::Core::uint32 inBufferBytes,
                                   Fortress::Core::uint32 &outWrittenBytes) {
    outWrittenBytes = 0u;
    FVirtualFileSystemRoute route{};
    if (!ResolvePath(absolutePath, route) || !route.Found || route.Driver == nullptr || route.Device == nullptr) {
        return false;
    }

    if (route.ReadOnly) {
        return false;
    }

    return route.Driver->WriteFile(*route.Device,
                                   route.RelativePath,
                                   inBuffer,
                                   inBufferBytes,
                                   outWrittenBytes);
}

void FVirtualFileSystem::GetMounts(FVirtualFileSystemMountSnapshot *outMounts,
                                   Fortress::Core::uint32 capacity,
                                   Fortress::Core::uint32 &outCount) {
    outCount = 0u;
    if (!GInitialized || outMounts == nullptr || capacity == 0u) {
        return;
    }

    for (Fortress::Core::uint32 i = 0; i < GMountCapacity; i++) {
        if (!GMounts[i].InUse) {
            continue;
        }

        if (outCount >= capacity) {
            break;
        }

        outMounts[outCount] = FVirtualFileSystemMountSnapshot{
            .ReadOnly = GMounts[i].ReadOnly,
            .DriverName = (GMounts[i].Driver == nullptr) ? "" : GMounts[i].Driver->GetDriverName(),
        };

        const Fortress::Core::uint32 mountLen = StringLength(GMounts[i].MountPath, GMaxMountPath);
        (void)CopyStringBounded(outMounts[outCount].MountPath,
                                static_cast<Fortress::Core::uint32>(sizeof(outMounts[outCount].MountPath)),
                                GMounts[i].MountPath,
                                mountLen);
        outCount++;
    }
}

void FVirtualFileSystem::GetStats(FVirtualFileSystemStats &outStats) {
    outStats = FVirtualFileSystemStats{
        .MountCount = CountMounts(),
        .Capacity = GMountCapacity,
    };
}

} // namespace Fortress::Storage