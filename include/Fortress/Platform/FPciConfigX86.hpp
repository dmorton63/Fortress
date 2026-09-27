#ifndef FORTRESS_PLATFORM_FPCICONFIGX86_HPP
#define FORTRESS_PLATFORM_FPCICONFIGX86_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Platform {

class FPciConfigX86 {
  public:
    static Fortress::Core::uint32 Read32(Fortress::Core::uint8 bus,
                                         Fortress::Core::uint8 device,
                                         Fortress::Core::uint8 function,
                                         Fortress::Core::uint8 offset);

    static Fortress::Core::uint16 Read16(Fortress::Core::uint8 bus,
                                         Fortress::Core::uint8 device,
                                         Fortress::Core::uint8 function,
                                         Fortress::Core::uint8 offset);

    static Fortress::Core::uint8 Read8(Fortress::Core::uint8 bus,
                                       Fortress::Core::uint8 device,
                                       Fortress::Core::uint8 function,
                                       Fortress::Core::uint8 offset);
};

} // namespace Fortress::Platform

#endif
