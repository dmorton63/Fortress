#include "Fortress/Cpu/FPanicScreen.hpp"

#include <cstddef>
#include <cstdint>

#include "limine.h"

namespace Fortress::Cpu {

static limine_framebuffer *GFramebuffer = nullptr;
static bool GSerialReady = false;

static inline void Out8(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t In8(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void InitSerial() {
    // COM1 @ 115200 8N1
    Out8(0x3F8 + 1, 0x00);
    Out8(0x3F8 + 3, 0x80);
    Out8(0x3F8 + 0, 0x01);
    Out8(0x3F8 + 1, 0x00);
    Out8(0x3F8 + 3, 0x03);
    Out8(0x3F8 + 2, 0xC7);
    Out8(0x3F8 + 4, 0x0B);
    GSerialReady = true;
}

static void SerialPutChar(char c) {
    if (!GSerialReady) {
        return;
    }
    while ((In8(0x3F8 + 5) & 0x20u) == 0) {}
    Out8(0x3F8, static_cast<uint8_t>(c));
}

static void SerialWrite(const char *text) {
    if (text == nullptr) {
        return;
    }
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            SerialPutChar('\r');
        }
        SerialPutChar(text[i]);
    }
}

static void AppendChar(char *dst, size_t dstSize, size_t &offset, char c) {
    if (offset + 1 >= dstSize) {
        return;
    }
    dst[offset++] = c;
    dst[offset] = '\0';
}

static void AppendString(char *dst, size_t dstSize, size_t &offset, const char *src) {
    if (src == nullptr) {
        return;
    }
    for (size_t i = 0; src[i] != '\0'; i++) {
        AppendChar(dst, dstSize, offset, src[i]);
    }
}

static void AppendUInt(char *dst, size_t dstSize, size_t &offset, uint64_t value) {
    char rev[24] = {};
    size_t n = 0;
    if (value == 0) {
        AppendChar(dst, dstSize, offset, '0');
        return;
    }
    while (value > 0 && n < sizeof(rev)) {
        rev[n++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    while (n > 0) {
        AppendChar(dst, dstSize, offset, rev[n - 1]);
        n--;
    }
}

static void AppendHex(char *dst, size_t dstSize, size_t &offset, uint64_t value) {
    static const char Hex[] = "0123456789ABCDEF";
    AppendString(dst, dstSize, offset, "0x");
    bool started = false;
    for (int shift = 60; shift >= 0; shift -= 4) {
        const uint8_t nibble = static_cast<uint8_t>((value >> shift) & 0xFu);
        if (!started && nibble == 0 && shift != 0) {
            continue;
        }
        started = true;
        AppendChar(dst, dstSize, offset, Hex[nibble]);
    }
}

static uint64_t ReadCR2() {
    uint64_t value;
    __asm__ volatile("mov %%cr2, %0" : "=r"(value));
    return value;
}

static void PutPixel(int x, int y, uint32_t color) {
    if (GFramebuffer == nullptr) {
        return;
    }

    if (x < 0 || y < 0) {
        return;
    }

    const uint64_t width = GFramebuffer->width;
    const uint64_t height = GFramebuffer->height;
    if (static_cast<uint64_t>(x) >= width || static_cast<uint64_t>(y) >= height) {
        return;
    }

    auto *base = static_cast<uint8_t *>(GFramebuffer->address);
    auto *pixel = reinterpret_cast<uint32_t *>(base + static_cast<uint64_t>(y) * GFramebuffer->pitch + static_cast<uint64_t>(x) * 4);
    *pixel = color;
}

static void Fill(uint32_t color) {
    if (GFramebuffer == nullptr) {
        return;
    }

    for (uint64_t y = 0; y < GFramebuffer->height; y++) {
        for (uint64_t x = 0; x < GFramebuffer->width; x++) {
            auto *base = static_cast<uint8_t *>(GFramebuffer->address);
            auto *pixel = reinterpret_cast<uint32_t *>(base + y * GFramebuffer->pitch + x * 4);
            *pixel = color;
        }
    }
}

static void DrawDigit(int value, int originX, int originY, uint32_t color) {
    static const uint8_t Digits[10][7] = {
        {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110},
        {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},
        {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111},
        {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110},
        {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010},
        {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110},
        {0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110},
        {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000},
        {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110},
        {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110},
    };

    if (value < 0 || value > 9) {
        return;
    }

    constexpr int Scale = 6;
    for (int y = 0; y < 7; y++) {
        for (int x = 0; x < 5; x++) {
            if ((Digits[value][y] & (1u << (4 - x))) == 0) {
                continue;
            }

            for (int sy = 0; sy < Scale; sy++) {
                for (int sx = 0; sx < Scale; sx++) {
                    PutPixel(originX + x * Scale + sx, originY + y * Scale + sy, color);
                }
            }
        }
    }
}

void FPanicScreen::Initialize(limine_framebuffer *framebuffer) {
    GFramebuffer = framebuffer;
    InitSerial();
}

void FPanicScreen::OnException(Fortress::Core::uint64 vector, Fortress::Core::uint64 errorCode) {
    char line[160] = {};
    size_t pos = 0;
    AppendString(line, sizeof(line), pos, "PANIC EXC ");
    AppendUInt(line, sizeof(line), pos, vector);
    AppendString(line, sizeof(line), pos, " ERR=");
    AppendHex(line, sizeof(line), pos, errorCode);

    if (vector == 14) {
        const uint64_t faultAddress = ReadCR2();
        AppendString(line, sizeof(line), pos, " CR2=");
        AppendHex(line, sizeof(line), pos, faultAddress);
    }

    AppendString(line, sizeof(line), pos, "\n");
    SerialWrite(line);

    Fill(0x001010A0);

    if (GFramebuffer == nullptr) {
        return;
    }

    const int centerX = static_cast<int>(GFramebuffer->width / 2);
    const int centerY = static_cast<int>(GFramebuffer->height / 2);

    for (int x = centerX - 240; x <= centerX + 240; x++) {
        for (int y = centerY - 80; y <= centerY + 80; y++) {
            PutPixel(x, y, 0x00FFFFFF);
        }
    }

    const int tens = static_cast<int>((vector / 10) % 10);
    const int ones = static_cast<int>(vector % 10);
    DrawDigit(tens, centerX - 70, centerY - 20, 0x001010A0);
    DrawDigit(ones, centerX + 10, centerY - 20, 0x001010A0);
}

} // namespace Fortress::Cpu
