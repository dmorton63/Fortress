#ifndef FORTRESS_KERNEL_FKERNELBOOTREQUESTS_HPP
#define FORTRESS_KERNEL_FKERNELBOOTREQUESTS_HPP

#include "limine.h"

namespace Fortress::Kernel {

bool AreBootRequestsSupported();

const limine_framebuffer_response *GetFramebufferResponse();
const limine_memmap_response *GetMemmapResponse();
const limine_hhdm_response *GetHhdmResponse();
const limine_smp_response *GetSmpResponse();

} // namespace Fortress::Kernel

#endif
