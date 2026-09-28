#include "Fortress/Platform/FKeyboardX86.hpp"

namespace Fortress::Platform {

static bool GLeftShiftDown = false;
static bool GRightShiftDown = false;

static inline void Out8(Fortress::Core::uint16 port, Fortress::Core::uint8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline Fortress::Core::uint8 In8(Fortress::Core::uint16 port) {
    Fortress::Core::uint8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static char ScancodeToAscii(Fortress::Core::uint8 scanCode, bool shiftDown) {
    switch (scanCode) {
        case 0x02: return shiftDown ? '!' : '1';
        case 0x03: return shiftDown ? '@' : '2';
        case 0x04: return shiftDown ? '#' : '3';
        case 0x05: return shiftDown ? '$' : '4';
        case 0x06: return shiftDown ? '%' : '5';
        case 0x07: return shiftDown ? '^' : '6';
        case 0x08: return shiftDown ? '&' : '7';
        case 0x09: return shiftDown ? '*' : '8';
        case 0x0A: return shiftDown ? '(' : '9';
        case 0x0B: return shiftDown ? ')' : '0';
        case 0x10: return shiftDown ? 'Q' : 'q';
        case 0x11: return shiftDown ? 'W' : 'w';
        case 0x12: return shiftDown ? 'E' : 'e';
        case 0x13: return shiftDown ? 'R' : 'r';
        case 0x14: return shiftDown ? 'T' : 't';
        case 0x15: return shiftDown ? 'Y' : 'y';
        case 0x16: return shiftDown ? 'U' : 'u';
        case 0x17: return shiftDown ? 'I' : 'i';
        case 0x18: return shiftDown ? 'O' : 'o';
        case 0x19: return shiftDown ? 'P' : 'p';
        case 0x1A: return shiftDown ? '{' : '[';
        case 0x1B: return shiftDown ? '}' : ']';
        case 0x1E: return shiftDown ? 'A' : 'a';
        case 0x1F: return shiftDown ? 'S' : 's';
        case 0x20: return shiftDown ? 'D' : 'd';
        case 0x21: return shiftDown ? 'F' : 'f';
        case 0x22: return shiftDown ? 'G' : 'g';
        case 0x23: return shiftDown ? 'H' : 'h';
        case 0x24: return shiftDown ? 'J' : 'j';
        case 0x25: return shiftDown ? 'K' : 'k';
        case 0x26: return shiftDown ? 'L' : 'l';
        case 0x27: return shiftDown ? ':' : ';';
        case 0x28: return shiftDown ? '"' : '\'';
        case 0x29: return shiftDown ? '~' : '`';
        case 0x2B: return shiftDown ? '|' : '\\';
        case 0x2C: return shiftDown ? 'Z' : 'z';
        case 0x2D: return shiftDown ? 'X' : 'x';
        case 0x2E: return shiftDown ? 'C' : 'c';
        case 0x2F: return shiftDown ? 'V' : 'v';
        case 0x30: return shiftDown ? 'B' : 'b';
        case 0x31: return shiftDown ? 'N' : 'n';
        case 0x32: return shiftDown ? 'M' : 'm';
        case 0x33: return shiftDown ? '<' : ',';
        case 0x34: return shiftDown ? '>' : '.';
        case 0x35: return shiftDown ? '?' : '/';
        case 0x39: return ' ';
        case 0x1C: return '\n';
        case 0x0E: return '\b';
        case 0x0C: return shiftDown ? '_' : '-';
        default: return 0;
    }
}

bool FKeyboardX86::Initialize() {
    while ((In8(0x64) & 0x01u) != 0) {
        (void)In8(0x60);
    }

    Out8(0x64, 0xAE);
    return true;
}

bool FKeyboardX86::PollEvent(FKeyEvent &outEvent) {
    const Fortress::Core::uint8 status = In8(0x64);
    if ((status & 0x01u) == 0) {
        return false;
    }

    const Fortress::Core::uint8 sc = In8(0x60);

    if (sc == 0xE0u || sc == 0xE1u) {
        return false;
    }

    const bool released = (sc & 0x80u) != 0;
    const Fortress::Core::uint8 code = static_cast<Fortress::Core::uint8>(sc & 0x7Fu);

    if (code == 0x2Au) {
        GLeftShiftDown = !released;
        return false;
    }

    if (code == 0x36u) {
        GRightShiftDown = !released;
        return false;
    }

    const bool shiftDown = GLeftShiftDown || GRightShiftDown;

    outEvent.Pressed = !released;
    outEvent.Ascii = ScancodeToAscii(code, shiftDown);
    outEvent.ScanCode = code;
    outEvent.ShiftActive = shiftDown;
    return true;
}

} // namespace Fortress::Platform
