#include "Fortress/Platform/FTimerX86.hpp"

namespace Fortress::Platform {

bool FTimerX86::GInitialized = false;
Fortress::Core::uint16 FTimerX86::GLastCounter = 0;
Fortress::Core::uint64 FTimerX86::GTickSamples = 0;
Fortress::Core::uint64 FTimerX86::GZeroDeltaSamples = 0;
Fortress::Core::uint64 FTimerX86::GClampedDeltaSamples = 0;
Fortress::Core::uint64 FTimerX86::GLastDeltaMicros = 0;
Fortress::Core::uint64 FTimerX86::GMaxDeltaMicros = 0;

static inline void Out8(Fortress::Core::uint16 port, Fortress::Core::uint8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline Fortress::Core::uint8 In8(Fortress::Core::uint16 port) {
    Fortress::Core::uint8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void FTimerX86::WriteCommand(Fortress::Core::uint8 value) {
    Out8(0x43, value);
}

void FTimerX86::WriteChannel0(Fortress::Core::uint8 value) {
    Out8(0x40, value);
}

Fortress::Core::uint8 FTimerX86::ReadChannel0() {
    return In8(0x40);
}

Fortress::Core::uint16 FTimerX86::ReadCounter() {
    WriteCommand(0x00);
    const Fortress::Core::uint8 low = ReadChannel0();
    const Fortress::Core::uint8 high = ReadChannel0();
    return static_cast<Fortress::Core::uint16>((static_cast<Fortress::Core::uint16>(high) << 8) | low);
}

bool FTimerX86::Initialize() {
    // Channel 0, lobyte/hibyte, mode 2, binary.
    WriteCommand(0x34);
    WriteChannel0(0x00);
    WriteChannel0(0x00);

    GLastCounter = ReadCounter();
    GTickSamples = 0;
    GZeroDeltaSamples = 0;
    GClampedDeltaSamples = 0;
    GLastDeltaMicros = 0;
    GMaxDeltaMicros = 0;
    GInitialized = true;
    return true;
}

float FTimerX86::TickSeconds() {
    if (!GInitialized) {
        return 0.0f;
    }

    const Fortress::Core::uint16 current = ReadCounter();

    Fortress::Core::uint32 deltaCounts;
    if (GLastCounter >= current) {
        deltaCounts = static_cast<Fortress::Core::uint32>(GLastCounter - current);
    } else {
        deltaCounts = static_cast<Fortress::Core::uint32>(GLastCounter + (0x10000u - current));
    }

    GLastCounter = current;

    constexpr float PITFrequency = 1193182.0f;
    constexpr float MaxTickSeconds = 0.05f;
    GTickSamples++;
    if (deltaCounts == 0u) {
        GZeroDeltaSamples++;
    }

    float dt = static_cast<float>(deltaCounts) / PITFrequency;
    if (dt > MaxTickSeconds) {
        dt = MaxTickSeconds;
        GClampedDeltaSamples++;
    }

    GLastDeltaMicros = static_cast<Fortress::Core::uint64>(dt * 1000000.0f);
    if (GLastDeltaMicros > GMaxDeltaMicros) {
        GMaxDeltaMicros = GLastDeltaMicros;
    }

    return dt;
}

bool FTimerX86::IsInitialized() {
    return GInitialized;
}

void FTimerX86::GetStats(FTimerX86Stats &outStats) {
    outStats = FTimerX86Stats{
        .Initialized = GInitialized,
        .LastCounter = GLastCounter,
        .TickSamples = GTickSamples,
        .ZeroDeltaSamples = GZeroDeltaSamples,
        .ClampedDeltaSamples = GClampedDeltaSamples,
        .LastDeltaMicros = GLastDeltaMicros,
        .MaxDeltaMicros = GMaxDeltaMicros,
    };
}

} // namespace Fortress::Platform
