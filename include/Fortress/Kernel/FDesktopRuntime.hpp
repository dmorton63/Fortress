#ifndef FORTRESS_KERNEL_FDESKTOPRUNTIME_HPP
#define FORTRESS_KERNEL_FDESKTOPRUNTIME_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Kernel/FDesktopCompositor.hpp"
#include "Fortress/Kernel/FDesktopInputRouter.hpp"
#include "Fortress/Kernel/FDesktopShell.hpp"
#include "Fortress/Kernel/FDesktopShellPolicy.hpp"
#include "Fortress/Kernel/FDesktopSurfaceContentHost.hpp"
#include "Fortress/Kernel/FDesktopSurfaceOverlay.hpp"
#include "Fortress/Kernel/FEventManager.hpp"
#include "Fortress/Kernel/FKernelBootstrap.hpp"

namespace Fortress::Kernel {

class FDesktopRuntime {
  public:
    bool Initialize(FKernelRuntimeContext &runtime);
    bool IsReady() const;

    void HandleInputEvent(const FKernelEvent &event);
    void Tick(const FKernelRuntimeContext &runtime);

    FDesktopCompositor &GetCompositor();
    FDesktopInputRouter &GetInputRouter();
    FDesktopShell &GetShell();
    FDesktopSurfaceOverlay &GetOverlay();

  private:
    static void DesktopHudAttach(FDesktopCompositor &compositor);
    static void DesktopHudTick(FDesktopCompositor &compositor, Fortress::Core::uint64 tickCount);
    static void DesktopStatusAttach(FDesktopCompositor &compositor);
    static void DesktopStatusTick(FDesktopCompositor &compositor, Fortress::Core::uint64 tickCount);

    void OnDesktopHudAttach(FDesktopCompositor &compositor);
    void OnDesktopHudTick(FDesktopCompositor &compositor, Fortress::Core::uint64 tickCount);
    void OnDesktopStatusAttach(FDesktopCompositor &compositor);
    void OnDesktopStatusTick(FDesktopCompositor &compositor, Fortress::Core::uint64 tickCount);
    void CleanupOcclusionProbeSurfaces();
    void RegisterDefaultSurfaceControls(Fortress::Core::uint32 surfaceId, bool statusSurface);

    void RoutePointerSample(const FKernelRuntimeContext &runtime);
    void DrainCompositorDirtyFallback();
    void TryLogStats();

    static FDesktopRuntime *ActiveInstance;

    static constexpr Fortress::Core::uint64 DesktopStatsLogIntervalTicks = 1200u;
    Fortress::Core::uint64 DesktopTickCount = 0u;
    Fortress::Core::uint64 NextDesktopStatsTick = 2u;

    FDesktopCompositor Compositor = {};
    FDesktopShell Shell = {};
    FDesktopShellPolicy ShellPolicy = {};
    FDesktopInputRouter InputRouter = {};
    FDesktopSurfaceContentHost ContentHost = {};
    FDesktopSurfaceOverlay Overlay = {};

    Fortress::Core::uint32 DesktopHudSurfaceId = 0u;
    Fortress::Core::uint32 DesktopStatusSurfaceId = 0u;
    Fortress::Core::uint32 OcclusionProbeSurfaceAId = 0u;
    Fortress::Core::uint32 OcclusionProbeSurfaceBId = 0u;
    Fortress::Core::uint32 OcclusionProbeSurfaceCId = 0u;
    bool OcclusionProbeCleanupPending = false;
    bool Ready = false;
};

} // namespace Fortress::Kernel

#endif
