#ifndef FORTRESS_KERNEL_FMESSAGEBUS_HPP
#define FORTRESS_KERNEL_FMESSAGEBUS_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

struct FMessageEnvelope {
    Fortress::Core::uint32 SourceServiceId = 0;
    Fortress::Core::uint32 TargetServiceId = 0;
    Fortress::Core::uint32 ChannelId = 0;
    Fortress::Core::uint32 MessageType = 0;
    Fortress::Core::uint32 PayloadWords[4] = {};
    Fortress::Core::uint32 PayloadWordCount = 0;
};

struct FMessageBusStats {
    Fortress::Core::uint32 Capacity = 0;
    Fortress::Core::uint32 QueueDepth = 0;
    Fortress::Core::uint64 PublishedCount = 0;
    Fortress::Core::uint64 ConsumedCount = 0;
    Fortress::Core::uint64 DroppedCount = 0;
};

class FMessageBus {
  public:
    static constexpr Fortress::Core::uint32 AnyChannel = 0xFFFFFFFFu;

    static bool Initialize();
    static bool Publish(const FMessageEnvelope &message);
    static bool Consume(Fortress::Core::uint32 channelId, FMessageEnvelope &outMessage);
    static void GetStats(FMessageBusStats &outStats);
};

} // namespace Fortress::Kernel

#endif