#include "Fortress/Kernel/FKernelInputEventPlane.hpp"

#include <cstddef>

#include "Fortress/Kernel/FKernelCommandConsole.hpp"
#include "Fortress/Kernel/FKernelRuntimeIds.hpp"
#include "Fortress/Kernel/FKernelTextFormat.hpp"

namespace Fortress::Kernel {

static Fortress::Core::uint64 GInputEventsReceived = 0;
static Fortress::Core::uint32 GLastKeyAscii = 0;
static Fortress::Core::uint32 GInputPulseFramesRemaining = 0;

void FKernelInputEventPlane::Initialize() {
    GInputEventsReceived = 0;
    GLastKeyAscii = 0;
    GInputPulseFramesRemaining = 0;
}

void FKernelInputEventPlane::HandleInputEvent(const FKernelEvent &event, void *context) {
    (void)context;

    if (event.EventId != Fortress::Kernel::FKernelRuntimeIds::EventInputKeyPressed ||
        event.SourceServiceId != Fortress::Kernel::FKernelRuntimeIds::ServiceKeyboardInput) {
        return;
    }

    GInputEventsReceived++;
    GLastKeyAscii = event.Arg0 & 0xFFu;
    GInputPulseFramesRemaining = 6u;

    if ((GInputEventsReceived % 32ull) != 0ull) {
        return;
    }

    char line[96] = {};
    size_t pos = 0;
    FKernelTextFormat::AppendString(line, sizeof(line), pos, "EVENT INPUT RX ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, GInputEventsReceived);
    FKernelTextFormat::AppendString(line, sizeof(line), pos, " KEY ");
    FKernelTextFormat::AppendUInt(line, sizeof(line), pos, GLastKeyAscii);
    FKernelCommandConsole::PushSystemLog(line);
}

bool FKernelInputEventPlane::ConsumeInputPulseFrame() {
    if (GInputPulseFramesRemaining == 0u) {
        return false;
    }

    GInputPulseFramesRemaining--;
    return true;
}

void FKernelInputEventPlane::GetStats(FKernelInputEventStats &outStats) {
    outStats = FKernelInputEventStats{
        .InputEventsReceived = GInputEventsReceived,
        .LastKeyAscii = GLastKeyAscii,
    };
}

} // namespace Fortress::Kernel