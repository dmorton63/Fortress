#ifndef FORTRESS_STORAGE_IFILESYSTEMDRIVER_HPP
#define FORTRESS_STORAGE_IFILESYSTEMDRIVER_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Storage {

class IBlockDevice;

class IFileSystemDriver {
  public:
    virtual const char *GetDriverName() const = 0;
    virtual bool ProbeAndBind(IBlockDevice &device, bool mountReadOnly) = 0;
    virtual bool ReadFile(IBlockDevice &device,
                          const char *relativePath,
                          void *outBuffer,
                          Fortress::Core::uint32 outBufferBytes,
                          Fortress::Core::uint32 &outReadBytes) = 0;
    virtual bool WriteFile(IBlockDevice &device,
                           const char *relativePath,
                           const void *inBuffer,
                           Fortress::Core::uint32 inBufferBytes,
                           Fortress::Core::uint32 &outWrittenBytes) = 0;
    virtual bool DeleteFile(IBlockDevice &device, const char *relativePath) = 0;

  protected:
    ~IFileSystemDriver() = default;
};

} // namespace Fortress::Storage

#endif