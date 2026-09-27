#ifndef FORTRESS_STORAGE_IBLOCKDEVICE_HPP
#define FORTRESS_STORAGE_IBLOCKDEVICE_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Storage {

class IBlockDevice {
  public:
    virtual Fortress::Core::uint32 GetBlockSizeBytes() const = 0;
    virtual Fortress::Core::uint64 GetBlockCount() const = 0;
    virtual bool IsReadOnly() const = 0;

    virtual bool ReadBlocks(Fortress::Core::uint64 startBlock,
                            Fortress::Core::uint32 blockCount,
                            void *outBuffer,
                            Fortress::Core::uint32 outBufferBytes) = 0;
    virtual bool WriteBlocks(Fortress::Core::uint64 startBlock,
                             Fortress::Core::uint32 blockCount,
                             const void *inBuffer,
                             Fortress::Core::uint32 inBufferBytes) = 0;

    virtual const char *GetDeviceName() const = 0;

  protected:
    ~IBlockDevice() = default;
};

} // namespace Fortress::Storage

#endif