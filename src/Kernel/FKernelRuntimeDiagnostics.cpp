#include "Fortress/Kernel/FKernelRuntimeDiagnostics.hpp"

#include "Fortress/Kernel/FKernelCommandConsole.hpp"
#include "Fortress/Kernel/FKernelCommandControlPlane.hpp"
#include "Fortress/Kernel/FKernelApWorker.hpp"
#include "Fortress/Kernel/FKernelAIExecutionMonitor.hpp"
#include "Fortress/Kernel/FKernelCoreDispatch.hpp"
#include "Fortress/Kernel/FKernelInputEventPlane.hpp"
#include "Fortress/Kernel/FKernelIrqControlPlane.hpp"
#include "Fortress/Kernel/FKernelNetworkTelemetry.hpp"
#include "Fortress/Kernel/FKernelRuntimeIds.hpp"
#include "Fortress/Kernel/FKernelScheduler.hpp"
#include "Fortress/Kernel/FKernelSchedulerEventPlane.hpp"
#include "Fortress/Kernel/FKernelTextFormat.hpp"
#include "Fortress/Kernel/FPortManager.hpp"
#include "Fortress/Kernel/FServiceRegistry.hpp"
#include "Fortress/Kernel/FServiceRegistryDatabaseAdapter.hpp"
#include "Fortress/Platform/FTimerX86.hpp"
#include "Fortress/Video/FVideoConsole.hpp"

