#ifndef FORTRESS_KERNEL_FDESKTOPSHELLPOLICY_HPP
#define FORTRESS_KERNEL_FDESKTOPSHELLPOLICY_HPP

namespace Fortress::Kernel {

class FDesktopShellPolicy {
  public:
    void ResetDefaults();

    bool ShouldAssignInitialFocusOnInitialize() const;
    bool ShouldRoutePointerFocus() const;
    bool ShouldRaiseOnFocusChange() const;
    bool ShouldCaptureOnPointerFocus() const;
    bool ShouldReleaseCaptureOnPointerRelease() const;

  private:
    bool AssignInitialFocusOnInitialize = true;
    bool RoutePointerFocus = true;
    bool RaiseOnFocusChange = true;
    bool CaptureOnPointerFocus = true;
    bool ReleaseCaptureOnPointerRelease = true;
};

} // namespace Fortress::Kernel

#endif
