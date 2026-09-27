#include "Fortress/Kernel/FKernelRuntimeDiagnostics.hpp"

#include "Fortress/Kernel/FKernelCommandConsole.hpp"
#include "Fortress/Kernel/FKernelCommandControlPlane.hpp"
#include "Fortress/Kernel/FKernelApWorker.hpp"
#include "Fortress/Kernel/FKernelCoreDispatch.hpp"
#include "Fortress/Kernel/FKernelInputEventPlane.hpp"
#include "Fortress/Kernel/FKernelIrqControlPlane.hpp"
#include "Fortress/Kernel/FKernelRuntimeIds.hpp"
#include "Fortress/Kernel/FKernelScheduler.hpp"
#include "Fortress/Kernel/FKernelSchedulerEventPlane.hpp"
#include "Fortress/Kernel/FKernelTextFormat.hpp"
#include "Fortress/Kernel/FServiceRegistry.hpp"
#include "Fortress/Platform/FTimerX86.hpp"
#include "Fortress/Video/FVideoConsole.hpp"

namespace Fortress::Kernel {

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
    return true;
}

void FKernelRuntimeDiagnostics::Tick(FKernelRuntimeContext &runtime) {
    TryLogSchedulerStats();
    TryLogEventFlowStats();
    TryLogTextPipelineStats(runtime);
    TryLogTimerStats();
    TryLogIrqStats();
    TryLogCoreDispatchStats();
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

} // namespace Fortress::Kernel
