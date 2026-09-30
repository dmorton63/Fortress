#include "Fortress/Kernel/FKernelRuntimeLoop.hpp"

#include "Fortress/Kernel/FDesktopRuntime.hpp"
#include "Fortress/Kernel/FCpuCoreManager.hpp"
#include "Fortress/Kernel/FEventManager.hpp"
#include "Fortress/Kernel/FKernelCommandConsole.hpp"
#include "Fortress/Kernel/FKernelCommandControlPlane.hpp"
#include "Fortress/Kernel/FKernelCoreDispatch.hpp"
#include "Fortress/Kernel/FKernelCubeScene.hpp"
#include "Fortress/Kernel/FKernelFramePipeline.hpp"
#include "Fortress/Kernel/FKernelInputEventPlane.hpp"
#include "Fortress/Kernel/FKernelRuntimeDiagnostics.hpp"
#include "Fortress/Kernel/FKernelScheduler.hpp"
#include "Fortress/Kernel/FKernelSchedulerEventPlane.hpp"
#include "Fortress/Kernel/FKernelSubsystemStateTracker.hpp"
#include "Fortress/Platform/FTimerX86.hpp"

namespace Fortress::Kernel {

namespace {

static FDesktopRuntime GDesktopRuntime = {};
static FKernelRuntimeDiagnostics GRuntimeDiagnostics = {};
static FKernelRuntimeContext *GLongOperationRuntime = nullptr;
static Fortress::Core::uint64 *GLongOperationFpsValue = nullptr;
static constexpr Fortress::Core::uint32 GCoreDispatchDrainBudgetPerTick = 8u;
static Fortress::Core::uint32 GDispatchDrainCoreId = 0u;

static void PumpLongOperationFrame() {
    if (GLongOperationRuntime == nullptr || GLongOperationFpsValue == nullptr) {
        return;
    }

    static float longOpFpsElapsed = 0.0f;
    static Fortress::Core::uint64 longOpFpsFrames = 0;

    const float deltaTime = Fortress::Platform::FTimerX86::TickSeconds();
    (void)FKernelCoreDispatch::DrainForCore(GDispatchDrainCoreId, GCoreDispatchDrainBudgetPerTick);
    FKernelScheduler::OnTick();
    (void)FEventManager::DispatchOne();
    GRuntimeDiagnostics.Tick(*GLongOperationRuntime);
    GDesktopRuntime.Tick(*GLongOperationRuntime);
    GLongOperationRuntime->CubeScene->Advance(deltaTime, FKernelCommandControlPlane::IsScenePaused());

    longOpFpsElapsed += deltaTime;
    longOpFpsFrames++;
    if (longOpFpsElapsed >= 1.0f) {
        *GLongOperationFpsValue = longOpFpsFrames;
        longOpFpsElapsed -= 1.0f;
        longOpFpsFrames = 0;
    }

    const FFrameRenderOptions options = FKernelFramePipeline::BuildFrameRenderOptions(*GLongOperationFpsValue, false);
    FKernelFramePipeline::RenderFrame(*GLongOperationRuntime, options, GDesktopRuntime);
}

} // namespace

void RunRuntimeLoop(FKernelRuntimeContext &runtime) {
    float fpsElapsed = 0.0f;
    Fortress::Core::uint64 fpsFrames = 0;
    Fortress::Core::uint64 fpsValue = 0;

    GLongOperationRuntime = &runtime;
    GLongOperationFpsValue = &fpsValue;
    GDispatchDrainCoreId = FCpuCoreManager::GetBootstrapCoreId();

    FKernelSubsystemStateTracker::Initialize(runtime.SubsystemState);
    FKernelCommandConsole::PushSystemLog("SUBSYS PHASE BOOT");
    (void)FKernelSubsystemStateTracker::TransitionTo(runtime.SubsystemState, EKernelSubsystemPhase::Init, 0u);
    FKernelCommandConsole::PushSystemLog("SUBSYS PHASE INIT");

    FKernelCommandConsole::SetLongOperationYieldCallback(&PumpLongOperationFrame);
    FKernelSchedulerEventPlane::Initialize();
    FKernelCommandControlPlane::Initialize();
    FKernelInputEventPlane::Initialize();
    const bool diagnosticsReady = GRuntimeDiagnostics.Initialize(&GDesktopRuntime);
    const bool desktopReady = GDesktopRuntime.Initialize(runtime);
    if (diagnosticsReady && desktopReady) {
        (void)FKernelSubsystemStateTracker::TransitionTo(runtime.SubsystemState, EKernelSubsystemPhase::Ready, 0u);
        FKernelCommandConsole::PushSystemLog("SUBSYS PHASE READY");
    } else {
        (void)FKernelSubsystemStateTracker::TransitionTo(runtime.SubsystemState, EKernelSubsystemPhase::Degraded, 0u);
        if (!diagnosticsReady) {
            FKernelCommandConsole::PushSystemLog("SUBSYS DEGRADED: DIAG INIT");
        }
        if (!desktopReady) {
            FKernelCommandConsole::PushSystemLog("SUBSYS DEGRADED: DESKTOP INIT");
        }
        FKernelCommandConsole::PushSystemLog("SUBSYS PHASE DEGRADED");
    }

    for (;;) {
        const float deltaTime = Fortress::Platform::FTimerX86::TickSeconds();
        FKernelCommandConsole::PollInput();
        (void)FKernelCoreDispatch::DrainForCore(GDispatchDrainCoreId, GCoreDispatchDrainBudgetPerTick);
        FKernelScheduler::OnTick();
        (void)FEventManager::DispatchOne();
        GRuntimeDiagnostics.Tick(runtime);
        GDesktopRuntime.Tick(runtime);

        runtime.CubeScene->Advance(deltaTime, FKernelCommandControlPlane::IsScenePaused());

        fpsElapsed += deltaTime;
        fpsFrames++;
        if (fpsElapsed >= 1.0f) {
            fpsValue = fpsFrames;
            fpsElapsed -= 1.0f;
            fpsFrames = 0;
        }

        const FFrameRenderOptions options = FKernelFramePipeline::BuildFrameRenderOptions(fpsValue, true);
        FKernelFramePipeline::RenderFrame(runtime, options, GDesktopRuntime);
    }
}

} // namespace Fortress::Kernel