namespace Fortress::Kernel {

namespace {

static Fortress::Core::uint64 AbsDiffU64(Fortress::Core::uint64 a, Fortress::Core::uint64 b) {
    return (a >= b) ? (a - b) : (b - a);
}

static bool GHavePublishedSubsystemHealthSnapshot = false;
static FKernelSubsystemHealthSnapshot GPublishedSubsystemHealthSnapshot = {};
static bool GSubsystemHealthPublishForced = false;
static constexpr Fortress::Core::uint64 GSubsystemHealthPublishIntervalDefaultTicks = 900u;
static Fortress::Core::uint64 GSubsystemHealthPublishIntervalTicks = GSubsystemHealthPublishIntervalDefaultTicks;

} // namespace

bool FKernelRuntimeDiagnostics::Initialize(FDesktopRuntime *desktopRuntime) {
    DesktopRuntime = desktopRuntime;

    FServiceRegistrationInfo schedulerService{};
    if (!FServiceRegistry::FindServiceById(FKernelRuntimeIds::ServiceScheduler, schedulerService)) {
        FKernelCommandConsole::PushSystemLog("EVENT FLOW FAIL: SCHED SERVICE MISSING");
        return false;
    }

    char endpointLine[96] = {};
    size_t endpointPos = 0;
    FKernelTextFormat::AppendString(endpointLine, sizeof(endpointLine), endpointPos, "EVENT FLOW SCHED CH ");
    FKernelTextFormat::AppendUInt(endpointLine, sizeof(endpointLine), endpointPos, schedulerService.EndpointChannelId);
    FKernelCommandConsole::PushSystemLog(endpointLine);

    FServiceRegistrationInfo commandService{};
    if (!FServiceRegistry::FindServiceById(FKernelRuntimeIds::ServiceCommandConsole, commandService)) {
        FKernelCommandConsole::PushSystemLog("EVENT FLOW FAIL: CONSOLE SERVICE MISSING");
        return false;
    }

    char commandEndpointLine[96] = {};
    size_t commandEndpointPos = 0;
    FKernelTextFormat::AppendString(commandEndpointLine, sizeof(commandEndpointLine), commandEndpointPos, "EVENT FLOW CMD CH ");
    FKernelTextFormat::AppendUInt(commandEndpointLine,
                                  sizeof(commandEndpointLine),
                                  commandEndpointPos,
                                  commandService.EndpointChannelId);
    FKernelCommandConsole::PushSystemLog(commandEndpointLine);

    FServiceRegistrationInfo inputService{};
    if (!FServiceRegistry::FindServiceById(FKernelRuntimeIds::ServiceKeyboardInput, inputService)) {
        FKernelCommandConsole::PushSystemLog("EVENT FLOW FAIL: INPUT SERVICE MISSING");
        return false;
    }

    char inputEndpointLine[96] = {};
    size_t inputEndpointPos = 0;
    FKernelTextFormat::AppendString(inputEndpointLine, sizeof(inputEndpointLine), inputEndpointPos, "EVENT FLOW INPUT CH ");
    FKernelTextFormat::AppendUInt(inputEndpointLine, sizeof(inputEndpointLine), inputEndpointPos, inputService.EndpointChannelId);
    FKernelCommandConsole::PushSystemLog(inputEndpointLine);

    if (!FEventManager::Subscribe(FKernelRuntimeIds::TopicScheduler,
                                  &FKernelSchedulerEventPlane::HandleSchedulerEvent,
                                  nullptr,
                                  HeartbeatSubscriptionHandle)) {
        FKernelCommandConsole::PushSystemLog("EVENT FLOW FAIL: SUBSCRIBE");
        return false;
    }

    if (!FEventManager::Subscribe(FKernelRuntimeIds::TopicCommand,
                                  &FKernelCommandControlPlane::HandleCommandEvent,
                                  nullptr,
                                  CommandSubscriptionHandle)) {
        FKernelCommandConsole::PushSystemLog("EVENT FLOW FAIL: CMD SUBSCRIBE");
        return false;
    }

    if (!FEventManager::Subscribe(FKernelRuntimeIds::TopicInput,
                                  &FKernelInputEventPlane::HandleInputEvent,
                                  nullptr,
                                  InputSubscriptionHandle)) {
        FKernelCommandConsole::PushSystemLog("EVENT FLOW FAIL: INPUT SUBSCRIBE");
        return false;
    }

    if (!FEventManager::Subscribe(FKernelRuntimeIds::TopicInput,
                                  &FKernelRuntimeDiagnostics::HandleDesktopInputEvent,
                                  this,
                                  DesktopInputSubscriptionHandle)) {
        FKernelCommandConsole::PushSystemLog("EVENT FLOW FAIL: DESKTOP INPUT SUBSCRIBE");
        return false;
    }

    FKernelCommandConsole::PushSystemLog("EVENT FLOW READY");
    LastSubsystemHealthSnapshot = FKernelSubsystemHealthSnapshot{};
    return true;
}

void FKernelRuntimeDiagnostics::Tick(FKernelRuntimeContext &runtime) {
    TryPublishSubsystemHealthSnapshot(runtime);
    TryLogSchedulerStats();
    TryLogEventFlowStats();
    TryLogTextPipelineStats(runtime);
    TryLogTimerStats();
    TryLogIrqStats();
    TryLogCoreDispatchStats();
}

void FKernelRuntimeDiagnostics::PublishSubsystemHealthSnapshot(const FKernelSubsystemHealthSnapshot &snapshot) {
    LastSubsystemHealthSnapshot = snapshot;
    GPublishedSubsystemHealthSnapshot = snapshot;
    GHavePublishedSubsystemHealthSnapshot = true;

    char line[240] = {};
    size_t pos = 0;
    FKernelTextFormat::AppendString(line, sizeof(line), pos, "SUBSYS HEALTH PH ");
    FKernelTextFormat::AppendString(line,
                                    sizeof(line),
                                    pos,
                                    FKernelSubsystemStateTracker::GetPhaseName(snapshot.Phase));
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " T ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, snapshot.TickCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " SRDY ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, snapshot.Scheduler.ReadyDepth);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " SPRE ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, snapshot.Scheduler.PreemptionCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " SSTRV ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, snapshot.Scheduler.StarvationCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " SDRFTUS ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, snapshot.Scheduler.TickDriftMicros);
    FKernelCommandConsole::PushSystemLog(line);

    const FKernelAIExecutionTelemetry aiTelemetry{
        .TickCount = snapshot.TickCount,
        .SchedulerReadyDepth = snapshot.Scheduler.ReadyDepth,
        .EventQueueDepth = snapshot.EventManager.QueueDepth,
        .DeniedCapabilityChecks = snapshot.Security.DeniedCapabilityChecks,
    };
    FKernelAIExecutionMonitor::IngestTelemetry(aiTelemetry);

