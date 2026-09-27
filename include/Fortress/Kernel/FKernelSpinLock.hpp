#ifndef FORTRESS_KERNEL_FKERNELSPINLOCK_HPP
#define FORTRESS_KERNEL_FKERNELSPINLOCK_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Cpu/FMemoryBarriersX64.hpp"

namespace Fortress::Kernel {

class FKernelSpinLock {
  public:
    void Acquire() {
        while (__atomic_test_and_set(&GState, __ATOMIC_ACQUIRE)) {
            while (__atomic_load_n(&GState, __ATOMIC_RELAXED) != 0u) {
                __asm__ volatile("pause");
            }
        }
        Fortress::Cpu::FMemoryBarriersX64::Compiler();
    }

    bool TryAcquire() {
        if (__atomic_test_and_set(&GState, __ATOMIC_ACQUIRE)) {
            return false;
        }
        Fortress::Cpu::FMemoryBarriersX64::Compiler();
        return true;
    }

    void Release() {
        Fortress::Cpu::FMemoryBarriersX64::Compiler();
        __atomic_clear(&GState, __ATOMIC_RELEASE);
    }

  private:
    Fortress::Core::uint8 GState = 0u;
};

} // namespace Fortress::Kernel

#endif
