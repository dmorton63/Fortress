#ifndef FORTRESS_PLATFORM_FXHCIPCIDISCOVERY_HPP
#define FORTRESS_PLATFORM_FXHCIPCIDISCOVERY_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Platform {

struct FXhciControllerInfo {
    Fortress::Core::uint8 Bus;
    Fortress::Core::uint8 Device;
    Fortress::Core::uint8 Function;
    Fortress::Core::uint16 VendorId;
    Fortress::Core::uint16 DeviceId;
    Fortress::Core::uint8 ProgramInterface;
    Fortress::Core::uint64 MmioBase;
    Fortress::Core::uint64 MmioSize;
    bool Found;
};

class FXhciPciDiscovery {
  public:
    static bool DiscoverFirst(FXhciControllerInfo &outInfo);
};

} // namespace Fortress::Platform

#endif
