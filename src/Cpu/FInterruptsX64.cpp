#include "Fortress/Cpu/FInterruptsX64.hpp"

namespace Fortress::Cpu {

namespace {

struct [[gnu::packed]] FIdtEntry {
    Fortress::Core::uint16 OffsetLow;
    Fortress::Core::uint16 Selector;
    Fortress::Core::uint8 Ist;
    Fortress::Core::uint8 TypeAttr;
    Fortress::Core::uint16 OffsetMid;
    Fortress::Core::uint32 OffsetHigh;
    Fortress::Core::uint32 Zero;
};

struct [[gnu::packed]] FIdtr {
    Fortress::Core::uint16 Limit;
    Fortress::Core::uint64 Base;
};

struct [[gnu::packed]] FInterruptFrame {
    Fortress::Core::uint64 Rip;
    Fortress::Core::uint64 Cs;
    Fortress::Core::uint64 Rflags;
    Fortress::Core::uint64 Rsp;
    Fortress::Core::uint64 Ss;
};

alignas(16) static FIdtEntry GIdt[256];
static FExceptionCallback GCallback = nullptr;

static inline void Halt() {
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

static void DispatchException(Fortress::Core::uint64 vector, Fortress::Core::uint64 errorCode) {
    if (GCallback != nullptr) {
        GCallback(vector, errorCode);
    }
    Halt();
}

#define DEF_ISR_NOERR(N) \
extern "C" __attribute__((interrupt)) void Isr##N(FInterruptFrame *frame) { \
    (void)frame; \
    DispatchException(N, 0); \
}

#define DEF_ISR_ERR(N) \
extern "C" __attribute__((interrupt)) void Isr##N(FInterruptFrame *frame, Fortress::Core::uint64 errorCode) { \
    (void)frame; \
    DispatchException(N, errorCode); \
}

DEF_ISR_NOERR(0) DEF_ISR_NOERR(1) DEF_ISR_NOERR(2) DEF_ISR_NOERR(3)
DEF_ISR_NOERR(4) DEF_ISR_NOERR(5) DEF_ISR_NOERR(6) DEF_ISR_NOERR(7)
DEF_ISR_ERR(8)
DEF_ISR_NOERR(9)
DEF_ISR_ERR(10) DEF_ISR_ERR(11) DEF_ISR_ERR(12) DEF_ISR_ERR(13) DEF_ISR_ERR(14)
DEF_ISR_NOERR(15) DEF_ISR_NOERR(16)
DEF_ISR_ERR(17)
DEF_ISR_NOERR(18) DEF_ISR_NOERR(19) DEF_ISR_NOERR(20)
DEF_ISR_ERR(21)
DEF_ISR_NOERR(22) DEF_ISR_NOERR(23) DEF_ISR_NOERR(24) DEF_ISR_NOERR(25)
DEF_ISR_NOERR(26) DEF_ISR_NOERR(27) DEF_ISR_NOERR(28)
DEF_ISR_ERR(29)
DEF_ISR_ERR(30)
DEF_ISR_NOERR(31)

static void SetGate(Fortress::Core::uint8 vector, void (*handler)()) {
    const Fortress::Core::uint64 addr = reinterpret_cast<Fortress::Core::uint64>(handler);

    GIdt[vector].OffsetLow = static_cast<Fortress::Core::uint16>(addr & 0xFFFFu);
    GIdt[vector].Selector = 0x08;
    GIdt[vector].Ist = 0;
    GIdt[vector].TypeAttr = 0x8E;
    GIdt[vector].OffsetMid = static_cast<Fortress::Core::uint16>((addr >> 16) & 0xFFFFu);
    GIdt[vector].OffsetHigh = static_cast<Fortress::Core::uint32>((addr >> 32) & 0xFFFFFFFFu);
    GIdt[vector].Zero = 0;
}

} // namespace

void FInterruptsX64::Initialize(FExceptionCallback callback) {
    GCallback = callback;

    for (Fortress::Core::uint32 i = 0; i < 256; i++) {
        GIdt[i] = FIdtEntry{};
    }

    SetGate(0, reinterpret_cast<void (*)()>(Isr0));
    SetGate(1, reinterpret_cast<void (*)()>(Isr1));
    SetGate(2, reinterpret_cast<void (*)()>(Isr2));
    SetGate(3, reinterpret_cast<void (*)()>(Isr3));
    SetGate(4, reinterpret_cast<void (*)()>(Isr4));
    SetGate(5, reinterpret_cast<void (*)()>(Isr5));
    SetGate(6, reinterpret_cast<void (*)()>(Isr6));
    SetGate(7, reinterpret_cast<void (*)()>(Isr7));
    SetGate(8, reinterpret_cast<void (*)()>(Isr8));
    SetGate(9, reinterpret_cast<void (*)()>(Isr9));
    SetGate(10, reinterpret_cast<void (*)()>(Isr10));
    SetGate(11, reinterpret_cast<void (*)()>(Isr11));
    SetGate(12, reinterpret_cast<void (*)()>(Isr12));
    SetGate(13, reinterpret_cast<void (*)()>(Isr13));
    SetGate(14, reinterpret_cast<void (*)()>(Isr14));
    SetGate(15, reinterpret_cast<void (*)()>(Isr15));
    SetGate(16, reinterpret_cast<void (*)()>(Isr16));
    SetGate(17, reinterpret_cast<void (*)()>(Isr17));
    SetGate(18, reinterpret_cast<void (*)()>(Isr18));
    SetGate(19, reinterpret_cast<void (*)()>(Isr19));
    SetGate(20, reinterpret_cast<void (*)()>(Isr20));
    SetGate(21, reinterpret_cast<void (*)()>(Isr21));
    SetGate(22, reinterpret_cast<void (*)()>(Isr22));
    SetGate(23, reinterpret_cast<void (*)()>(Isr23));
    SetGate(24, reinterpret_cast<void (*)()>(Isr24));
    SetGate(25, reinterpret_cast<void (*)()>(Isr25));
    SetGate(26, reinterpret_cast<void (*)()>(Isr26));
    SetGate(27, reinterpret_cast<void (*)()>(Isr27));
    SetGate(28, reinterpret_cast<void (*)()>(Isr28));
    SetGate(29, reinterpret_cast<void (*)()>(Isr29));
    SetGate(30, reinterpret_cast<void (*)()>(Isr30));
    SetGate(31, reinterpret_cast<void (*)()>(Isr31));

    const FIdtr idtr{
        .Limit = static_cast<Fortress::Core::uint16>(sizeof(GIdt) - 1),
        .Base = reinterpret_cast<Fortress::Core::uint64>(&GIdt[0]),
    };

    __asm__ volatile("lidt %0" : : "m"(idtr));
}

} // namespace Fortress::Cpu
