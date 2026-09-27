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

    for (Fortress::Core::uint32 i = 0; i < GMaxSubscriptions; i++) {
        const FSubscriptionSlot &slot = GSubscriptionSlots[i];
        if (!slot.InUse || slot.Handler == nullptr || slot.TopicId != event.TopicId) {
            continue;
        }

        slot.Handler(event, slot.Context);
    }

    GDispatchedCount++;
    return true;
}

void FEventManager::GetStats(FEventManagerStats &outStats) {
    outStats = FEventManagerStats{
        .SubscriptionCount = CountSubscriptions(),
        .PublishedCount = GPublishedCount,
        .DispatchedCount = GDispatchedCount,
        .DroppedCount = GDroppedCount,
    };
}

} // namespace Fortress::Kernel
