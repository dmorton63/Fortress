#include "Fortress/Platform/FXhciMmioRegisters.hpp"

#include "Fortress/Memory/FMmio.hpp"

namespace Fortress::Platform {

using Fortress::Memory::FMmio;

static constexpr Fortress::Core::uint64 CapLengthOffset = 0x00;
static constexpr Fortress::Core::uint64 HciVersionOffset = 0x02;
static constexpr Fortress::Core::uint64 HcsParams1Offset = 0x04;
static constexpr Fortress::Core::uint64 HcsParams2Offset = 0x08;
static constexpr Fortress::Core::uint64 HcsParams3Offset = 0x0C;
static constexpr Fortress::Core::uint64 HccParams1Offset = 0x10;
static constexpr Fortress::Core::uint64 DbOffOffset = 0x14;
static constexpr Fortress::Core::uint64 RtsOffOffset = 0x18;

static constexpr Fortress::Core::uint64 OpUsbCmdOffset = 0x00;
static constexpr Fortress::Core::uint64 OpUsbStsOffset = 0x04;
static constexpr Fortress::Core::uint64 OpPageSizeOffset = 0x08;
static constexpr Fortress::Core::uint64 OpCrcrOffset = 0x18;
static constexpr Fortress::Core::uint64 OpDcbaapOffset = 0x30;
static constexpr Fortress::Core::uint64 OpConfigOffset = 0x38;
static constexpr Fortress::Core::uint64 OpPortRegBaseOffset = 0x400;
static constexpr Fortress::Core::uint64 OpPortRegStride = 0x10;
static constexpr Fortress::Core::uint64 OpPortScOffset = 0x00;

static constexpr Fortress::Core::uint64 RtInterrupterSetBaseOffset = 0x20;
static constexpr Fortress::Core::uint64 RtInterrupterStride = 0x20;
static constexpr Fortress::Core::uint64 RtImanOffset = 0x00;
static constexpr Fortress::Core::uint64 RtImodOffset = 0x04;
static constexpr Fortress::Core::uint64 RtErstszOffset = 0x08;
static constexpr Fortress::Core::uint64 RtErstbaOffset = 0x10;
static constexpr Fortress::Core::uint64 RtErdpOffset = 0x18;

static Fortress::Core::uint64 InterrupterOffset(Fortress::Core::uint8 interrupterIndex) {
    return RtInterrupterSetBaseOffset + static_cast<Fortress::Core::uint64>(interrupterIndex) * RtInterrupterStride;
}

bool FXhciMmioRegisters::Initialize(Fortress::Core::uint64 capabilityBaseVirtualAddress) {
    if (capabilityBaseVirtualAddress == 0) {
        return false;
    }

    CapabilityBase = capabilityBaseVirtualAddress;

    const Fortress::Core::uint64 capLength = static_cast<Fortress::Core::uint64>(ReadCapLength());
    const Fortress::Core::uint64 dbOff = static_cast<Fortress::Core::uint64>(ReadDbOff() & ~0x3u);
    const Fortress::Core::uint64 rtsOff = static_cast<Fortress::Core::uint64>(ReadRtsOff() & ~0x1Fu);

    OperationalBase = CapabilityBase + capLength;
    DoorbellBase = CapabilityBase + dbOff;
    RuntimeBase = CapabilityBase + rtsOff;
    return true;
}

Fortress::Core::uint64 FXhciMmioRegisters::GetCapabilityBase() const {
    return CapabilityBase;
}

Fortress::Core::uint64 FXhciMmioRegisters::GetOperationalBase() const {
    return OperationalBase;
}

Fortress::Core::uint64 FXhciMmioRegisters::GetDoorbellBase() const {
    return DoorbellBase;
}

Fortress::Core::uint64 FXhciMmioRegisters::GetRuntimeBase() const {
    return RuntimeBase;
}

Fortress::Core::uint8 FXhciMmioRegisters::ReadCapLength() const {
    return FMmio::Read8(CapabilityBase + CapLengthOffset);
}

Fortress::Core::uint16 FXhciMmioRegisters::ReadHciVersion() const {
    return FMmio::Read16(CapabilityBase + HciVersionOffset);
}

Fortress::Core::uint32 FXhciMmioRegisters::ReadHcsParams1() const {
    return FMmio::Read32(CapabilityBase + HcsParams1Offset);
}

Fortress::Core::uint32 FXhciMmioRegisters::ReadHcsParams2() const {
    return FMmio::Read32(CapabilityBase + HcsParams2Offset);
}

Fortress::Core::uint32 FXhciMmioRegisters::ReadHcsParams3() const {
    return FMmio::Read32(CapabilityBase + HcsParams3Offset);
}

Fortress::Core::uint32 FXhciMmioRegisters::ReadHccParams1() const {
    return FMmio::Read32(CapabilityBase + HccParams1Offset);
}

Fortress::Core::uint32 FXhciMmioRegisters::ReadDbOff() const {
    return FMmio::Read32(CapabilityBase + DbOffOffset);
}

Fortress::Core::uint32 FXhciMmioRegisters::ReadRtsOff() const {
    return FMmio::Read32(CapabilityBase + RtsOffOffset);
}

Fortress::Core::uint32 FXhciMmioRegisters::ReadUsbCmd() const {
    return FMmio::Read32(OperationalBase + OpUsbCmdOffset);
}

void FXhciMmioRegisters::WriteUsbCmd(Fortress::Core::uint32 value) const {
    FMmio::Write32(OperationalBase + OpUsbCmdOffset, value);
}

Fortress::Core::uint32 FXhciMmioRegisters::ReadUsbSts() const {
    return FMmio::Read32(OperationalBase + OpUsbStsOffset);
}

Fortress::Core::uint32 FXhciMmioRegisters::ReadPageSize() const {
    return FMmio::Read32(OperationalBase + OpPageSizeOffset);
}

Fortress::Core::uint64 FXhciMmioRegisters::ReadCrcr() const {
    return FMmio::Read64(OperationalBase + OpCrcrOffset);
}

void FXhciMmioRegisters::WriteCrcr(Fortress::Core::uint64 value) const {
    FMmio::Write64(OperationalBase + OpCrcrOffset, value);
}

Fortress::Core::uint64 FXhciMmioRegisters::ReadDcbaap() const {
    return FMmio::Read64(OperationalBase + OpDcbaapOffset);
}

void FXhciMmioRegisters::WriteDcbaap(Fortress::Core::uint64 value) const {
    FMmio::Write64(OperationalBase + OpDcbaapOffset, value);
}

Fortress::Core::uint32 FXhciMmioRegisters::ReadConfig() const {
    return FMmio::Read32(OperationalBase + OpConfigOffset);
}

void FXhciMmioRegisters::WriteConfig(Fortress::Core::uint32 value) const {
    FMmio::Write32(OperationalBase + OpConfigOffset, value);
}

Fortress::Core::uint32 FXhciMmioRegisters::ReadInterrupterIman(Fortress::Core::uint8 interrupterIndex) const {
    return FMmio::Read32(RuntimeBase + InterrupterOffset(interrupterIndex) + RtImanOffset);
}

void FXhciMmioRegisters::WriteInterrupterIman(Fortress::Core::uint8 interrupterIndex, Fortress::Core::uint32 value) const {
    FMmio::Write32(RuntimeBase + InterrupterOffset(interrupterIndex) + RtImanOffset, value);
}

Fortress::Core::uint32 FXhciMmioRegisters::ReadInterrupterImod(Fortress::Core::uint8 interrupterIndex) const {
    return FMmio::Read32(RuntimeBase + InterrupterOffset(interrupterIndex) + RtImodOffset);
}

void FXhciMmioRegisters::WriteInterrupterImod(Fortress::Core::uint8 interrupterIndex, Fortress::Core::uint32 value) const {
    FMmio::Write32(RuntimeBase + InterrupterOffset(interrupterIndex) + RtImodOffset, value);
}

Fortress::Core::uint32 FXhciMmioRegisters::ReadInterrupterErstsz(Fortress::Core::uint8 interrupterIndex) const {
    return FMmio::Read32(RuntimeBase + InterrupterOffset(interrupterIndex) + RtErstszOffset);
}

void FXhciMmioRegisters::WriteInterrupterErstsz(Fortress::Core::uint8 interrupterIndex, Fortress::Core::uint32 value) const {
    FMmio::Write32(RuntimeBase + InterrupterOffset(interrupterIndex) + RtErstszOffset, value);
}

Fortress::Core::uint64 FXhciMmioRegisters::ReadInterrupterErstba(Fortress::Core::uint8 interrupterIndex) const {
    return FMmio::Read64(RuntimeBase + InterrupterOffset(interrupterIndex) + RtErstbaOffset);
}

void FXhciMmioRegisters::WriteInterrupterErstba(Fortress::Core::uint8 interrupterIndex, Fortress::Core::uint64 value) const {
    FMmio::Write64(RuntimeBase + InterrupterOffset(interrupterIndex) + RtErstbaOffset, value);
}

Fortress::Core::uint64 FXhciMmioRegisters::ReadInterrupterErdp(Fortress::Core::uint8 interrupterIndex) const {
    return FMmio::Read64(RuntimeBase + InterrupterOffset(interrupterIndex) + RtErdpOffset);
}

void FXhciMmioRegisters::WriteInterrupterErdp(Fortress::Core::uint8 interrupterIndex, Fortress::Core::uint64 value) const {
    FMmio::Write64(RuntimeBase + InterrupterOffset(interrupterIndex) + RtErdpOffset, value);
}

Fortress::Core::uint32 FXhciMmioRegisters::ReadPortSc(Fortress::Core::uint8 portNumber1Based) const {
    if (portNumber1Based == 0) {
        return 0;
    }
    const Fortress::Core::uint64 offset = OpPortRegBaseOffset +
                                           (static_cast<Fortress::Core::uint64>(portNumber1Based - 1u) * OpPortRegStride) +
                                           OpPortScOffset;
    return FMmio::Read32(OperationalBase + offset);
}

void FXhciMmioRegisters::RingDoorbell(Fortress::Core::uint8 slotId,
                                      Fortress::Core::uint8 endpointTarget,
                                      Fortress::Core::uint16 streamId) const {
    const Fortress::Core::uint32 value = static_cast<Fortress::Core::uint32>(endpointTarget) |
                                         (static_cast<Fortress::Core::uint32>(streamId) << 16u);
    FMmio::WriteBarrier();
    FMmio::Write32(DoorbellBase + static_cast<Fortress::Core::uint64>(slotId) * 4ull, value);
}

} // namespace Fortress::Platform
