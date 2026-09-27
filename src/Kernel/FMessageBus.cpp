#include "Fortress/Kernel/FMessageBus.hpp"

namespace Fortress::Kernel {

static constexpr Fortress::Core::uint32 GMessageCapacity = 128;

static bool GInitialized = false;
static FMessageEnvelope GMessages[GMessageCapacity] = {};
static Fortress::Core::uint32 GMessageCount = 0;
static Fortress::Core::uint64 GPublishedCount = 0;
static Fortress::Core::uint64 GConsumedCount = 0;
static Fortress::Core::uint64 GDroppedCount = 0;

bool FMessageBus::Initialize() {
    GInitialized = true;
    GMessageCount = 0;
    GPublishedCount = 0;
    GConsumedCount = 0;
    GDroppedCount = 0;

    for (Fortress::Core::uint32 i = 0; i < GMessageCapacity; i++) {
        GMessages[i] = FMessageEnvelope{};
    }

    return true;
}

bool FMessageBus::Publish(const FMessageEnvelope &message) {
    if (!GInitialized) {
        return false;
    }

    if (GMessageCount >= GMessageCapacity) {
        GDroppedCount++;
        return false;
    }

    FMessageEnvelope copy = message;
    if (copy.PayloadWordCount > 4) {
        copy.PayloadWordCount = 4;
    }

    GMessages[GMessageCount++] = copy;
    GPublishedCount++;
    return true;
}

bool FMessageBus::Consume(Fortress::Core::uint32 channelId, FMessageEnvelope &outMessage) {
    if (!GInitialized || GMessageCount == 0) {
        return false;
    }

    Fortress::Core::uint32 matchIndex = GMessageCapacity;
    for (Fortress::Core::uint32 i = 0; i < GMessageCount; i++) {
        if (channelId == AnyChannel || GMessages[i].ChannelId == channelId) {
            matchIndex = i;
            break;
        }
    }

    if (matchIndex >= GMessageCount) {
        return false;
    }

    outMessage = GMessages[matchIndex];

    for (Fortress::Core::uint32 i = matchIndex + 1; i < GMessageCount; i++) {
        GMessages[i - 1] = GMessages[i];
    }

    GMessageCount--;
    GMessages[GMessageCount] = FMessageEnvelope{};
    GConsumedCount++;
    return true;
}

void FMessageBus::GetStats(FMessageBusStats &outStats) {
    outStats = FMessageBusStats{
        .Capacity = GMessageCapacity,
        .QueueDepth = GMessageCount,
        .PublishedCount = GPublishedCount,
        .ConsumedCount = GConsumedCount,
        .DroppedCount = GDroppedCount,
    };
}

} // namespace Fortress::Kernel
