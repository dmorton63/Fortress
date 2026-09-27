#ifndef FORTRESS_KERNEL_FKERNELCONFIG_HPP
#define FORTRESS_KERNEL_FKERNELCONFIG_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

struct FKernelConfig {
    static constexpr Fortress::Core::uint64 KernelArenaBytes = 32ull * 1024ull * 1024ull;
    static constexpr Fortress::Core::uint64 ReserveLowBytes = 0x100000ull;

    static constexpr Fortress::Core::uint64 KernelHeapBase = 0xFFFFA00000000000ull;
    static constexpr Fortress::Core::uint64 KernelHeapInitialPages = 256ull;
    static constexpr Fortress::Core::uint64 KernelHeapGrowPages = 64ull;

    static constexpr Fortress::Core::uint64 VmallocBase = 0xFFFFA20000000000ull;
    static constexpr Fortress::Core::uint64 PinnedMapBase = 0xFFFFA30000000000ull;
    static constexpr Fortress::Core::uint64 PinnedMapPages = 8192ull;
    static constexpr Fortress::Core::uint64 DmaMapBase = 0xFFFFA32000000000ull;
    static constexpr Fortress::Core::uint64 DmaMapPages = 8192ull;

    static constexpr Fortress::Core::int32 HudOriginX = 16;
    static constexpr Fortress::Core::int32 HudOriginY = 16;

    static constexpr float SceneFixedStepSeconds = 1.0f / 60.0f;
    static constexpr float SceneYawRate = 1.15f;
    static constexpr float ScenePitchRate = 0.75f;
    static constexpr float SceneNearPlane = 0.1f;
    static constexpr float SceneFarPlane = 100.0f;
    static constexpr float SceneFovRadians = 1.22173f;
    static constexpr float SceneDistance = 5.0f;
};

} // namespace Fortress::Kernel

#endif
