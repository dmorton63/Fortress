#include "Fortress/Kernel/FKernelSubsystemStateTracker.hpp"

namespace Fortress::Kernel {

void FKernelSubsystemStateTracker::Initialize(FKernelSubsystemRuntimeState &state) {
    state = FKernelSubsystemRuntimeState{};
}

bool FKernelSubsystemStateTracker::TransitionTo(FKernelSubsystemRuntimeState &state,
                                                EKernelSubsystemPhase nextPhase,
                                                Fortress::Core::uint64 transitionTick) {
    if (state.Phase == nextPhase) {
        return false;
    }

    state.Phase = nextPhase;
    state.TransitionCount++;
    state.LastTransitionTick = transitionTick;
    if (nextPhase == EKernelSubsystemPhase::Degraded) {
        state.DegradedTransitionCount++;
    }

    return true;
}

const char *FKernelSubsystemStateTracker::GetPhaseName(EKernelSubsystemPhase phase) {
    switch (phase) {
    case EKernelSubsystemPhase::Boot:
        return "BOOT";
    case EKernelSubsystemPhase::Init:
        return "INIT";
    case EKernelSubsystemPhase::Ready:
        return "READY";
    case EKernelSubsystemPhase::Degraded:
        return "DEGRADED";
    default:
        return "UNKNOWN";
    }
}

} // namespace Fortress::Kernel
