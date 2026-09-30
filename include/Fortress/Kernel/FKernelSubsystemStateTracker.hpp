#ifndef FORTRESS_KERNEL_FKERNELSUBSYSTEMSTATETRACKER_HPP
#define FORTRESS_KERNEL_FKERNELSUBSYSTEMSTATETRACKER_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

enum class EKernelSubsystemPhase : Fortress::Core::uint8 {
    Boot = 0,
    Init,
    Ready,
    Degraded,
};

struct FKernelSubsystemRuntimeState {
    EKernelSubsystemPhase Phase = EKernelSubsystemPhase::Boot;
    Fortress::Core::uint32 TransitionCount = 0;
    Fortress::Core::uint32 DegradedTransitionCount = 0;
    Fortress::Core::uint64 LastTransitionTick = 0;
};

class FKernelSubsystemStateTracker {
  public:
    static void Initialize(FKernelSubsystemRuntimeState &state);
    static bool TransitionTo(FKernelSubsystemRuntimeState &state,
                             EKernelSubsystemPhase nextPhase,
                             Fortress::Core::uint64 transitionTick);
    static const char *GetPhaseName(EKernelSubsystemPhase phase);
};

} // namespace Fortress::Kernel

#endif
