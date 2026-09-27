#ifndef FORTRESS_STORAGE_FRAMBLOCKDEVICE_HPP
#define FORTRESS_STORAGE_FRAMBLOCKDEVICE_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Storage/IBlockDevice.hpp"

namespace Fortress::Storage {

class FRamBlockDevice final : public IBlockDevice {
  public:
    static constexpr Fortress::Core::uint32 SupportedBlockSizeBytes = 512u;
    static constexpr Fortress::Core::uint32 MaxBlocks = 4096u;

    bool Initialize(Fortress::Core::uint32 blockSizeBytes,
                    Fortress::Core::uint32 blockCount,
                    bool readOnly);

    Fortress::Core::uint32 GetBlockSizeBytes() const override;
    Fortress::Core::uint64 GetBlockCount() const override;
    bool IsReadOnly() const override;

    bool ReadBlocks(Fortress::Core::uint64 startBlock,
                    Fortress::Core::uint32 blockCount,
                    void *outBuffer,
                    Fortress::Core::uint32 outBufferBytes) override;
    bool WriteBlocks(Fortress::Core::uint64 startBlock,
                     Fortress::Core::uint32 blockCount,
                     const void *inBuffer,
                     Fortress::Core::uint32 inBufferBytes) override;

    const char *GetDeviceName() const override;

  private:
    bool GInitialized = false;
    bool GReadOnly = false;
    Fortress::Core::uint32 GBlockSizeBytes = SupportedBlockSizeBytes;
    Fortress::Core::uint32 GBlockCount = 0;
    Fortress::Core::uint8 GStorage[SupportedBlockSizeBytes * MaxBlocks] = {};
};

} // namespace Fortress::Storage

#endif