#include "Fortress/Kernel/FDesktopShellPolicy.hpp"

namespace Fortress::Kernel {

void FDesktopShellPolicy::ResetDefaults() {
    AssignInitialFocusOnInitialize = true;
    RoutePointerFocus = true;
    RaiseOnFocusChange = true;
    CaptureOnPointerFocus = true;
    ReleaseCaptureOnPointerRelease = true;
}

bool FDesktopShellPolicy::ShouldAssignInitialFocusOnInitialize() const {
    return AssignInitialFocusOnInitialize;
}

bool FDesktopShellPolicy::ShouldRoutePointerFocus() const {
    return RoutePointerFocus;
}

bool FDesktopShellPolicy::ShouldRaiseOnFocusChange() const {
    return RaiseOnFocusChange;
}

bool FDesktopShellPolicy::ShouldCaptureOnPointerFocus() const {
    return CaptureOnPointerFocus;
}

bool FDesktopShellPolicy::ShouldReleaseCaptureOnPointerRelease() const {
    return ReleaseCaptureOnPointerRelease;
}

} // namespace Fortress::Kernel
