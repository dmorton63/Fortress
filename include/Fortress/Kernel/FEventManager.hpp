#ifndef FORTRESS_KERNEL_FEVENTMANAGER_HPP
#define FORTRESS_KERNEL_FEVENTMANAGER_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

struct FKernelEvent {
    Fortress::Core::uint32 EventId = 0;
    Fortress::Core::uint32 TopicId = 0;
    Fortress::Core::uint32 SourceServiceId = 0;
    Fortress::Core::uint32 Arg0 = 0;
    Fortress::Core::uint32 Arg1 = 0;
    Fortress::Core::uint32 Arg2 = 0;
};

struct FKernelEventSubscriptionHandle {
    Fortress::Core::uint32 Id = 0;
};

using FKernelEventHandler = void (*)(const FKernelEvent &event, void *context);

struct FEventManagerStats {
    Fortress::Core::uint32 SubscriptionCount = 0;
    Fortress::Core::uint32 QueueDepth = 0;
    Fortress::Core::uint64 PublishedCount = 0;
    Fortress::Core::uint64 DispatchedCount = 0;
    Fortress::Core::uint64 DroppedCount = 0;
    Fortress::Core::uint64 FanoutLatencyMicros = 0;
    Fortress::Core::uint64 HandlerFaultCount = 0;
};

class FEventManager {
  public:
    static constexpr Fortress::Core::uint32 EventChannelId = 1u;

    static bool Initialize();
    static bool Subscribe(Fortress::Core::uint32 topicId,
                          FKernelEventHandler handler,
                          void *context,
                          FKernelEventSubscriptionHandle &outHandle);
    static bool Unsubscribe(FKernelEventSubscriptionHandle handle);
    static bool Publish(const FKernelEvent &event);
    static bool DispatchOne();
    static void GetStats(FEventManagerStats &outStats);
};

} // namespace Fortress::Kernel

#endif