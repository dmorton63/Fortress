#include "Fortress/Platform/FPciConfigX86.hpp"

namespace Fortress::Platform {

static constexpr Fortress::Core::uint16 PciConfigAddressPort = 0xCF8;
static constexpr Fortress::Core::uint16 PciConfigDataPort = 0xCFC;

static inline void Out32(Fortress::Core::uint16 port, Fortress::Core::uint32 value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline Fortress::Core::uint32 In32(Fortress::Core::uint16 port) {
    Fortress::Core::uint32 value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static Fortress::Core::uint32 BuildConfigAddress(Fortress::Core::uint8 bus,
                                                  Fortress::Core::uint8 device,
                                                  Fortress::Core::uint8 function,
                                                  Fortress::Core::uint8 offset) {
    return (1u << 31) | (static_cast<Fortress::Core::uint32>(bus) << 16u) |
           (static_cast<Fortress::Core::uint32>(device) << 11u) |
           (static_cast<Fortress::Core::uint32>(function) << 8u) |
           (static_cast<Fortress::Core::uint32>(offset) & 0xFCu);
}

Fortress::Core::uint32 FPciConfigX86::Read32(Fortress::Core::uint8 bus,
                                             Fortress::Core::uint8 device,
                                             Fortress::Core::uint8 function,
                                             Fortress::Core::uint8 offset) {
    Out32(PciConfigAddressPort, BuildConfigAddress(bus, device, function, offset));
    return In32(PciConfigDataPort);
}

Fortress::Core::uint16 FPciConfigX86::Read16(Fortress::Core::uint8 bus,
                                             Fortress::Core::uint8 device,
                                             Fortress::Core::uint8 function,
                                             Fortress::Core::uint8 offset) {
    const Fortress::Core::uint32 value = Read32(bus, device, function, offset);
    const Fortress::Core::uint32 shift = static_cast<Fortress::Core::uint32>((offset & 0x2u) * 8u);
    return static_cast<Fortress::Core::uint16>((value >> shift) & 0xFFFFu);
}

Fortress::Core::uint8 FPciConfigX86::Read8(Fortress::Core::uint8 bus,
                                           Fortress::Core::uint8 device,
                                           Fortress::Core::uint8 function,
                                           Fortress::Core::uint8 offset) {
    const Fortress::Core::uint32 value = Read32(bus, device, function, offset);
    const Fortress::Core::uint32 shift = static_cast<Fortress::Core::uint32>((offset & 0x3u) * 8u);
    return static_cast<Fortress::Core::uint8>((value >> shift) & 0xFFu);
}

} // namespace Fortress::Platform
