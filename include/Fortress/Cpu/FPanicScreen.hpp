#ifndef FORTRESS_CPU_FPANICSCREEN_HPP
#define FORTRESS_CPU_FPANICSCREEN_HPP

#include "Fortress/Core/FTypes.hpp"

struct limine_framebuffer;

namespace Fortress::Cpu {

class FPanicScreen {
  public:
    static void Initialize(limine_framebuffer *framebuffer);
    static void OnException(Fortress::Core::uint64 vector, Fortress::Core::uint64 errorCode);
};

} // namespace Fortress::Cpu

#endif
