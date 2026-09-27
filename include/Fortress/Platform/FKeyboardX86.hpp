#ifndef FORTRESS_PLATFORM_FKEYBOARDX86_HPP
#define FORTRESS_PLATFORM_FKEYBOARDX86_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Platform {

struct FKeyEvent {
    bool Pressed;
    char Ascii;
  Fortress::Core::uint8 ScanCode;
  bool ShiftActive;
};

class FKeyboardX86 {
  public:
    static bool Initialize();
    static bool PollEvent(FKeyEvent &outEvent);
};

} // namespace Fortress::Platform

#endif
