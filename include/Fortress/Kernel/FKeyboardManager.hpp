#ifndef FORTRESS_KERNEL_FKEYBOARDMANAGER_HPP
#define FORTRESS_KERNEL_FKEYBOARDMANAGER_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

enum class EKeyboardLayout : Fortress::Core::uint8 {
  UsQwerty = 0,
  UsDvorak = 1,
};

enum class EKeyboardKey : Fortress::Core::uint8 {
  Unknown = 0,
  Enter,
  Backspace,
  Space,
  Digit,
  Letter,
  Symbol,
};

enum EKeyboardModifierFlags : Fortress::Core::uint8 {
  KeyboardModifierNone = 0,
  KeyboardModifierShift = 1u << 0,
};

struct FKeyboardInputEvent {
    bool Pressed = false;
    char Ascii = 0;
};

struct FKeyboardKeyEvent {
  bool Pressed = false;
  char Ascii = 0;
  Fortress::Core::uint8 ScanCode = 0;
  Fortress::Core::uint8 Modifiers = KeyboardModifierNone;
  EKeyboardKey Key = EKeyboardKey::Unknown;
};

class FKeyboardManager {
  public:
    static bool Initialize();
    static bool PollKeyEvent(FKeyboardKeyEvent &OutEvent);
    static bool PollEvent(FKeyboardInputEvent &OutEvent);
    static bool EnqueueSyntheticKeyEvent(const FKeyboardKeyEvent &event);
    static void SetLayout(EKeyboardLayout Layout);
    static EKeyboardLayout GetLayout();
    static const char *GetLayoutName();
    static Fortress::Core::uint8 GetModifierFlags();
};

} // namespace Fortress::Kernel

#endif