    const EKernelAIExecutionAction action = FKernelAIExecutionMonitor::EvaluateLastTelemetry();
    if (action != EKernelAIExecutionAction::NoAction) {
        char aiLine[96] = {};
        size_t aiPos = 0;
        FKernelTextFormat::AppendString(aiLine, sizeof(aiLine), aiPos, "AI MON ACTION ");
        FKernelTextFormat::AppendUInt(aiLine,
                                      sizeof(aiLine),
                                      aiPos,
                                      static_cast<Fortress::Core::uint64>(action));
        FKernelCommandConsole::PushSystemLog(aiLine);
    }
}

void FKernelRuntimeDiagnostics::GetLastSubsystemHealthSnapshot(FKernelSubsystemHealthSnapshot &outSnapshot) const {
    outSnapshot = LastSubsystemHealthSnapshot;
}

bool FKernelRuntimeDiagnostics::TryGetPublishedSubsystemHealthSnapshot(FKernelSubsystemHealthSnapshot &outSnapshot) {
    if (!GHavePublishedSubsystemHealthSnapshot) {
        return false;
    }

    outSnapshot = GPublishedSubsystemHealthSnapshot;
    return true;
}

void FKernelRuntimeDiagnostics::RequestSubsystemHealthPublish() {
    GSubsystemHealthPublishForced = true;
}

Fortress::Core::uint64 FKernelRuntimeDiagnostics::GetSubsystemHealthPublishIntervalTicks() {
    return GSubsystemHealthPublishIntervalTicks;
}

bool FKernelRuntimeDiagnostics::SetSubsystemHealthPublishIntervalTicks(Fortress::Core::uint64 intervalTicks) {
    if (intervalTicks == 0u || intervalTicks > 60000u) {
        return false;
    }

    GSubsystemHealthPublishIntervalTicks = intervalTicks;
    return true;
}

void FKernelRuntimeDiagnostics::ResetSubsystemHealthPublishIntervalTicks() {
    GSubsystemHealthPublishIntervalTicks = GSubsystemHealthPublishIntervalDefaultTicks;
}

