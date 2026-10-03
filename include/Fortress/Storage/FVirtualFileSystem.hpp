#ifndef FORTRESS_STORAGE_FVIRTUALFILESYSTEM_HPP
#define FORTRESS_STORAGE_FVIRTUALFILESYSTEM_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Storage {

class IBlockDevice;
class IFileSystemDriver;

struct FVirtualFileSystemRoute {
    bool Found = false;
    bool ReadOnly = false;
    const char *DriverName = "";
    IFileSystemDriver *Driver = nullptr;
    IBlockDevice *Device = nullptr;
    char RelativePath[96] = {};
};

struct FVirtualFileSystemStats {
    Fortress::Core::uint32 MountCount = 0;
    Fortress::Core::uint32 Capacity = 0;
};

struct FVirtualFileSystemMountSnapshot {
    char MountPath[32] = {};
    bool ReadOnly = false;
    const char *DriverName = "";
};

class FVirtualFileSystem {
  public:
    static bool Initialize();
    static bool Mount(const char *mountPath,
                      IBlockDevice *device,
                      IFileSystemDriver *driver,
                      bool readOnly);
    static bool ResolvePath(const char *absolutePath, FVirtualFileSystemRoute &outRoute);
    static bool ReadFile(const char *absolutePath,
                         void *outBuffer,
                         Fortress::Core::uint32 outBufferBytes,
                         Fortress::Core::uint32 &outReadBytes);
    static bool WriteFile(const char *absolutePath,
                          const void *inBuffer,
                          Fortress::Core::uint32 inBufferBytes,
                          Fortress::Core::uint32 &outWrittenBytes);
    static bool DeleteFile(const char *absolutePath);
    static void GetMounts(FVirtualFileSystemMountSnapshot *outMounts,
                          Fortress::Core::uint32 capacity,
                          Fortress::Core::uint32 &outCount);
    static void GetStats(FVirtualFileSystemStats &outStats);
};

} // namespace Fortress::Storage

#endif