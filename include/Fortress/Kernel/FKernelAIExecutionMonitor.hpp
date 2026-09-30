#ifndef FORTRESS_KERNEL_FKERNELAIEXECUTIONMONITOR_HPP
#define FORTRESS_KERNEL_FKERNELAIEXECUTIONMONITOR_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

enum class EKernelAIExecutionAction : Fortress::Core::uint8 {
    NoAction = 0,
    Alert,
    Throttle,
    Isolate,
    Restart,
};

enum class EKernelAIBuiltInPolicyMode : Fortress::Core::uint8 {
    NoOp = 0,
    Alert,
    Throttle,
    Isolate,
    Restart,
};

struct FKernelAIExecutionTelemetry {
    Fortress::Core::uint64 TickCount = 0;
    Fortress::Core::uint32 SchedulerReadyDepth = 0;
    Fortress::Core::uint32 EventQueueDepth = 0;
    Fortress::Core::uint64 DeniedCapabilityChecks = 0;
};

struct FKernelAIPolicyThresholds {
    Fortress::Core::uint64 AlertDeniedMin = 1;
    Fortress::Core::uint32 AlertEventQueueMin = 64;
    Fortress::Core::uint32 ThrottleEventQueueMin = 128;
    Fortress::Core::uint64 IsolateDeniedMin = 32;
    Fortress::Core::uint32 IsolateEventQueueMin = 256;
    Fortress::Core::uint32 RestartEventQueueMin = 64;
    Fortress::Core::uint64 RestartDeniedMin = 64;
};

struct FKernelAIPolicyStateSnapshot {
    EKernelAIBuiltInPolicyMode Mode = EKernelAIBuiltInPolicyMode::NoOp;
    FKernelAIPolicyThresholds Thresholds = {};
};

using FKernelAIExecutionPolicyEvaluator = EKernelAIExecutionAction (*)(const FKernelAIExecutionTelemetry &telemetry,
                                                                        void *context);

class FKernelAIExecutionMonitor {
  public:
    static bool Initialize();
    static void SetPolicyEvaluator(FKernelAIExecutionPolicyEvaluator evaluator, void *context);
        static bool IsPolicyEvaluatorInstalled();
        static void SetBuiltInPolicyMode(EKernelAIBuiltInPolicyMode mode);
        static EKernelAIBuiltInPolicyMode GetBuiltInPolicyMode();
        static const char *GetBuiltInPolicyModeName();
        static void GetBuiltInPolicyThresholds(FKernelAIPolicyThresholds &outThresholds);
        static void SetBuiltInPolicyThresholds(const FKernelAIPolicyThresholds &thresholds);
        static void ResetBuiltInPolicyThresholds();
        static void GetPolicyStateSnapshot(FKernelAIPolicyStateSnapshot &outSnapshot);
        static void SetPolicyStateSnapshot(const FKernelAIPolicyStateSnapshot &snapshot);
        static void ResetPolicyStateSnapshot();
    static void IngestTelemetry(const FKernelAIExecutionTelemetry &telemetry);
    static EKernelAIExecutionAction EvaluateLastTelemetry();
        static EKernelAIExecutionAction GetLastAction();
    static void GetLastTelemetry(FKernelAIExecutionTelemetry &outTelemetry);
};

} // namespace Fortress::Kernel

#endif
