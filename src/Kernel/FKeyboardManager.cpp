#include "Fortress/Kernel/FKeyboardManager.hpp"

#include "Fortress/Platform/FKeyboardX86.hpp"

namespace Fortress::Kernel {

static EKeyboardLayout GKeyboardLayout = EKeyboardLayout::UsQwerty;
static Fortress::Core::uint8 GModifierFlags = KeyboardModifierNone;
static constexpr Fortress::Core::uint32 GSyntheticQueueCapacity = 64u;
static FKeyboardKeyEvent GSyntheticQueue[GSyntheticQueueCapacity] = {};
static Fortress::Core::uint32 GSyntheticQueueHead = 0u;
static Fortress::Core::uint32 GSyntheticQueueTail = 0u;
static Fortress::Core::uint32 GSyntheticQueueCount = 0u;

static bool DequeueSyntheticKeyEvent(FKeyboardKeyEvent &outEvent) {
    if (GSyntheticQueueCount == 0u) {
        return false;
    }

    outEvent = GSyntheticQueue[GSyntheticQueueHead];
    GSyntheticQueueHead = (GSyntheticQueueHead + 1u) % GSyntheticQueueCapacity;
    GSyntheticQueueCount--;
    return true;
}

static char TranslateDvorakAscii(char Character) {
    switch (Character) {
    case 'q': return '\'';
    case 'w': return ',';
    case 'e': return '.';
    case 'r': return 'p';
    case 't': return 'y';
    case 'y': return 'f';
    case 'u': return 'g';
    case 'i': return 'c';
    case 'o': return 'r';
    case 'p': return 'l';
    case 'a': return 'a';
    case 's': return 'o';
    case 'd': return 'e';
    case 'f': return 'u';
    case 'g': return 'i';
    case 'h': return 'd';
    case 'j': return 'h';
    case 'k': return 't';
    case 'l': return 'n';
    case 'z': return ';';
    case 'x': return 'q';
    case 'c': return 'j';
    case 'v': return 'k';
    case 'b': return 'x';
    case 'n': return 'b';
    case 'm': return 'm';
    case 'Q': return '"';
    case 'W': return '<';
    case 'E': return '>';
    case 'R': return 'P';
    case 'T': return 'Y';
    case 'Y': return 'F';
    case 'U': return 'G';
    case 'I': return 'C';
    case 'O': return 'R';
    case 'P': return 'L';
    case 'A': return 'A';
    case 'S': return 'O';
    case 'D': return 'E';
    case 'F': return 'U';
    case 'G': return 'I';
    case 'H': return 'D';
    case 'J': return 'H';
    case 'K': return 'T';
    case 'L': return 'N';
    case 'Z': return ':';
    case 'X': return 'Q';
    case 'C': return 'J';
    case 'V': return 'K';
    case 'B': return 'X';
    case 'N': return 'B';
    case 'M': return 'M';
    default:
        return Character;
    }
}

static char TranslateAsciiForLayout(char Character, EKeyboardLayout Layout) {
    switch (Layout) {
    case EKeyboardLayout::UsDvorak:
        return TranslateDvorakAscii(Character);
    case EKeyboardLayout::UsQwerty:
    default:
        return Character;
    }
}

static EKeyboardKey ClassifyKey(Fortress::Core::uint8 ScanCode, char Ascii) {
    if (ScanCode == 0x1Cu) {
        return EKeyboardKey::Enter;
    }

    if (ScanCode == 0x0Eu) {
        return EKeyboardKey::Backspace;
    }

    if (ScanCode == 0x39u) {
        return EKeyboardKey::Space;
    }

    if (Ascii >= '0' && Ascii <= '9') {
        return EKeyboardKey::Digit;
    }

    if ((Ascii >= 'a' && Ascii <= 'z') || (Ascii >= 'A' && Ascii <= 'Z')) {
        return EKeyboardKey::Letter;
    }

    if (Ascii != 0) {
        return EKeyboardKey::Symbol;
    }

    return EKeyboardKey::Unknown;
}

bool FKeyboardManager::Initialize() {
    GKeyboardLayout = EKeyboardLayout::UsQwerty;
    GModifierFlags = KeyboardModifierNone;
    GSyntheticQueueHead = 0u;
    GSyntheticQueueTail = 0u;
    GSyntheticQueueCount = 0u;
    return Fortress::Platform::FKeyboardX86::Initialize();
}

bool FKeyboardManager::PollKeyEvent(FKeyboardKeyEvent &OutEvent) {
    if (DequeueSyntheticKeyEvent(OutEvent)) {
        GModifierFlags = OutEvent.Modifiers;
        return true;
    }

    Fortress::Platform::FKeyEvent PlatformEvent{};
    if (!Fortress::Platform::FKeyboardX86::PollEvent(PlatformEvent)) {
        return false;
    }

    const char TranslatedAscii = TranslateAsciiForLayout(PlatformEvent.Ascii, GKeyboardLayout);
    GModifierFlags = PlatformEvent.ShiftActive ? KeyboardModifierShift : KeyboardModifierNone;

    OutEvent = FKeyboardKeyEvent{};
    OutEvent.Pressed = PlatformEvent.Pressed;
    OutEvent.Ascii = TranslatedAscii;
    OutEvent.ScanCode = PlatformEvent.ScanCode;
    OutEvent.Modifiers = GModifierFlags;
    OutEvent.Key = ClassifyKey(PlatformEvent.ScanCode, TranslatedAscii);
    return true;
}

bool FKeyboardManager::PollEvent(FKeyboardInputEvent &OutEvent) {
    FKeyboardKeyEvent KeyEvent{};
    if (!PollKeyEvent(KeyEvent)) {
        return false;
    }

    OutEvent.Pressed = KeyEvent.Pressed;
    OutEvent.Ascii = KeyEvent.Ascii;
    return true;
}

void FKeyboardManager::SetLayout(EKeyboardLayout Layout) {
    GKeyboardLayout = Layout;
}

EKeyboardLayout FKeyboardManager::GetLayout() {
    return GKeyboardLayout;
}

const char *FKeyboardManager::GetLayoutName() {
    switch (GKeyboardLayout) {
    case EKeyboardLayout::UsDvorak:
        return "US-DVORAK";
    case EKeyboardLayout::UsQwerty:
    default:
        return "US-QWERTY";
    }
}

Fortress::Core::uint8 FKeyboardManager::GetModifierFlags() {
    return GModifierFlags;
}

bool FKeyboardManager::EnqueueSyntheticKeyEvent(const FKeyboardKeyEvent &event) {
    if (GSyntheticQueueCount >= GSyntheticQueueCapacity) {
        return false;
    }

    GSyntheticQueue[GSyntheticQueueTail] = event;
    GSyntheticQueueTail = (GSyntheticQueueTail + 1u) % GSyntheticQueueCapacity;
    GSyntheticQueueCount++;
    return true;
}

} // namespace Fortress::Kernel