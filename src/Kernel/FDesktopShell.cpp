#include "Fortress/Kernel/FDesktopShell.hpp"

namespace Fortress::Kernel {

bool FDesktopShell::Initialize(FDesktopCompositor *compositor) {
    if (compositor == nullptr || !compositor->IsReady()) {
        return false;
    }

    Compositor = compositor;
    for (Fortress::Core::uint32 i = 0; i < MaxComponents; i++) {
        Components[i] = FDesktopShellComponent{};
    }
    ComponentCount = 0;
    TickCount = 0;
    Ready = true;
    return true;
}

bool FDesktopShell::IsReady() const {
    return Ready;
}

bool FDesktopShell::RegisterComponent(const FDesktopShellComponent &component) {
    if (!Ready || ComponentCount >= MaxComponents) {
        return false;
    }

    Components[ComponentCount++] = component;
    if (component.OnAttach != nullptr) {
        component.OnAttach(*Compositor);
    }

    return true;
}

void FDesktopShell::Tick() {
    if (!Ready || Compositor == nullptr) {
        return;
    }

    TickCount++;
    for (Fortress::Core::uint32 i = 0; i < ComponentCount; i++) {
        if (Components[i].OnTick != nullptr) {
            Components[i].OnTick(*Compositor, TickCount);
        }
    }
}

void FDesktopShell::GetStats(FDesktopShellStats &outStats) const {
    outStats = FDesktopShellStats{
        .ComponentCount = ComponentCount,
        .TickCount = TickCount,
    };
}

} // namespace Fortress::Kernel