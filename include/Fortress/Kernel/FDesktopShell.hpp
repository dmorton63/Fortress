#ifndef FORTRESS_KERNEL_FDESKTOPSHELL_HPP
#define FORTRESS_KERNEL_FDESKTOPSHELL_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Kernel/FDesktopCompositor.hpp"

namespace Fortress::Kernel {

struct FDesktopShellComponent {
    const char *Name = "";
    void (*OnAttach)(FDesktopCompositor &compositor) = nullptr;
    void (*OnTick)(FDesktopCompositor &compositor, Fortress::Core::uint64 tickCount) = nullptr;
};

struct FDesktopShellStats {
    Fortress::Core::uint32 ComponentCount = 0;
    Fortress::Core::uint64 TickCount = 0;
};

class FDesktopShell {
  public:
    bool Initialize(FDesktopCompositor *compositor);
    bool IsReady() const;

    bool IsComponentRegistered(const char *name) const;
    bool RegisterComponent(const FDesktopShellComponent &component);
    bool UnregisterComponent(const char *name);
    void Tick();
    void GetStats(FDesktopShellStats &outStats) const;

  private:
    static constexpr Fortress::Core::uint32 MaxComponents = 8u;

    FDesktopCompositor *Compositor = nullptr;
    FDesktopShellComponent Components[MaxComponents] = {};
    Fortress::Core::uint32 ComponentCount = 0;
    Fortress::Core::uint64 TickCount = 0;
    bool Ready = false;
};

} // namespace Fortress::Kernel

#endif