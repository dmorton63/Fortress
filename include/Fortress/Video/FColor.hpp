#ifndef FORTRESS_VIDEO_FCOLOR_HPP
#define FORTRESS_VIDEO_FCOLOR_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Video {

struct FColor {
    Fortress::Core::uint8 R;
    Fortress::Core::uint8 G;
    Fortress::Core::uint8 B;
    Fortress::Core::uint8 A;

    static constexpr FColor RGB(Fortress::Core::uint8 r, Fortress::Core::uint8 g, Fortress::Core::uint8 b) {
        return FColor{.R = r, .G = g, .B = b, .A = 255};
    }
};

} // namespace Fortress::Video

#endif