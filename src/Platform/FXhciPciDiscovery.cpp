#include "Fortress/Platform/FXhciPciDiscovery.hpp"

#include "Fortress/Platform/FPciConfigX86.hpp"

namespace Fortress::Platform {

static constexpr Fortress::Core::uint8 ClassSerialBus = 0x0Cu;
static constexpr Fortress::Core::uint8 SubclassUsb = 0x03u;
static constexpr Fortress::Core::uint8 ProgIfXhci = 0x30u;

static constexpr Fortress::Core::uint8 PciOffsetVendorId = 0x00u;
static constexpr Fortress::Core::uint8 PciOffsetHeaderType = 0x0Eu;
static constexpr Fortress::Core::uint8 PciOffsetClassCode = 0x0Bu;
static constexpr Fortress::Core::uint8 PciOffsetSubclass = 0x0Au;
static constexpr Fortress::Core::uint8 PciOffsetProgIf = 0x09u;
static constexpr Fortress::Core::uint8 PciOffsetBar0 = 0x10u;
static constexpr Fortress::Core::uint8 PciOffsetBar1 = 0x14u;

static bool ReadControllerMmioBase(Fortress::Core::uint8 bus,
                                   Fortress::Core::uint8 device,
                                   Fortress::Core::uint8 function,
                                   Fortress::Core::uint64 &outBase) {
    outBase = 0;

    const Fortress::Core::uint32 bar0 = FPciConfigX86::Read32(bus, device, function, PciOffsetBar0);
    if (bar0 == 0 || bar0 == 0xFFFFFFFFu) {
        return false;
    }

    if ((bar0 & 0x1u) != 0) {
        return false;
    }

    const Fortress::Core::uint32 memoryType = (bar0 >> 1u) & 0x3u;
    if (memoryType == 0x2u) {
        const Fortress::Core::uint32 bar1 = FPciConfigX86::Read32(bus, device, function, PciOffsetBar1);
        if (bar1 == 0xFFFFFFFFu) {
            return false;
        }

        outBase = (static_cast<Fortress::Core::uint64>(bar1) << 32u) |
                  static_cast<Fortress::Core::uint64>(bar0 & 0xFFFFFFF0u);
        return true;
    }

    outBase = static_cast<Fortress::Core::uint64>(bar0 & 0xFFFFFFF0u);
    return true;
}

bool FXhciPciDiscovery::DiscoverFirst(FXhciControllerInfo &outInfo) {
    outInfo = FXhciControllerInfo{};

    for (Fortress::Core::uint16 bus = 0; bus < 256; bus++) {
        for (Fortress::Core::uint8 device = 0; device < 32; device++) {
            const Fortress::Core::uint16 vendor0 =
                FPciConfigX86::Read16(static_cast<Fortress::Core::uint8>(bus), device, 0, PciOffsetVendorId);
            if (vendor0 == 0xFFFFu) {
                continue;
            }

            const Fortress::Core::uint8 headerType =
                FPciConfigX86::Read8(static_cast<Fortress::Core::uint8>(bus), device, 0, PciOffsetHeaderType);
            const Fortress::Core::uint8 functionCount = ((headerType & 0x80u) != 0) ? 8u : 1u;

            for (Fortress::Core::uint8 function = 0; function < functionCount; function++) {
                const Fortress::Core::uint16 vendorId =
                    FPciConfigX86::Read16(static_cast<Fortress::Core::uint8>(bus), device, function, PciOffsetVendorId);
                if (vendorId == 0xFFFFu) {
                    continue;
                }

                const Fortress::Core::uint8 classCode =
                    FPciConfigX86::Read8(static_cast<Fortress::Core::uint8>(bus), device, function, PciOffsetClassCode);
                const Fortress::Core::uint8 subclass =
                    FPciConfigX86::Read8(static_cast<Fortress::Core::uint8>(bus), device, function, PciOffsetSubclass);
                const Fortress::Core::uint8 progIf =
                    FPciConfigX86::Read8(static_cast<Fortress::Core::uint8>(bus), device, function, PciOffsetProgIf);

                if (classCode != ClassSerialBus || subclass != SubclassUsb || progIf != ProgIfXhci) {
                    continue;
                }

                Fortress::Core::uint64 mmioBase = 0;
                if (!ReadControllerMmioBase(static_cast<Fortress::Core::uint8>(bus), device, function, mmioBase)) {
                    continue;
                }

                outInfo = FXhciControllerInfo{
                    .Bus = static_cast<Fortress::Core::uint8>(bus),
                    .Device = device,
                    .Function = function,
                    .VendorId = vendorId,
                    .DeviceId = FPciConfigX86::Read16(static_cast<Fortress::Core::uint8>(bus), device, function, 0x02u),
                    .ProgramInterface = progIf,
                    .MmioBase = mmioBase,
                    .MmioSize = 0x4000ull,
                    .Found = true,
                };
                return true;
            }
        }
    }

    return false;
}

} // namespace Fortress::Platform
