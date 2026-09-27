#ifndef FORTRESS_CPU_FMEMORYBARRIERSX64_HPP
#define FORTRESS_CPU_FMEMORYBARRIERSX64_HPP

namespace Fortress::Cpu {

class FMemoryBarriersX64 {
  public:
    static inline void Compiler() {
        __asm__ volatile("" ::: "memory");
    }

    static inline void Read() {
        __asm__ volatile("lfence" ::: "memory");
    }

    static inline void Write() {
        __asm__ volatile("sfence" ::: "memory");
    }

    static inline void Full() {
        __asm__ volatile("mfence" ::: "memory");
    }
};

} // namespace Fortress::Cpu

#endif
