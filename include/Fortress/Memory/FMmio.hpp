#ifndef FORTRESS_MEMORY_FMMIO_HPP
#define FORTRESS_MEMORY_FMMIO_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Cpu/FMemoryBarriersX64.hpp"

namespace Fortress::Memory {

class FMmio {
  public:
    static inline Fortress::Core::uint8 Read8(Fortress::Core::uint64 address) {
        Fortress::Cpu::FMemoryBarriersX64::Compiler();
        const auto *ptr = reinterpret_cast<volatile const Fortress::Core::uint8 *>(address);
        const Fortress::Core::uint8 value = *ptr;
        Fortress::Cpu::FMemoryBarriersX64::Read();
        return value;
    }

    static inline Fortress::Core::uint16 Read16(Fortress::Core::uint64 address) {
        Fortress::Cpu::FMemoryBarriersX64::Compiler();
        const auto *ptr = reinterpret_cast<volatile const Fortress::Core::uint16 *>(address);
        const Fortress::Core::uint16 value = *ptr;
        Fortress::Cpu::FMemoryBarriersX64::Read();
        return value;
    }

    static inline Fortress::Core::uint32 Read32(Fortress::Core::uint64 address) {
        Fortress::Cpu::FMemoryBarriersX64::Compiler();
        const auto *ptr = reinterpret_cast<volatile const Fortress::Core::uint32 *>(address);
        const Fortress::Core::uint32 value = *ptr;
        Fortress::Cpu::FMemoryBarriersX64::Read();
        return value;
    }

    static inline Fortress::Core::uint64 Read64(Fortress::Core::uint64 address) {
        Fortress::Cpu::FMemoryBarriersX64::Compiler();
        const auto *ptr = reinterpret_cast<volatile const Fortress::Core::uint64 *>(address);
        const Fortress::Core::uint64 value = *ptr;
        Fortress::Cpu::FMemoryBarriersX64::Read();
        return value;
    }

    static inline void Write8(Fortress::Core::uint64 address, Fortress::Core::uint8 value) {
        Fortress::Cpu::FMemoryBarriersX64::Compiler();
        auto *ptr = reinterpret_cast<volatile Fortress::Core::uint8 *>(address);
        *ptr = value;
        Fortress::Cpu::FMemoryBarriersX64::Write();
    }

    static inline void Write16(Fortress::Core::uint64 address, Fortress::Core::uint16 value) {
        Fortress::Cpu::FMemoryBarriersX64::Compiler();
        auto *ptr = reinterpret_cast<volatile Fortress::Core::uint16 *>(address);
        *ptr = value;
        Fortress::Cpu::FMemoryBarriersX64::Write();
    }

    static inline void Write32(Fortress::Core::uint64 address, Fortress::Core::uint32 value) {
        Fortress::Cpu::FMemoryBarriersX64::Compiler();
        auto *ptr = reinterpret_cast<volatile Fortress::Core::uint32 *>(address);
        *ptr = value;
        Fortress::Cpu::FMemoryBarriersX64::Write();
    }

    static inline void Write64(Fortress::Core::uint64 address, Fortress::Core::uint64 value) {
        Fortress::Cpu::FMemoryBarriersX64::Compiler();
        auto *ptr = reinterpret_cast<volatile Fortress::Core::uint64 *>(address);
        *ptr = value;
        Fortress::Cpu::FMemoryBarriersX64::Write();
    }

    static inline void ReadBarrier() {
        Fortress::Cpu::FMemoryBarriersX64::Read();
    }

    static inline void WriteBarrier() {
        Fortress::Cpu::FMemoryBarriersX64::Write();
    }

    static inline void FullBarrier() {
        Fortress::Cpu::FMemoryBarriersX64::Full();
    }
};

} // namespace Fortress::Memory

#endif
