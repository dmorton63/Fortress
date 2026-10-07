#ifndef FORTRESS_STORAGE_FHOSTSHAREBRIDGEBLOCKDEVICE_HPP
#define FORTRESS_STORAGE_FHOSTSHAREBRIDGEBLOCKDEVICE_HPP

#include "Fortress/Storage/IBlockDevice.hpp"

namespace Fortress::Storage {

class FHostShareBridgeBlockDevice final : public IBlockDevice {
  public:
    bool Initialize(Fortress::Core::uint32 shareIndex);
    Fortress::Core::uint32 GetShareIndex() const;

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
    static constexpr Fortress::Core::uint32 BridgeBlockSizeBytes = 4096u;
    static constexpr Fortress::Core::uint64 BridgeBlockCount = 1u;

    bool GInitialized = false;
    Fortress::Core::uint32 GShareIndex = 0u;
    char GDeviceName[32] = {};
};

} // namespace Fortress::Storage

#endif
