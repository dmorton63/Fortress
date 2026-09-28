#include "Fortress/Kernel/FDesktopShell.hpp"

namespace Fortress::Kernel {

static bool StrEq(const char *a, const char *b) {
    if (a == nullptr || b == nullptr) {
        return false;
    }

    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return false;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

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

bool FDesktopShell::IsComponentRegistered(const char *name) const {
    if (!Ready || name == nullptr || name[0] == '\0') {
        return false;
    }

    for (Fortress::Core::uint32 i = 0; i < ComponentCount; i++) {
        if (StrEq(Components[i].Name, name)) {
            return true;
        }
    }

    return false;
}

bool FDesktopShell::RegisterComponent(const FDesktopShellComponent &component) {
    if (!Ready || ComponentCount >= MaxComponents || component.Name == nullptr || component.Name[0] == '\0' ||
        IsComponentRegistered(component.Name)) {
        return false;
    }

    Components[ComponentCount++] = component;
    if (component.OnAttach != nullptr) {
        component.OnAttach(*Compositor);
    }

    return true;
}

bool FDesktopShell::UnregisterComponent(const char *name) {
    if (!Ready || name == nullptr || name[0] == '\0') {
        return false;
    }

    for (Fortress::Core::uint32 i = 0; i < ComponentCount; i++) {
        if (!StrEq(Components[i].Name, name)) {
            continue;
        }

        for (Fortress::Core::uint32 j = i + 1; j < ComponentCount; j++) {
            Components[j - 1u] = Components[j];
        }
        if (ComponentCount > 0u) {
            ComponentCount--;
            Components[ComponentCount] = FDesktopShellComponent{};
        }
        return true;
    }

    return false;
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