void FKernelRuntimeDiagnostics::BuildSubsystemHealthSnapshot(FKernelRuntimeContext &runtime,
                                                             FKernelSubsystemHealthSnapshot &outSnapshot) {
    outSnapshot = FKernelSubsystemHealthSnapshot{};
    outSnapshot.Phase = runtime.SubsystemState.Phase;

    FKernelSchedulerStats schedulerStats{};
    FKernelScheduler::GetStats(schedulerStats);
    outSnapshot.TickCount = schedulerStats.TickCount;
    outSnapshot.Scheduler.ReadyDepth = schedulerStats.ReadyTaskCount;
    outSnapshot.Scheduler.PreemptionCount = schedulerStats.PreemptionCount;
    outSnapshot.Scheduler.StarvationCount = schedulerStats.StarvationTickCount;

    Fortress::Platform::FTimerX86Stats timerStats{};
    Fortress::Platform::FTimerX86::GetStats(timerStats);
    static constexpr Fortress::Core::uint64 SchedulerTargetTickMicros = 16666u;
    outSnapshot.Scheduler.TickDriftMicros = AbsDiffU64(timerStats.LastDeltaMicros, SchedulerTargetTickMicros);

    FEventManagerStats eventStats{};
    FEventManager::GetStats(eventStats);
    outSnapshot.EventManager.QueueDepth = eventStats.QueueDepth;
    outSnapshot.EventManager.FanoutLatencyMicros = eventStats.FanoutLatencyMicros;
    outSnapshot.EventManager.DroppedEvents = eventStats.DroppedCount;
    outSnapshot.EventManager.HandlerFaultCount = eventStats.HandlerFaultCount;

    FServiceRegistryStats serviceStats{};
    FServiceRegistry::GetStats(serviceStats);
    outSnapshot.Services.ServiceCount = serviceStats.ServiceCount;
    outSnapshot.Services.FailedStarts = serviceStats.FailedStartCount;
    outSnapshot.Services.RestartAttempts = serviceStats.RestartAttemptCount;
    outSnapshot.Services.DependencyViolations = serviceStats.DependencyViolationCount;

    FServiceRegistryDatabaseAdapterStats adapterStats{};
    FServiceRegistryDatabaseAdapter::GetStats(adapterStats);
    outSnapshot.Services.FailedStarts += adapterStats.FailedLookupCount;

    FPortManagerStats portStats{};
    FPortManager::GetStats(portStats);
    outSnapshot.Security.DeniedCapabilityChecks = portStats.DeniedAccessCount;
    outSnapshot.Security.PolicyLoaded = (serviceStats.ServiceCount > 0u);
    outSnapshot.Security.AuditQueuePressure = portStats.ActiveLeaseCount;

    FKernelNetworkStats networkStats{};
    FKernelNetworkTelemetry::GetStats(networkStats);
    outSnapshot.Network.InterfaceCount = networkStats.InterfaceCount;
    outSnapshot.Network.RxCount = networkStats.RxCount;
    outSnapshot.Network.TxCount = networkStats.TxCount;
    outSnapshot.Network.DropCount = networkStats.DropCount;
    outSnapshot.Network.LinkUp = networkStats.LinkUp;

    if (DesktopRuntime != nullptr) {
        outSnapshot.DesktopWindow.DesktopReady = DesktopRuntime->IsReady();
        FDesktopCompositorStats compositorStats{};
        DesktopRuntime->GetCompositor().GetStats(compositorStats);
        outSnapshot.DesktopWindow.SurfaceCount = compositorStats.SurfaceCount;
        outSnapshot.DesktopWindow.DirtySurfaceCount = compositorStats.DirtySurfaceCount;
        outSnapshot.DesktopWindow.HighestZOrder = compositorStats.HighestZOrder;
    }
}

void FKernelRuntimeDiagnostics::TryPublishSubsystemHealthSnapshot(FKernelRuntimeContext &runtime) {
    FKernelSchedulerStats schedulerStats{};
    FKernelScheduler::GetStats(schedulerStats);
    if (!GSubsystemHealthPublishForced && schedulerStats.TickCount < NextSubsystemHealthTick) {
        return;
    }

    FKernelSubsystemHealthSnapshot snapshot{};
    BuildSubsystemHealthSnapshot(runtime, snapshot);
    PublishSubsystemHealthSnapshot(snapshot);
    GSubsystemHealthPublishForced = false;
    NextSubsystemHealthTick = schedulerStats.TickCount + GSubsystemHealthPublishIntervalTicks;
}

void FKernelRuntimeDiagnostics::HandleDesktopInputEvent(const FKernelEvent &event, void *context) {
    FKernelRuntimeDiagnostics *self = static_cast<FKernelRuntimeDiagnostics *>(context);
    if (self == nullptr || self->DesktopRuntime == nullptr) {
        return;
    }

    self->DesktopRuntime->HandleInputEvent(event);
}

