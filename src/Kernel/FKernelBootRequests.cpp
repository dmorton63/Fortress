#include "Fortress/Kernel/FKernelBootRequests.hpp"

namespace Fortress::Kernel {

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request GFramebufferRequest = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = nullptr,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request GMemmapRequest = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = nullptr,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request GHhdmRequest = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = nullptr,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_smp_request GSmpRequest = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0,
    .response = nullptr,
    .flags = 0,
};

__attribute__((used, section(".limine_requests_start_marker")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end_marker")))
static volatile LIMINE_REQUESTS_END_MARKER;

bool AreBootRequestsSupported() {
    return LIMINE_BASE_REVISION_SUPPORTED;
}

const limine_framebuffer_response *GetFramebufferResponse() {
    return GFramebufferRequest.response;
}

const limine_memmap_response *GetMemmapResponse() {
    return GMemmapRequest.response;
}

const limine_hhdm_response *GetHhdmResponse() {
    return GHhdmRequest.response;
}

const limine_smp_response *GetSmpResponse() {
    return GSmpRequest.response;
}

} // namespace Fortress::Kernel
