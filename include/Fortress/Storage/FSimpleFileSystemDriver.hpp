#ifndef FORTRESS_STORAGE_FSIMPLEFILESYSTEMDRIVER_HPP
#define FORTRESS_STORAGE_FSIMPLEFILESYSTEMDRIVER_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Storage/IFileSystemDriver.hpp"

namespace Fortress::Storage {

class FSimpleFileSystemDriver final : public IFileSystemDriver {
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
    struct FSuperblock {
        Fortress::Core::uint32 Magic = 0u;
        Fortress::Core::uint16 Version = 0u;
        Fortress::Core::uint16 Reserved0 = 0u;
        Fortress::Core::uint32 BlockSizeBytes = 0u;
        Fortress::Core::uint32 TotalBlocks = 0u;
        Fortress::Core::uint32 DirectoryStartBlock = 0u;
        Fortress::Core::uint32 DirectoryBlockCount = 0u;
        Fortress::Core::uint32 DataStartBlock = 0u;
        Fortress::Core::uint32 NextFreeBlock = 0u;
        Fortress::Core::uint32 MaxEntries = 0u;
        Fortress::Core::uint32 EntrySizeBytes = 0u;
        Fortress::Core::uint32 FileCount = 0u;
        Fortress::Core::uint32 Reserved1 = 0u;
        Fortress::Core::uint8 Reserved2[12] = {};
    };

    struct FDirectoryEntry {
        Fortress::Core::uint8 InUse = 0u;
        Fortress::Core::uint8 Reserved0[3] = {};
        Fortress::Core::uint32 StartBlock = 0u;
        Fortress::Core::uint32 BlockCount = 0u;
        Fortress::Core::uint32 SizeBytes = 0u;
        char Name[44] = {};
        Fortress::Core::uint8 Reserved1[4] = {};
    };

    static constexpr Fortress::Core::uint32 SuperblockMagic = 0x31465346u; // "FSF1"
    static constexpr Fortress::Core::uint16 SuperblockVersion = 1u;
    static constexpr Fortress::Core::uint32 MaxDirectoryBlocks = 32u;
    static constexpr Fortress::Core::uint32 MaxBlockSizeBytes = 4096u;

    bool LoadOrFormat(IBlockDevice &device, bool mountReadOnly);
    bool FormatEmpty(IBlockDevice &device);
    bool ReadSuperblock(IBlockDevice &device, FSuperblock &outSuperblock) const;
    bool WriteSuperblock(IBlockDevice &device, const FSuperblock &superblock) const;

    bool NormalizeRelativePath(const char *relativePath, char *outPath, Fortress::Core::uint32 outSize) const;
    bool ReadDirectoryEntry(IBlockDevice &device, Fortress::Core::uint32 entryIndex, FDirectoryEntry &outEntry) const;
    bool WriteDirectoryEntry(IBlockDevice &device, Fortress::Core::uint32 entryIndex, const FDirectoryEntry &entry) const;
    bool FindEntry(IBlockDevice &device,
                   const char *normalizedPath,
                   Fortress::Core::uint32 &outEntryIndex,
                   FDirectoryEntry &outEntry) const;
    bool FindFreeEntry(IBlockDevice &device, Fortress::Core::uint32 &outEntryIndex) const;
    Fortress::Core::uint32 ComputeHighestUsedEndBlock(IBlockDevice &device) const;

    bool WriteFileData(IBlockDevice &device,
                       Fortress::Core::uint32 startBlock,
                       const void *inBuffer,
                       Fortress::Core::uint32 inBufferBytes,
                       Fortress::Core::uint32 blockCount) const;
    bool ReadFileData(IBlockDevice &device,
                      Fortress::Core::uint32 startBlock,
                      void *outBuffer,
                      Fortress::Core::uint32 outBufferBytes,
                      Fortress::Core::uint32 sizeBytes,
                      Fortress::Core::uint32 blockCount) const;

    bool GBound = false;
    bool GBoundReadOnly = true;
    FSuperblock GSuperblock = {};
};

} // namespace Fortress::Storage

#endif