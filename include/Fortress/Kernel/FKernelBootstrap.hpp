#ifndef FORTRESS_KERNEL_FKERNELBOOTSTRAP_HPP
#define FORTRESS_KERNEL_FKERNELBOOTSTRAP_HPP

#include "limine.h"

#include "Fortress/Kernel/FKernelSubsystemStateTracker.hpp"

namespace Fortress::Video {
class FDisplayDevice;
class FDisplayManager;
class FRenderer3D;
class FVideoDevice;
class FVideoConsole;
}

namespace Fortress::Kernel {
class FKernelCubeScene;

struct FKernelRuntimeContext {
  Fortress::Video::FDisplayManager *DisplayManager;
  Fortress::Video::FDisplayDevice *DisplayDevice;
    Fortress::Video::FVideoDevice *VideoDevice;
    Fortress::Video::FVideoConsole *Console;
    Fortress::Video::FRenderer3D *Renderer3D;
    Fortress::Kernel::FKernelCubeScene *CubeScene;
    Fortress::Kernel::FKernelSubsystemRuntimeState SubsystemState;
};

class FKernelBootstrap {
  public:
    static bool Initialize(const limine_framebuffer_response *framebufferResponse,
                           const limine_memmap_response *memmapResponse,
                           const limine_hhdm_response *hhdmResponse,
                           const limine_smp_response *mpResponse,
                           FKernelRuntimeContext &outContext);

    static void HaltForever();
    static void EnableFPUAndSSE();
};

} // namespace Fortress::Kernel

#endif
