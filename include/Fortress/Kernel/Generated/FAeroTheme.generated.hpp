#ifndef FORTRESS_KERNEL_GENERATED_FAEROTHEME_GENERATED_HPP
#define FORTRESS_KERNEL_GENERATED_FAEROTHEME_GENERATED_HPP

#include "Fortress/Video/FColor.hpp"

namespace Fortress::Kernel::GeneratedAeroTheme {

static constexpr Fortress::Video::FColor Color(unsigned r, unsigned g, unsigned b, unsigned a) {
    return Fortress::Video::FColor{
        .R = static_cast<Fortress::Core::uint8>(r),
        .G = static_cast<Fortress::Core::uint8>(g),
        .B = static_cast<Fortress::Core::uint8>(b),
        .A = static_cast<Fortress::Core::uint8>(a),
    };
}

static constexpr Fortress::Video::FColor kWindowBackground = Color(103u, 14u, 14u, 255u);
static constexpr Fortress::Video::FColor kTitleBarGradientStart = Color(255u, 77u, 77u, 255u);
static constexpr Fortress::Video::FColor kTitleBarGradientEnd = Color(125u, 28u, 28u, 255u);
static constexpr Fortress::Video::FColor kButtonNormal = Color(255u, 255u, 255u, 51u);
static constexpr Fortress::Video::FColor kButtonHover = Color(255u, 255u, 255u, 69u);
static constexpr Fortress::Video::FColor kButtonPressed = Color(255u, 255u, 255u, 102u);
static constexpr Fortress::Video::FColor kButtonGlow = Color(78u, 160u, 255u, 160u);
static constexpr Fortress::Video::FColor kTextPrimary = Color(0u, 0u, 0u, 255u);
static constexpr Fortress::Video::FColor kTextSecondary = Color(255u, 255u, 255u, 255u);
static constexpr Fortress::Video::FColor kBorder = Color(46u, 59u, 80u, 255u);
static constexpr Fortress::Video::FColor kShadow = Color(0u, 0u, 0u, 128u);
static constexpr Fortress::Video::FColor kAccentPrimary = Color(78u, 160u, 255u, 255u);
static constexpr Fortress::Video::FColor kAccentSecondary = Color(140u, 200u, 255u, 255u);

} // namespace Fortress::Kernel::GeneratedAeroTheme

#endif
