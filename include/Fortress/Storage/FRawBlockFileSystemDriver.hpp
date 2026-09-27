#ifndef FORTRESS_STORAGE_FRAWBLOCKFILESYSTEMDRIVER_HPP
#define FORTRESS_STORAGE_FRAWBLOCKFILESYSTEMDRIVER_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Storage/IFileSystemDriver.hpp"

namespace Fortress::Storage {

class FRawBlockFileSystemDriver final : public IFileSystemDriver {
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

    Fortress::Core::uint32 GetBoundBlockSizeBytes() const;
    Fortress::Core::uint64 GetBoundBlockCount() const;
    bool IsBoundReadOnly() const;

  private:
    bool GBound = false;
    bool GBoundReadOnly = false;
    Fortress::Core::uint32 GBoundBlockSizeBytes = 0;
    Fortress::Core::uint64 GBoundBlockCount = 0;
};

} // namespace Fortress::Storage

#endif