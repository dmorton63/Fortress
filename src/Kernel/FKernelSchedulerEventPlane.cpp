#include "Fortress/Kernel/FKernelSchedulerEventPlane.hpp"

#include <cstddef>

#include "Fortress/Kernel/FKernelCommandConsole.hpp"
#include "Fortress/Kernel/FKernelRuntimeIds.hpp"
#include "Fortress/Kernel/FKernelTextFormat.hpp"

namespace Fortress::Kernel {

static Fortress::Core::uint64 GHeartbeatEventsReceived = 0;
static Fortress::Core::uint32 GLastHeartbeatRunCount = 0;

void FKernelSchedulerEventPlane::Initialize() {
    GHeartbeatEventsReceived = 0;
    GLastHeartbeatRunCount = 0;
}

void FKernelSchedulerEventPlane::HandleSchedulerEvent(const FKernelEvent &event, void *context) {
    (void)context;

    if (event.EventId != Fortress::Kernel::FKernelRuntimeIds::EventSchedulerHeartbeat ||
        event.SourceServiceId != Fortress::Kernel::FKernelRuntimeIds::ServiceScheduler) {
        return;
    }

    GHeartbeatEventsReceived++;
    GLastHeartbeatRunCount = event.Arg0;
    if ((GHeartbeatEventsReceived % 120ull) != 0ull) {
        return;
    }

    char line[96] = {};
    size_t pos = 0;
    FKernelTextFormat::AppendString(line, sizeof(line), pos, "EVENT HEARTBEAT RX ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, GHeartbeatEventsReceived);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " RUN ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, GLastHeartbeatRunCount);
    FKernelCommandConsole::PushSystemLog(line);
}

void FKernelSchedulerEventPlane::GetStats(FKernelSchedulerEventStats &outStats) {
    outStats = FKernelSchedulerEventStats{
        .HeartbeatEventsReceived = GHeartbeatEventsReceived,
        .LastHeartbeatRunCount = GLastHeartbeatRunCount,
    };
}

} // namespace Fortress::Kernel