void FKernelRuntimeDiagnostics::TryLogSchedulerStats() {
    FKernelSchedulerStats schedulerStats{};
    FKernelScheduler::GetStats(schedulerStats);
    if (schedulerStats.TickCount < NextSchedulerStatsTick) {
        return;
    }

    char line[176] = {};
    size_t pos = 0;
    FKernelTextFormat::AppendString(line, sizeof(line), pos, "SCHED TICK ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, schedulerStats.TickCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " READY ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, schedulerStats.ReadyTaskCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " RUN ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, schedulerStats.RunningTaskCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " BLK ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, schedulerStats.BlockedTaskCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " LAST ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, schedulerStats.LastScheduledTaskId);
    FKernelCommandConsole::PushSystemLog(line);

    NextSchedulerStatsTick = schedulerStats.TickCount + SchedulerStatsLogIntervalTicks;
}

void FKernelRuntimeDiagnostics::TryLogEventFlowStats() {
    FKernelSchedulerStats schedulerStats{};
    FKernelScheduler::GetStats(schedulerStats);
    if (schedulerStats.TickCount < NextEventStatsTick) {
        return;
    }

    FEventManagerStats eventStats{};
    FEventManager::GetStats(eventStats);

    FKernelSchedulerEventStats schedulerEventStats{};
    FKernelSchedulerEventPlane::GetStats(schedulerEventStats);

    FKernelCommandControlStats commandStats{};
    FKernelCommandControlPlane::GetStats(commandStats);

    FKernelInputEventStats inputStats{};
    FKernelInputEventPlane::GetStats(inputStats);

    char line[196] = {};
    size_t pos = 0;
    FKernelTextFormat::AppendString(line, sizeof(line), pos, "EVENT STATS PUB ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, eventStats.PublishedCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " DSP ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, eventStats.DispatchedCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " DROP ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, eventStats.DroppedCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " HRX ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, schedulerEventStats.HeartbeatEventsReceived);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " CRX ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, commandStats.CommandEventsReceived);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " IRX ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, inputStats.InputEventsReceived);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " CH ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, static_cast<Fortress::Core::uint64>(commandStats.LastCommandHash));
    FKernelCommandConsole::PushSystemLog(line);

    NextEventStatsTick = schedulerStats.TickCount + EventStatsLogIntervalTicks;
}

void FKernelRuntimeDiagnostics::TryLogTextPipelineStats(FKernelRuntimeContext &runtime) {
    if (runtime.Console == nullptr) {
        return;
    }

    FKernelSchedulerStats schedulerStats{};
    FKernelScheduler::GetStats(schedulerStats);
    if (schedulerStats.TickCount < NextTextStatsTick) {
        return;
    }

    Fortress::Video::FFontCacheStats cacheStats{};
    runtime.Console->GetFontCacheStats(cacheStats);

    char line[320] = {};
    size_t pos = 0;
    FKernelTextFormat::AppendString(line, sizeof(line), pos, "TEXT CACHE E ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, cacheStats.EntryCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " C ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, cacheStats.Capacity);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " H ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, cacheStats.HitCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " M ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, cacheStats.MissCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " EV ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, cacheStats.EvictionCount);
    FKernelCommandConsole::PushSystemLog(line);

    NextTextStatsTick = schedulerStats.TickCount + TextStatsLogIntervalTicks;
}

void FKernelRuntimeDiagnostics::TryLogTimerStats() {
    FKernelSchedulerStats schedulerStats{};
    FKernelScheduler::GetStats(schedulerStats);
    if (schedulerStats.TickCount < NextTimerStatsTick) {
        return;
    }

    Fortress::Platform::FTimerX86Stats timerStats{};
    Fortress::Platform::FTimerX86::GetStats(timerStats);

    char line[196] = {};
    size_t pos = 0;
    FKernelTextFormat::AppendString(line, sizeof(line), pos, "TIMER PIT INIT ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, timerStats.Initialized ? 1u : 0u);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " TICKS ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, timerStats.TickSamples);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " ZERO ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, timerStats.ZeroDeltaSamples);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " CLAMP ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, timerStats.ClampedDeltaSamples);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " LASTUS ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, timerStats.LastDeltaMicros);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " MAXUS ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, timerStats.MaxDeltaMicros);
    FKernelCommandConsole::PushSystemLog(line);

    NextTimerStatsTick = schedulerStats.TickCount + TimerStatsLogIntervalTicks;
}

void FKernelRuntimeDiagnostics::TryLogIrqStats() {
    FKernelSchedulerStats schedulerStats{};
    FKernelScheduler::GetStats(schedulerStats);
    if (schedulerStats.TickCount < NextIrqStatsTick) {
        return;
    }

    FKernelIrqControlStats irqStats{};
    FKernelIrqControlPlane::GetStats(irqStats);

    char line[160] = {};
    size_t pos = 0;
    FKernelTextFormat::AppendString(line, sizeof(line), pos, "IRQ REG ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, irqStats.RegisteredCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " MASK ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, irqStats.MaskedCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " HIT ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, irqStats.DeliveredCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " DROP ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, irqStats.DroppedCount);
    FKernelCommandConsole::PushSystemLog(line);

    NextIrqStatsTick = schedulerStats.TickCount + IrqStatsLogIntervalTicks;
}

