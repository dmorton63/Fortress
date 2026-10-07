#ifndef FORTRESS_STORAGE_FHOSTSHAREFILESYSTEMDRIVER_HPP
#define FORTRESS_STORAGE_FHOSTSHAREFILESYSTEMDRIVER_HPP

#include "Fortress/Storage/IFileSystemDriver.hpp"

namespace Fortress::Storage {

class FHostShareFileSystemDriver final : public IFileSystemDriver {
  public:
    const char *GetDriverName() const override;
    bool ProbeAndBind(IBlockDevice &device, bool mountReadOnly) override;
    bool ReadFile(IBlockDevice &device,
                  const char *relativePath,
                  void *outBuffer,
                  Fortress::Core::uint32 outBufferBytes,
                  Fortress::Core::uint32 &outReadBytes) override;
    bool WriteFile(IBlockDevice &device,
                   const char *relativePath,
                   const void *inBuffer,
                   Fortress::Core::uint32 inBufferBytes,
                   Fortress::Core::uint32 &outWrittenBytes) override;
    bool DeleteFile(IBlockDevice &device, const char *relativePath) override;

  private:
    bool NormalizeRelativePath(const char *relativePath,
                               char *outPath,
                               Fortress::Core::uint32 outSize) const;

    bool GBound = false;
    bool GMountReadOnly = true;
    Fortress::Core::uint32 GShareIndex = 0u;
};

} // namespace Fortress::Storage

#endif
