#include <cstddef>
#include <cstdint>

#include "Fortress/Kernel/FKernelBootRequests.hpp"
#include "Fortress/Kernel/FKernelBootstrap.hpp"
#include "Fortress/Kernel/FKernelRuntimeLoop.hpp"

using Fortress::Kernel::AreBootRequestsSupported;
using Fortress::Kernel::FKernelBootstrap;
using Fortress::Kernel::FKernelRuntimeContext;

extern "C" void _start() {
    if (!AreBootRequestsSupported()) {
        FKernelBootstrap::HaltForever();
    }

    FKernelBootstrap::EnableFPUAndSSE();

    FKernelRuntimeContext runtime{};
    if (!FKernelBootstrap::Initialize(
            Fortress::Kernel::GetFramebufferResponse(),
            Fortress::Kernel::GetMemmapResponse(),
            Fortress::Kernel::GetHhdmResponse(),
            Fortress::Kernel::GetSmpResponse(),
            runtime)) {
        FKernelBootstrap::HaltForever();
    }

    Fortress::Kernel::RunRuntimeLoop(runtime);
}
