#ifndef FORTRESS_KERNEL_FKERNELRUNTIMEIDS_HPP
#define FORTRESS_KERNEL_FKERNELRUNTIMEIDS_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel::FKernelRuntimeIds {

static constexpr Fortress::Core::uint32 ServiceScheduler = 1u;
static constexpr Fortress::Core::uint32 ServiceDisplayManager = 2u;
static constexpr Fortress::Core::uint32 ServiceCommandConsole = 3u;
static constexpr Fortress::Core::uint32 ServiceKeyboardInput = 4u;
static constexpr Fortress::Core::uint32 ServiceVirtualFileSystem = 5u;
static constexpr Fortress::Core::uint32 ServiceDesktopCompositor = 6u;
static constexpr Fortress::Core::uint32 ServiceDesktopShell = 7u;

static constexpr Fortress::Core::uint32 ChannelSchedulerEndpoint = 100u;
static constexpr Fortress::Core::uint32 ChannelDisplayEndpoint = 101u;
static constexpr Fortress::Core::uint32 ChannelCommandConsoleEndpoint = 102u;
static constexpr Fortress::Core::uint32 ChannelKeyboardInputEndpoint = 103u;
static constexpr Fortress::Core::uint32 ChannelVirtualFileSystemEndpoint = 104u;
static constexpr Fortress::Core::uint32 ChannelDesktopCompositorEndpoint = 105u;
static constexpr Fortress::Core::uint32 ChannelDesktopShellEndpoint = 106u;

static constexpr Fortress::Core::uint32 TopicScheduler = 1u;
static constexpr Fortress::Core::uint32 TopicCommand = 2u;
static constexpr Fortress::Core::uint32 TopicInput = 3u;

static constexpr Fortress::Core::uint32 EventSchedulerHeartbeat = 1u;
static constexpr Fortress::Core::uint32 EventCommandSubmitted = 2u;
static constexpr Fortress::Core::uint32 EventRenderWireframeSet = 3u;
static constexpr Fortress::Core::uint32 EventScenePauseSet = 4u;
static constexpr Fortress::Core::uint32 EventCursorOverlaySet = 5u;
static constexpr Fortress::Core::uint32 EventInputKeyPressed = 6u;
static constexpr Fortress::Core::uint32 EventDesktopSurfaceOverlaySet = 7u;

} // namespace Fortress::Kernel::FKernelRuntimeIds

#endif