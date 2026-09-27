#ifndef FORTRESS_KERNEL_FKERNELHUDOVERLAY_HPP
#define FORTRESS_KERNEL_FKERNELHUDOVERLAY_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Memory/FKernelHeap.hpp"
#include "Fortress/Memory/FPhysicalMemoryManager.hpp"
#include "Fortress/Memory/FVirtualMemoryManager.hpp"

namespace Fortress::Video {
class FVideoConsole;
struct FVideoSurfaceView;
}

namespace Fortress::Kernel {

class FKernelHudOverlay {
  public:
    static void Render(Fortress::Video::FVideoConsole &console,
                       const Fortress::Video::FVideoSurfaceView &surface,
                       Fortress::Core::uint64 fpsValue,
                       bool wireframeEnabled,
                       const Fortress::Memory::FPhysicalMemoryStats &pmmStats,
                       const Fortress::Memory::FVirtualMemoryStats &vmmStats,
                       const Fortress::Memory::FKernelHeapStats &heapStats,
                       Fortress::Core::uint64 arenaUsedBytes);
};

} // namespace Fortress::Kernel

#endif
