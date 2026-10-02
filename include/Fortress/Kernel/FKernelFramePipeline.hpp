#ifndef FORTRESS_KERNEL_FKERNELFRAMEPIPELINE_HPP
#define FORTRESS_KERNEL_FKERNELFRAMEPIPELINE_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Kernel/FDesktopRuntime.hpp"
#include "Fortress/Kernel/FKernelBootstrap.hpp"

namespace Fortress::Kernel {

struct FFrameRenderOptions {
    Fortress::Core::uint64 FpsValue = 0;
  bool RenderScene = true;
    bool WireframeEnabled = false;
    bool RenderSurfaceSelfTest = false;
    bool RenderDesktopSurfaces = true;
    bool RenderHud = true;
    bool RenderInputPulse = false;
    bool RenderCursorOverlay = true;
    bool ConsumeButtonEdges = false;
};

class FKernelFramePipeline {
  public:
    static FFrameRenderOptions BuildFrameRenderOptions(Fortress::Core::uint64 fpsValue, bool consumeButtonEdges);

    static void RenderFrame(FKernelRuntimeContext &runtime,
                            const FFrameRenderOptions &options,
                            FDesktopRuntime &desktopRuntime);
};

} // namespace Fortress::Kernel

#endif
