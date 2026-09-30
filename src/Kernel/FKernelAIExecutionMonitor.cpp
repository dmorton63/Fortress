#include "Fortress/Kernel/FKernelAIExecutionMonitor.hpp"

namespace Fortress::Kernel {

static bool GInitialized = false;
static FKernelAIExecutionTelemetry GLastTelemetry = {};
static FKernelAIExecutionPolicyEvaluator GPolicyEvaluator = nullptr;
static void *GPolicyEvaluatorContext = nullptr;
static EKernelAIExecutionAction GLastAction = EKernelAIExecutionAction::NoAction;
static EKernelAIBuiltInPolicyMode GBuiltInPolicyMode = EKernelAIBuiltInPolicyMode::NoOp;
static FKernelAIPolicyThresholds GPolicyThresholds = {};

static void NormalizeThresholds(FKernelAIPolicyThresholds &thresholds) {
    if (thresholds.ThrottleEventQueueMin < thresholds.AlertEventQueueMin) {
        thresholds.ThrottleEventQueueMin = thresholds.AlertEventQueueMin;
    }
    if (thresholds.IsolateEventQueueMin < thresholds.ThrottleEventQueueMin) {
        thresholds.IsolateEventQueueMin = thresholds.ThrottleEventQueueMin;
    }
    if (thresholds.RestartEventQueueMin < thresholds.AlertEventQueueMin) {
        thresholds.RestartEventQueueMin = thresholds.AlertEventQueueMin;
    }
    if (thresholds.IsolateDeniedMin < thresholds.AlertDeniedMin) {
        thresholds.IsolateDeniedMin = thresholds.AlertDeniedMin;
    }
    if (thresholds.RestartDeniedMin < thresholds.IsolateDeniedMin) {
        thresholds.RestartDeniedMin = thresholds.IsolateDeniedMin;
    }
}

static EKernelAIExecutionAction EvaluateBuiltInPolicy(const FKernelAIExecutionTelemetry &telemetry) {
    switch (GBuiltInPolicyMode) {
    case EKernelAIBuiltInPolicyMode::NoOp:
        return EKernelAIExecutionAction::NoAction;
    case EKernelAIBuiltInPolicyMode::Alert:
        if (telemetry.DeniedCapabilityChecks >= GPolicyThresholds.AlertDeniedMin ||
            telemetry.EventQueueDepth >= GPolicyThresholds.AlertEventQueueMin) {
            return EKernelAIExecutionAction::Alert;
        }
        return EKernelAIExecutionAction::NoAction;
    case EKernelAIBuiltInPolicyMode::Throttle:
        if (telemetry.EventQueueDepth >= GPolicyThresholds.ThrottleEventQueueMin) {
            return EKernelAIExecutionAction::Throttle;
        }
        return EKernelAIExecutionAction::NoAction;
    case EKernelAIBuiltInPolicyMode::Isolate:
        if (telemetry.DeniedCapabilityChecks >= GPolicyThresholds.IsolateDeniedMin) {
            return EKernelAIExecutionAction::Isolate;
        }
        if (telemetry.EventQueueDepth >= GPolicyThresholds.IsolateEventQueueMin) {
            return EKernelAIExecutionAction::Throttle;
        }
        return EKernelAIExecutionAction::NoAction;
    case EKernelAIBuiltInPolicyMode::Restart:
        if (telemetry.SchedulerReadyDepth == 0u &&
            telemetry.EventQueueDepth >= GPolicyThresholds.RestartEventQueueMin) {
            return EKernelAIExecutionAction::Restart;
        }
        if (telemetry.DeniedCapabilityChecks >= GPolicyThresholds.RestartDeniedMin) {
            return EKernelAIExecutionAction::Isolate;
        }
        return EKernelAIExecutionAction::NoAction;
    default:
        return EKernelAIExecutionAction::NoAction;
    }
}

bool FKernelAIExecutionMonitor::Initialize() {
    GInitialized = true;
    GLastTelemetry = FKernelAIExecutionTelemetry{};
    GPolicyEvaluator = nullptr;
    GPolicyEvaluatorContext = nullptr;
    GLastAction = EKernelAIExecutionAction::NoAction;
    GBuiltInPolicyMode = EKernelAIBuiltInPolicyMode::NoOp;
    GPolicyThresholds = FKernelAIPolicyThresholds{};
    return true;
}

void FKernelAIExecutionMonitor::SetPolicyEvaluator(FKernelAIExecutionPolicyEvaluator evaluator, void *context) {
    if (!GInitialized) {
        return;
    }

    GPolicyEvaluator = evaluator;
    GPolicyEvaluatorContext = context;
}

bool FKernelAIExecutionMonitor::IsPolicyEvaluatorInstalled() {
    return GInitialized && GPolicyEvaluator != nullptr;
}

void FKernelAIExecutionMonitor::SetBuiltInPolicyMode(EKernelAIBuiltInPolicyMode mode) {
    if (!GInitialized) {
        return;
    }

    GBuiltInPolicyMode = mode;
}

EKernelAIBuiltInPolicyMode FKernelAIExecutionMonitor::GetBuiltInPolicyMode() {
    return GBuiltInPolicyMode;
}

const char *FKernelAIExecutionMonitor::GetBuiltInPolicyModeName() {
    switch (GBuiltInPolicyMode) {
    case EKernelAIBuiltInPolicyMode::NoOp:
        return "NOOP";
    case EKernelAIBuiltInPolicyMode::Alert:
        return "ALERT";
    case EKernelAIBuiltInPolicyMode::Throttle:
        return "THROTTLE";
    case EKernelAIBuiltInPolicyMode::Isolate:
        return "ISOLATE";
    case EKernelAIBuiltInPolicyMode::Restart:
        return "RESTART";
    default:
        return "UNKNOWN";
    }
}

void FKernelAIExecutionMonitor::GetBuiltInPolicyThresholds(FKernelAIPolicyThresholds &outThresholds) {
    outThresholds = GPolicyThresholds;
}

void FKernelAIExecutionMonitor::SetBuiltInPolicyThresholds(const FKernelAIPolicyThresholds &thresholds) {
    if (!GInitialized) {
        return;
    }

    GPolicyThresholds = thresholds;
    NormalizeThresholds(GPolicyThresholds);
}

void FKernelAIExecutionMonitor::ResetBuiltInPolicyThresholds() {
    if (!GInitialized) {
        return;
    }

    GPolicyThresholds = FKernelAIPolicyThresholds{};
    NormalizeThresholds(GPolicyThresholds);
}

void FKernelAIExecutionMonitor::GetPolicyStateSnapshot(FKernelAIPolicyStateSnapshot &outSnapshot) {
    outSnapshot = FKernelAIPolicyStateSnapshot{
        .Mode = GBuiltInPolicyMode,
        .Thresholds = GPolicyThresholds,
    };
}

void FKernelAIExecutionMonitor::SetPolicyStateSnapshot(const FKernelAIPolicyStateSnapshot &snapshot) {
    if (!GInitialized) {
        return;
    }

    GBuiltInPolicyMode = snapshot.Mode;
    GPolicyThresholds = snapshot.Thresholds;
    NormalizeThresholds(GPolicyThresholds);
}

void FKernelAIExecutionMonitor::ResetPolicyStateSnapshot() {
    if (!GInitialized) {
        return;
    }

    GBuiltInPolicyMode = EKernelAIBuiltInPolicyMode::NoOp;
    GPolicyThresholds = FKernelAIPolicyThresholds{};
    NormalizeThresholds(GPolicyThresholds);
}

void FKernelAIExecutionMonitor::IngestTelemetry(const FKernelAIExecutionTelemetry &telemetry) {
    if (!GInitialized) {
        return;
    }

    GLastTelemetry = telemetry;
}

EKernelAIExecutionAction FKernelAIExecutionMonitor::EvaluateLastTelemetry() {
    if (!GInitialized) {
        GLastAction = EKernelAIExecutionAction::NoAction;
        return GLastAction;
    }

    if (GPolicyEvaluator != nullptr) {
        GLastAction = GPolicyEvaluator(GLastTelemetry, GPolicyEvaluatorContext);
        return GLastAction;
    }

    GLastAction = EvaluateBuiltInPolicy(GLastTelemetry);
    return GLastAction;
}

EKernelAIExecutionAction FKernelAIExecutionMonitor::GetLastAction() {
    return GLastAction;
}

void FKernelAIExecutionMonitor::GetLastTelemetry(FKernelAIExecutionTelemetry &outTelemetry) {
    outTelemetry = GLastTelemetry;
}

} // namespace Fortress::Kernel
