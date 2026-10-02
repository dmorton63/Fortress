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

static constexpr Fortress::Video::FColor kWindowBackground = Color(14u, 43u, 102u, 255u);
static constexpr Fortress::Video::FColor kTitleBarGradientStart = Color(111u, 176u, 255u, 255u);
static constexpr Fortress::Video::FColor kTitleBarGradientEnd = Color(42u, 95u, 174u, 255u);
static constexpr Fortress::Video::FColor kButtonNormal = Color(255u, 255u, 255u, 143u);
static constexpr Fortress::Video::FColor kButtonHover = Color(255u, 255u, 255u, 87u);
static constexpr Fortress::Video::FColor kButtonPressed = Color(59u, 130u, 222u, 102u);
static constexpr Fortress::Video::FColor kButtonGlow = Color(247u, 206u, 70u, 160u);
static constexpr Fortress::Video::FColor kTextPrimary = Color(247u, 251u, 255u, 255u);
static constexpr Fortress::Video::FColor kTextSecondary = Color(244u, 248u, 255u, 255u);
static constexpr Fortress::Video::FColor kBorder = Color(32u, 64u, 111u, 255u);
static constexpr Fortress::Video::FColor kShadow = Color(0u, 0u, 0u, 66u);
static constexpr Fortress::Video::FColor kAccentPrimary = Color(30u, 94u, 183u, 255u);
static constexpr Fortress::Video::FColor kAccentSecondary = Color(97u, 166u, 255u, 255u);

} // namespace Fortress::Kernel::GeneratedAeroTheme

#endif
