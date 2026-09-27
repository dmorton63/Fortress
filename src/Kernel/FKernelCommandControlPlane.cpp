#include "Fortress/Kernel/FKernelCommandControlPlane.hpp"

#include <cstddef>

#include "Fortress/Kernel/FKernelCommandConsole.hpp"
#include "Fortress/Kernel/FKernelRuntimeIds.hpp"
#include "Fortress/Kernel/FKernelTextFormat.hpp"

namespace Fortress::Kernel {

static Fortress::Core::uint64 GCommandEventsReceived = 0;
static Fortress::Core::uint32 GLastCommandLength = 0;
static Fortress::Core::uint32 GLastCommandHash = 0;
static Fortress::Core::uint32 GCommandVisualPulseFramesRemaining = 0;
static bool GWireframeEnabledByEvent = false;
static bool GScenePausedByEvent = false;
static bool GCursorOverlayEnabledByEvent = false;
static bool GDesktopSurfaceOverlayEnabledByEvent = false;

void FKernelCommandControlPlane::Initialize() {
    GCommandEventsReceived = 0;
    GLastCommandLength = 0;
    GLastCommandHash = 0;
    GCommandVisualPulseFramesRemaining = 0;
    GWireframeEnabledByEvent = false;
    GScenePausedByEvent = false;
    GCursorOverlayEnabledByEvent = false;
    GDesktopSurfaceOverlayEnabledByEvent = false;
}

void FKernelCommandControlPlane::HandleCommandEvent(const FKernelEvent &event, void *context) {
    (void)context;

    if (event.SourceServiceId != Fortress::Kernel::FKernelRuntimeIds::ServiceCommandConsole) {
        return;
    }

    if (event.EventId == Fortress::Kernel::FKernelRuntimeIds::EventRenderWireframeSet) {
        GWireframeEnabledByEvent = (event.Arg0 != 0u);
        return;
    }

    if (event.EventId == Fortress::Kernel::FKernelRuntimeIds::EventScenePauseSet) {
        GScenePausedByEvent = (event.Arg0 != 0u);
        return;
    }

    if (event.EventId == Fortress::Kernel::FKernelRuntimeIds::EventCursorOverlaySet) {
        GCursorOverlayEnabledByEvent = (event.Arg0 != 0u);
        return;
    }

    if (event.EventId == Fortress::Kernel::FKernelRuntimeIds::EventDesktopSurfaceOverlaySet) {
        GDesktopSurfaceOverlayEnabledByEvent = (event.Arg0 != 0u);
        return;
    }

    if (event.EventId != Fortress::Kernel::FKernelRuntimeIds::EventCommandSubmitted) {
        return;
    }

    GCommandEventsReceived++;
    GLastCommandLength = event.Arg0;
    GLastCommandHash = event.Arg1;
    GCommandVisualPulseFramesRemaining = 18u;

    if ((GCommandEventsReceived % 8ull) != 0ull) {
        return;
    }

    char line[96] = {};
    size_t pos = 0;
    FKernelTextFormat::AppendString(line, sizeof(line), pos, "EVENT CMD RX ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, GCommandEventsReceived);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " LEN ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, GLastCommandLength);
    FKernelCommandConsole::PushSystemLog(line);
}

bool FKernelCommandControlPlane::IsWireframeEnabled() {
    return GWireframeEnabledByEvent;
}

bool FKernelCommandControlPlane::IsScenePaused() {
    return GScenePausedByEvent;
}

bool FKernelCommandControlPlane::IsCursorOverlayEnabled() {
    return GCursorOverlayEnabledByEvent;
}

bool FKernelCommandControlPlane::IsDesktopSurfaceOverlayEnabled() {
    return GDesktopSurfaceOverlayEnabledByEvent;
}

bool FKernelCommandControlPlane::ConsumeCommandPulseFrame() {
    if (GCommandVisualPulseFramesRemaining == 0u) {
        return false;
    }

    GCommandVisualPulseFramesRemaining--;
    return true;
}

void FKernelCommandControlPlane::GetStats(FKernelCommandControlStats &outStats) {
    outStats = FKernelCommandControlStats{
        .CommandEventsReceived = GCommandEventsReceived,
        .LastCommandLength = GLastCommandLength,
        .LastCommandHash = GLastCommandHash,
    };
}

} // namespace Fortress::Kernel