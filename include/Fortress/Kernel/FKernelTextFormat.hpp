#ifndef FORTRESS_KERNEL_FKERNELTEXTFORMAT_HPP
#define FORTRESS_KERNEL_FKERNELTEXTFORMAT_HPP

#include <cstddef>

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel::FKernelTextFormat {

inline void AppendChar(char *dst, size_t dstSize, size_t &offset, char c) {
    if (offset + 1 >= dstSize) {
        return;
    }

    dst[offset++] = c;
    dst[offset] = '\0';
}

inline void AppendString(char *dst, size_t dstSize, size_t &offset, const char *src) {
    if (src == nullptr) {
        return;
    }

    for (size_t i = 0; src[i] != '\0'; i++) {
        AppendChar(dst, dstSize, offset, src[i]);
    }
}

inline void AppendUInt(char *dst, size_t dstSize, size_t &offset, Fortress::Core::uint64 value) {
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

} // namespace Fortress::Kernel::FKernelTextFormat

#endif