void FKernelRuntimeDiagnostics::TryLogCoreDispatchStats() {
    FKernelSchedulerStats schedulerStats{};
    FKernelScheduler::GetStats(schedulerStats);
    if (schedulerStats.TickCount < NextCoreDispatchStatsTick) {
        return;
    }

    FKernelCoreDispatchStats coreDispatchStats{};
    FKernelCoreDispatch::GetStats(coreDispatchStats);

    char line[192] = {};
    size_t pos = 0;
    FKernelTextFormat::AppendString(line, sizeof(line), pos, "CDISP CORE ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, coreDispatchStats.OnlineCoreCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " PEND ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, coreDispatchStats.PendingWorkItems);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " ENQ ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, coreDispatchStats.EnqueuedCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " EXE ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, coreDispatchStats.ExecutedCount);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " DROP ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, coreDispatchStats.DroppedCount);
    FKernelCommandConsole::PushSystemLog(line);

    char perCoreLine[192] = {};
    size_t perCorePos = 0;
    FKernelTextFormat::AppendString(perCoreLine, sizeof(perCoreLine), perCorePos, "CDISP EXE CORE");
    for (Fortress::Core::uint32 coreId = 0u;
         coreId < coreDispatchStats.OnlineCoreCount && coreId < FKernelCoreDispatchMaxTrackedCores;
         coreId++) {
        FKernelTextFormat::AppendString(perCoreLine, sizeof(perCoreLine), perCorePos, " ");
        FKernelTextFormat::AppendUInt(perCoreLine, sizeof(perCoreLine), perCorePos, coreId);
        FKernelTextFormat::AppendString(perCoreLine, sizeof(perCoreLine), perCorePos, ":");
        FKernelTextFormat::AppendUInt(perCoreLine,
                          sizeof(perCoreLine),
                          perCorePos,
                          FKernelCoreDispatch::GetExecutedCountForCore(coreId));
    }
    FKernelCommandConsole::PushSystemLog(perCoreLine);

    FKernelApWorkerStats apWorkerStats{};
    FKernelApWorker::GetStats(apWorkerStats);
    char apLine[192] = {};
    size_t apPos = 0;
    FKernelTextFormat::AppendString(apLine, sizeof(apLine), apPos, "APW START ");
    FKernelTextFormat::AppendUInt(apLine, sizeof(apLine), apPos, apWorkerStats.StartedWorkers);
    FKernelTextFormat::AppendString(apLine, sizeof(apLine), apPos, " ACTIVE ");
    FKernelTextFormat::AppendUInt(apLine, sizeof(apLine), apPos, apWorkerStats.ActiveWorkers);
    FKernelTextFormat::AppendString(apLine, sizeof(apLine), apPos, " PTICK ");
    FKernelTextFormat::AppendUInt(apLine, sizeof(apLine), apPos, apWorkerStats.ProbeTicksTotal);
    FKernelTextFormat::AppendString(apLine, sizeof(apLine), apPos, " LAST ");
    FKernelTextFormat::AppendUInt(apLine, sizeof(apLine), apPos, apWorkerStats.LastProbeCoreId);
    FKernelTextFormat::AppendString(apLine, sizeof(apLine), apPos, " MODE ");
    FKernelTextFormat::AppendString(apLine,
                                    sizeof(apLine),
                                    apPos,
                                    FKernelApWorker::AreDispatchCallbacksEnabled() ? "CALLBACK" : "PROBE");
    FKernelCommandConsole::PushSystemLog(apLine);

    NextCoreDispatchStatsTick = schedulerStats.TickCount + CoreDispatchStatsLogIntervalTicks;
}

