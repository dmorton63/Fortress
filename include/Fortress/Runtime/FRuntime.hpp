#ifndef FORTRESS_RUNTIME_FRUNTIME_HPP
#define FORTRESS_RUNTIME_FRUNTIME_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Runtime {

void *Memset(void *dest, int value, Fortress::Core::usize count);
void *Memcpy(void *dest, const void *src, Fortress::Core::usize count);
void *Memmove(void *dest, const void *src, Fortress::Core::usize count);
int Memcmp(const void *lhs, const void *rhs, Fortress::Core::usize count);

} // namespace Fortress::Runtime

extern "C" void *memset(void *dest, int value, Fortress::Core::usize count);
extern "C" void *memcpy(void *dest, const void *src, Fortress::Core::usize count);
extern "C" void *memmove(void *dest, const void *src, Fortress::Core::usize count);
extern "C" int memcmp(const void *lhs, const void *rhs, Fortress::Core::usize count);

#endif
