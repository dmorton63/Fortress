#include "Fortress/Kernel/FEventManager.hpp"

#include "Fortress/Kernel/FMessageBus.hpp"

namespace Fortress::Kernel {

static constexpr Fortress::Core::uint32 GMaxSubscriptions = 32;

struct FSubscriptionSlot {
    bool InUse = false;
    Fortress::Core::uint32 Id = 0;
    Fortress::Core::uint32 TopicId = 0;
    FKernelEventHandler Handler = nullptr;
    void *Context = nullptr;
};

static bool GInitialized = false;
static Fortress::Core::uint32 GNextSubscriptionId = 1;
static FSubscriptionSlot GSubscriptionSlots[GMaxSubscriptions] = {};
static Fortress::Core::uint64 GPublishedCount = 0;
static Fortress::Core::uint64 GDispatchedCount = 0;
static Fortress::Core::uint64 GDroppedCount = 0;
static Fortress::Core::uint64 GFanoutLatencyMicros = 0;
static Fortress::Core::uint64 GHandlerFaultCount = 0;

static Fortress::Core::uint32 CountSubscriptions() {
    Fortress::Core::uint32 count = 0;
    for (Fortress::Core::uint32 i = 0; i < GMaxSubscriptions; i++) {
        if (GSubscriptionSlots[i].InUse) {
            count++;
        }
    }
    return count;
}

bool FEventManager::Initialize() {
    GInitialized = true;
    GNextSubscriptionId = 1;
    GPublishedCount = 0;
    GDispatchedCount = 0;
    GDroppedCount = 0;
    GFanoutLatencyMicros = 0;
    GHandlerFaultCount = 0;

    for (Fortress::Core::uint32 i = 0; i < GMaxSubscriptions; i++) {
        GSubscriptionSlots[i] = FSubscriptionSlot{};
    }

    return true;
}

bool FEventManager::Subscribe(Fortress::Core::uint32 topicId,
                              FKernelEventHandler handler,
                              void *context,
                              FKernelEventSubscriptionHandle &outHandle) {
    outHandle = FKernelEventSubscriptionHandle{};

    if (!GInitialized || handler == nullptr) {
        return false;
    }

    for (Fortress::Core::uint32 i = 0; i < GMaxSubscriptions; i++) {
        if (GSubscriptionSlots[i].InUse) {
            continue;
        }

        GSubscriptionSlots[i] = FSubscriptionSlot{
            .InUse = true,
            .Id = GNextSubscriptionId++,
            .TopicId = topicId,
            .Handler = handler,
            .Context = context,
        };
        outHandle.Id = GSubscriptionSlots[i].Id;
        return true;
    }

    return false;
}

bool FEventManager::Unsubscribe(FKernelEventSubscriptionHandle handle) {
    if (!GInitialized || handle.Id == 0) {
        return false;
    }

    for (Fortress::Core::uint32 i = 0; i < GMaxSubscriptions; i++) {
        if (!GSubscriptionSlots[i].InUse || GSubscriptionSlots[i].Id != handle.Id) {
            continue;
        }

        GSubscriptionSlots[i] = FSubscriptionSlot{};
        return true;
    }

    return false;
}

bool FEventManager::Publish(const FKernelEvent &event) {
    if (!GInitialized) {
        return false;
    }

    const FMessageEnvelope envelope{
        .SourceServiceId = event.SourceServiceId,
        .TargetServiceId = 0,
        .ChannelId = EventChannelId,
        .MessageType = event.EventId,
        .PayloadWords = {event.TopicId, event.Arg0, event.Arg1, event.Arg2},
        .PayloadWordCount = 4,
    };

    if (!FMessageBus::Publish(envelope)) {
        GDroppedCount++;
        return false;
    }

    GPublishedCount++;
    return true;
}

bool FEventManager::DispatchOne() {
    if (!GInitialized) {
        return false;
    }

    FMessageEnvelope envelope{};
    if (!FMessageBus::Consume(EventChannelId, envelope)) {
        return false;
    }

    const FKernelEvent event{
        .EventId = envelope.MessageType,
        .TopicId = envelope.PayloadWords[0],
        .SourceServiceId = envelope.SourceServiceId,
        .Arg0 = envelope.PayloadWords[1],
        .Arg1 = envelope.PayloadWords[2],
        .Arg2 = envelope.PayloadWords[3],
    };

    Fortress::Core::uint64 deliveredHandlers = 0;
    for (Fortress::Core::uint32 i = 0; i < GMaxSubscriptions; i++) {
        const FSubscriptionSlot &slot = GSubscriptionSlots[i];
        if (!slot.InUse || slot.TopicId != event.TopicId) {
            continue;
        }

        if (slot.Handler == nullptr) {
            GHandlerFaultCount++;
            continue;
        }

        slot.Handler(event, slot.Context);
        deliveredHandlers++;
    }

    if (deliveredHandlers > 0) {
        const Fortress::Core::uint64 latencySampleMicros = 1u + (deliveredHandlers * 2u);
        if (GFanoutLatencyMicros == 0u) {
            GFanoutLatencyMicros = latencySampleMicros;
        } else {
            GFanoutLatencyMicros = ((GFanoutLatencyMicros * 7u) + latencySampleMicros) / 8u;
        }
    }

    GDispatchedCount++;
    return true;
}

void FEventManager::GetStats(FEventManagerStats &outStats) {
    const Fortress::Core::uint64 queueDepth64 =
        (GPublishedCount >= GDispatchedCount) ? (GPublishedCount - GDispatchedCount) : 0u;

    outStats = FEventManagerStats{
        .SubscriptionCount = CountSubscriptions(),
        .QueueDepth = (queueDepth64 > static_cast<Fortress::Core::uint64>(0xFFFFFFFFu))
                          ? 0xFFFFFFFFu
                          : static_cast<Fortress::Core::uint32>(queueDepth64),
        .PublishedCount = GPublishedCount,
        .DispatchedCount = GDispatchedCount,
        .DroppedCount = GDroppedCount,
        .FanoutLatencyMicros = GFanoutLatencyMicros,
        .HandlerFaultCount = GHandlerFaultCount,
    };
}

} // namespace Fortress::Kernel