void FKernelRuntimeDiagnostics::TryLogServicePortStats() {
    FKernelSchedulerStats schedulerStats{};
    FKernelScheduler::GetStats(schedulerStats);
    if (schedulerStats.TickCount < NextServicePortStatsTick) {
        return;
    }

    FServiceRegistryDatabaseAdapterStats adapterStats{};
    FServiceRegistryDatabaseAdapter::GetStats(adapterStats);
    char serviceLine[160] = {};
    size_t servicePos = 0;
    FKernelTextFormat::AppendString(serviceLine, sizeof(serviceLine), servicePos, "SRDB REC ");
    FKernelTextFormat::AppendUInt(serviceLine, sizeof(serviceLine), servicePos, adapterStats.RecordCount);
    FKernelTextFormat::AppendString(serviceLine, sizeof(serviceLine), servicePos, " REF ");
    FKernelTextFormat::AppendUInt(serviceLine, sizeof(serviceLine), servicePos, adapterStats.RefreshCount);
    FKernelTextFormat::AppendString(serviceLine, sizeof(serviceLine), servicePos, " MISS ");
    FKernelTextFormat::AppendUInt(serviceLine, sizeof(serviceLine), servicePos, adapterStats.FailedLookupCount);
    FKernelCommandConsole::PushSystemLog(serviceLine);

    FPortManagerStats portStats{};
    FPortManager::GetStats(portStats);
    char portLine[196] = {};
    size_t portPos = 0;
    FKernelTextFormat::AppendString(portLine, sizeof(portLine), portPos, "PORT REG ");
    FKernelTextFormat::AppendUInt(portLine, sizeof(portLine), portPos, portStats.RegisteredPortCount);
    FKernelTextFormat::AppendString(portLine, sizeof(portLine), portPos, " OPEN ");
    FKernelTextFormat::AppendUInt(portLine, sizeof(portLine), portPos, portStats.ActiveLeaseCount);
    FKernelTextFormat::AppendString(portLine, sizeof(portLine), portPos, " DENY ");
    FKernelTextFormat::AppendUInt(portLine, sizeof(portLine), portPos, portStats.DeniedAccessCount);
    FKernelTextFormat::AppendString(portLine, sizeof(portLine), portPos, " OPEN# ");
    FKernelTextFormat::AppendUInt(portLine, sizeof(portLine), portPos, portStats.LeaseOpenCount);
    FKernelTextFormat::AppendString(portLine, sizeof(portLine), portPos, " CLS# ");
    FKernelTextFormat::AppendUInt(portLine, sizeof(portLine), portPos, portStats.LeaseCloseCount);
    FKernelCommandConsole::PushSystemLog(portLine);

    FPortAuditEntry lastDenied{};
    if (FPortManager::GetLastDeniedAuditEntry(lastDenied)) {
        char deniedLine[176] = {};
        size_t deniedPos = 0;
        FKernelTextFormat::AppendString(deniedLine, sizeof(deniedLine), deniedPos, "PORT DENY P ");
        FKernelTextFormat::AppendUInt(deniedLine, sizeof(deniedLine), deniedPos, lastDenied.PortId);
        FKernelTextFormat::AppendString(deniedLine, sizeof(deniedLine), deniedPos, " S ");
        FKernelTextFormat::AppendUInt(deniedLine, sizeof(deniedLine), deniedPos, lastDenied.ServiceId);
        FKernelTextFormat::AppendString(deniedLine, sizeof(deniedLine), deniedPos, " A ");
        FKernelTextFormat::AppendUInt(deniedLine,
                                      sizeof(deniedLine),
                                      deniedPos,
                                      static_cast<Fortress::Core::uint64>(lastDenied.Action));
        FKernelTextFormat::AppendString(deniedLine, sizeof(deniedLine), deniedPos, " L ");
        FKernelTextFormat::AppendUInt(deniedLine, sizeof(deniedLine), deniedPos, lastDenied.LeaseId);
        FKernelCommandConsole::PushSystemLog(deniedLine);
    }

    NextServicePortStatsTick = schedulerStats.TickCount + ServicePortStatsLogIntervalTicks;
}

} // namespace Fortress::Kernel
