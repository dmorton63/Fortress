#include "Fortress/Runtime/FRuntime.hpp"

namespace Fortress::Runtime {

void *Memset(void *dest, int value, Fortress::Core::usize count) {
    auto *bytes = static_cast<Fortress::Core::uint8 *>(dest);
    const auto fill = static_cast<Fortress::Core::uint8>(value);
    for (Fortress::Core::usize i = 0; i < count; i++) {
        bytes[i] = fill;
    }
    return dest;
}

void *Memcpy(void *dest, const void *src, Fortress::Core::usize count) {
    auto *d = static_cast<Fortress::Core::uint8 *>(dest);
    const auto *s = static_cast<const Fortress::Core::uint8 *>(src);
    for (Fortress::Core::usize i = 0; i < count; i++) {
        d[i] = s[i];
    }
    return dest;
}

void *Memmove(void *dest, const void *src, Fortress::Core::usize count) {
    auto *d = static_cast<Fortress::Core::uint8 *>(dest);
    const auto *s = static_cast<const Fortress::Core::uint8 *>(src);

    if (d == s || count == 0) {
        return dest;
    }

    if (d < s) {
        for (Fortress::Core::usize i = 0; i < count; i++) {
            d[i] = s[i];
        }
    } else {
        for (Fortress::Core::usize i = count; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}

int Memcmp(const void *lhs, const void *rhs, Fortress::Core::usize count) {
    const auto *a = static_cast<const Fortress::Core::uint8 *>(lhs);
    const auto *b = static_cast<const Fortress::Core::uint8 *>(rhs);

    for (Fortress::Core::usize i = 0; i < count; i++) {
        if (a[i] != b[i]) {
            return (a[i] < b[i]) ? -1 : 1;
        }
    }

    return 0;
}

} // namespace Fortress::Runtime

extern "C" void *memset(void *dest, int value, Fortress::Core::usize count) {
    return Fortress::Runtime::Memset(dest, value, count);
}

extern "C" void *memcpy(void *dest, const void *src, Fortress::Core::usize count) {
    return Fortress::Runtime::Memcpy(dest, src, count);
}

extern "C" void *memmove(void *dest, const void *src, Fortress::Core::usize count) {
    return Fortress::Runtime::Memmove(dest, src, count);
}

extern "C" int memcmp(const void *lhs, const void *rhs, Fortress::Core::usize count) {
    return Fortress::Runtime::Memcmp(lhs, rhs, count);
}
