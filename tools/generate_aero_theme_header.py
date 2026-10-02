#!/usr/bin/env python3
import json
import pathlib
import sys


def get_path(data, *path):
    node = data
    for part in path:
        if not isinstance(node, dict) or part not in node:
            return None
        node = node[part]
    return node


def parse_hex_rgb(value):
    if not isinstance(value, str):
        return None
    text = value.strip()
    if len(text) != 7 or not text.startswith("#"):
        return None
    try:
        r = int(text[1:3], 16)
        g = int(text[3:5], 16)
        b = int(text[5:7], 16)
    except ValueError:
        return None
    return (r, g, b)


def clamp_u8(value):
    if value < 0:
        return 0
    if value > 255:
        return 255
    return int(value)


def pct_to_alpha(percent, fallback):
    if not isinstance(percent, (int, float)):
        return fallback
    return clamp_u8(round((float(percent) * 255.0) / 100.0))


def color_from_candidates(data, candidates, fallback_rgb, fallback_a=255):
    for path in candidates:
        value = get_path(data, *path)
        rgb = parse_hex_rgb(value)
        if rgb is not None:
            return (*rgb, fallback_a)
    return (*fallback_rgb, fallback_a)


def main():
    if len(sys.argv) != 3:
        print("usage: generate_aero_theme_header.py <input_json> <output_header>", file=sys.stderr)
        return 1

    input_path = pathlib.Path(sys.argv[1])
    output_path = pathlib.Path(sys.argv[2])

    with input_path.open("r", encoding="utf-8") as f:
        data = json.load(f)

    schema = data.get("schema")
    if schema != "fortress.aero.theme":
        print(f"warning: unexpected schema {schema!r}; continuing with fallbacks", file=sys.stderr)

    # Desktop/chrome base colors.
    window_background = color_from_candidates(
        data,
        [("desktop", "backgroundBottom")],
        (0x1B, 0x22, 0x30),
    )
    title_start = color_from_candidates(
        data,
        [("chrome", "titleActive", "top"), ("desktop", "backgroundTop")],
        (0x32, 0x4B, 0x6A),
    )
    title_end = color_from_candidates(
        data,
        [("chrome", "titleActive", "bottom"), ("desktop", "panelSurface")],
        (0x20, 0x2A, 0x3A),
    )

    # Control body and states.
    accent_secondary = color_from_candidates(
        data,
        [("controls", "states", "normal", "bodyTop"), ("controls", "glassLayers", "bodyGradient", "top")],
        (0x8C, 0xC8, 0xFF),
    )
    accent_primary = color_from_candidates(
        data,
        [("controls", "states", "normal", "bodyBottom"), ("controls", "glassLayers", "bodyGradient", "bottom")],
        (0x4E, 0xA0, 0xFF),
    )

    # Texts.
    text_primary = color_from_candidates(
        data,
        [
            ("controls", "states", "normal", "text"),
            ("controls", "text"),
            ("desktop", "panelText"),
        ],
        (0xF4, 0xF8, 0xFF),
    )
    text_secondary = color_from_candidates(
        data,
        [("desktop", "panelText")],
        (0xC8, 0xD8, 0xF0),
    )

    # Border/shadow.
    border = color_from_candidates(
        data,
        [("chrome", "borderDark"), ("controls", "states", "normal", "border"), ("controls", "base")],
        (0x2E, 0x3B, 0x50),
    )
    shadow_rgb = color_from_candidates(
        data,
        [("chrome", "windowShadow")],
        (0x00, 0x00, 0x00),
    )
    shadow_alpha = pct_to_alpha(get_path(data, "chrome", "windowShadowOpacityPercent"), 0x80)
    shadow = (shadow_rgb[0], shadow_rgb[1], shadow_rgb[2], shadow_alpha)

    # Layer-derived alpha colors.
    inner_glow_rgb = color_from_candidates(
        data,
        [("controls", "glassLayers", "innerTopGlow", "color")],
        (0xFF, 0xFF, 0xFF),
    )
    inner_glow_alpha = pct_to_alpha(get_path(data, "controls", "glassLayers", "innerTopGlow", "opacityPercent"), 0x33)

    gloss_rgb = color_from_candidates(
        data,
        [("controls", "glassLayers", "gloss", "color")],
        (0xFF, 0xFF, 0xFF),
    )
    gloss_alpha = pct_to_alpha(get_path(data, "controls", "glassLayers", "gloss", "opacityPercent"), 0x45)

    pressed_rgb = color_from_candidates(
        data,
        [("controls", "states", "pressed", "bodyTop"), ("controls", "active")],
        (0xFF, 0xFF, 0xFF),
    )

    focus_rgb = color_from_candidates(
        data,
        [("controls", "focusRing")],
        (0x4E, 0xA0, 0xFF),
    )

    palette = {
        "WindowBackground": window_background,
        "TitleBarGradientStart": title_start,
        "TitleBarGradientEnd": title_end,
        "ButtonNormal": (inner_glow_rgb[0], inner_glow_rgb[1], inner_glow_rgb[2], inner_glow_alpha),
        "ButtonHover": (gloss_rgb[0], gloss_rgb[1], gloss_rgb[2], gloss_alpha),
        "ButtonPressed": (pressed_rgb[0], pressed_rgb[1], pressed_rgb[2], 0x66),
        "ButtonGlow": (focus_rgb[0], focus_rgb[1], focus_rgb[2], 0xA0),
        "TextPrimary": text_primary,
        "TextSecondary": text_secondary,
        "Border": border,
        "Shadow": shadow,
        "AccentPrimary": accent_primary,
        "AccentSecondary": accent_secondary,
    }

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="ascii", newline="\n") as f:
        f.write("#ifndef FORTRESS_KERNEL_GENERATED_FAEROTHEME_GENERATED_HPP\n")
        f.write("#define FORTRESS_KERNEL_GENERATED_FAEROTHEME_GENERATED_HPP\n\n")
        f.write("#include \"Fortress/Video/FColor.hpp\"\n\n")
        f.write("namespace Fortress::Kernel::GeneratedAeroTheme {\n\n")
        f.write("static constexpr Fortress::Video::FColor Color(unsigned r, unsigned g, unsigned b, unsigned a) {\n")
        f.write("    return Fortress::Video::FColor{\n")
        f.write("        .R = static_cast<Fortress::Core::uint8>(r),\n")
        f.write("        .G = static_cast<Fortress::Core::uint8>(g),\n")
        f.write("        .B = static_cast<Fortress::Core::uint8>(b),\n")
        f.write("        .A = static_cast<Fortress::Core::uint8>(a),\n")
        f.write("    };\n")
        f.write("}\n\n")

        for key, rgba in palette.items():
            f.write(
                f"static constexpr Fortress::Video::FColor k{key} = Color({rgba[0]}u, {rgba[1]}u, {rgba[2]}u, {rgba[3]}u);\n"
            )

        f.write("\n} // namespace Fortress::Kernel::GeneratedAeroTheme\n\n")
        f.write("#endif\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
