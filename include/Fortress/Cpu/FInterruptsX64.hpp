#ifndef FORTRESS_CPU_FINTERRUPTSX64_HPP
#define FORTRESS_CPU_FINTERRUPTSX64_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Cpu {

using FExceptionCallback = void (*)(Fortress::Core::uint64 vector, Fortress::Core::uint64 errorCode);

class FInterruptsX64 {
  public:
    static void Initialize(FExceptionCallback callback);
};

} // namespace Fortress::Cpu

#endif
