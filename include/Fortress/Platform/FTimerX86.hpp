#ifndef FORTRESS_PLATFORM_FTIMERX86_HPP
#define FORTRESS_PLATFORM_FTIMERX86_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Platform {

struct FTimerX86Stats {
    bool Initialized = false;
    Fortress::Core::uint16 LastCounter = 0;
    Fortress::Core::uint64 TickSamples = 0;
    Fortress::Core::uint64 ZeroDeltaSamples = 0;
    Fortress::Core::uint64 ClampedDeltaSamples = 0;
    Fortress::Core::uint64 LastDeltaMicros = 0;
    Fortress::Core::uint64 MaxDeltaMicros = 0;
};

class FTimerX86 {
  public:
    static bool Initialize();
    static float TickSeconds();
    static bool IsInitialized();
    static void GetStats(FTimerX86Stats &outStats);

  private:
    static Fortress::Core::uint16 ReadCounter();
    static void WriteCommand(Fortress::Core::uint8 value);
    static void WriteChannel0(Fortress::Core::uint8 value);
    static Fortress::Core::uint8 ReadChannel0();

    static bool GInitialized;
    static Fortress::Core::uint16 GLastCounter;
    static Fortress::Core::uint64 GTickSamples;
    static Fortress::Core::uint64 GZeroDeltaSamples;
    static Fortress::Core::uint64 GClampedDeltaSamples;
    static Fortress::Core::uint64 GLastDeltaMicros;
    static Fortress::Core::uint64 GMaxDeltaMicros;
};

} // namespace Fortress::Platform

#endif
