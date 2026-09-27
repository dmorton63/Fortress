#ifndef FORTRESS_PLATFORM_FXHCIMMIOREGISTERS_HPP
#define FORTRESS_PLATFORM_FXHCIMMIOREGISTERS_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Platform {

class FXhciMmioRegisters {
  public:
    bool Initialize(Fortress::Core::uint64 capabilityBaseVirtualAddress);

    Fortress::Core::uint64 GetCapabilityBase() const;
    Fortress::Core::uint64 GetOperationalBase() const;
    Fortress::Core::uint64 GetDoorbellBase() const;
    Fortress::Core::uint64 GetRuntimeBase() const;

    Fortress::Core::uint8 ReadCapLength() const;
    Fortress::Core::uint16 ReadHciVersion() const;
    Fortress::Core::uint32 ReadHcsParams1() const;
    Fortress::Core::uint32 ReadHcsParams2() const;
    Fortress::Core::uint32 ReadHcsParams3() const;
    Fortress::Core::uint32 ReadHccParams1() const;
    Fortress::Core::uint32 ReadDbOff() const;
    Fortress::Core::uint32 ReadRtsOff() const;

    Fortress::Core::uint32 ReadUsbCmd() const;
    void WriteUsbCmd(Fortress::Core::uint32 value) const;
    Fortress::Core::uint32 ReadUsbSts() const;
    Fortress::Core::uint32 ReadPageSize() const;
    Fortress::Core::uint64 ReadCrcr() const;
    void WriteCrcr(Fortress::Core::uint64 value) const;
    Fortress::Core::uint64 ReadDcbaap() const;
    void WriteDcbaap(Fortress::Core::uint64 value) const;
    Fortress::Core::uint32 ReadConfig() const;
    void WriteConfig(Fortress::Core::uint32 value) const;

    Fortress::Core::uint32 ReadInterrupterIman(Fortress::Core::uint8 interrupterIndex) const;
    void WriteInterrupterIman(Fortress::Core::uint8 interrupterIndex, Fortress::Core::uint32 value) const;
    Fortress::Core::uint32 ReadInterrupterImod(Fortress::Core::uint8 interrupterIndex) const;
    void WriteInterrupterImod(Fortress::Core::uint8 interrupterIndex, Fortress::Core::uint32 value) const;
    Fortress::Core::uint32 ReadInterrupterErstsz(Fortress::Core::uint8 interrupterIndex) const;
    void WriteInterrupterErstsz(Fortress::Core::uint8 interrupterIndex, Fortress::Core::uint32 value) const;
    Fortress::Core::uint64 ReadInterrupterErstba(Fortress::Core::uint8 interrupterIndex) const;
    void WriteInterrupterErstba(Fortress::Core::uint8 interrupterIndex, Fortress::Core::uint64 value) const;
    Fortress::Core::uint64 ReadInterrupterErdp(Fortress::Core::uint8 interrupterIndex) const;
    void WriteInterrupterErdp(Fortress::Core::uint8 interrupterIndex, Fortress::Core::uint64 value) const;

    Fortress::Core::uint32 ReadPortSc(Fortress::Core::uint8 portNumber1Based) const;

    void RingDoorbell(Fortress::Core::uint8 slotId,
                      Fortress::Core::uint8 endpointTarget,
                      Fortress::Core::uint16 streamId = 0) const;

  private:
    Fortress::Core::uint64 CapabilityBase = 0;
    Fortress::Core::uint64 OperationalBase = 0;
    Fortress::Core::uint64 DoorbellBase = 0;
    Fortress::Core::uint64 RuntimeBase = 0;
};

} // namespace Fortress::Platform

#endif
