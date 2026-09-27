#include "Fortress/Video/FTextRenderer.hpp"

#include "Fortress/Video/FBasicTextShaper.hpp"
#include "Fortress/Video/FWrapTextShaper.hpp"

namespace Fortress::Video {

static FBasicTextShaper GBasicTextShaper = {};
static FWrapTextShaper GWrapTextShaper = {};
static FTextShaperConfig GShaperConfig = {};

static const ITextShaper *SelectTextShaper() {
    if (GShaperConfig.Mode == ETextShaperMode::Wrap) {
        return &GWrapTextShaper;
    }

    return &GBasicTextShaper;
}

Fortress::Core::int32 FTextRenderer::GetGlyphAdvance(Fortress::Core::int32 Scale) {
    return 6 * Scale;
}

Fortress::Core::int32 FTextRenderer::GetLineAdvance(Fortress::Core::int32 Scale) {
    return 9 * Scale;
}

void FTextRenderer::SetShaperConfig(const FTextShaperConfig &Config) {
    GShaperConfig = Config;
    if (GShaperConfig.WrapWidthPixels < 0) {
        GShaperConfig.WrapWidthPixels = 0;
    }

    GWrapTextShaper.SetWrapWidthPixels(GShaperConfig.WrapWidthPixels);
}

void FTextRenderer::GetShaperConfig(FTextShaperConfig &OutConfig) {
    OutConfig = GShaperConfig;
}

const char *FTextRenderer::GetShaperModeName() {
    switch (GShaperConfig.Mode) {
    case ETextShaperMode::Wrap:
        return "WRAP";
    case ETextShaperMode::Basic:
    default:
        return "BASIC";
    }
}

static void DrawGlyph(const FVideoSurfaceView &Surface,
                      const FFontGlyphRaster &Glyph,
                      Fortress::Core::int32 X,
                      Fortress::Core::int32 Y,
                      Fortress::Core::int32 Scale,
                      Fortress::Core::uint32 ForegroundPacked,
                      Fortress::Core::uint32 BackgroundPacked) {
    for (Fortress::Core::int32 GlyphY = 0; GlyphY < Glyph.Height; GlyphY++) {
        const Fortress::Core::uint8 Row = Glyph.Rows[GlyphY];
        for (Fortress::Core::int32 GlyphX = 0; GlyphX < Glyph.Width; GlyphX++) {
            const bool On = (Row & (1u << (Glyph.Width - 1 - GlyphX))) != 0;
            const Fortress::Core::uint32 PackedColor = On ? ForegroundPacked : BackgroundPacked;
            for (Fortress::Core::int32 ScaleY = 0; ScaleY < Scale; ScaleY++) {
                for (Fortress::Core::int32 ScaleX = 0; ScaleX < Scale; ScaleX++) {
                    FVideoSurfaceOps::DrawPixel32(Surface,
                                                  X + GlyphX * Scale + ScaleX,
                                                  Y + GlyphY * Scale + ScaleY,
                                                  PackedColor);
                }
            }
        }
    }
}

bool FTextRenderer::DrawText(const FVideoSurfaceView &Surface,
                             const FFontManager &FontManager,
                             const char *Text,
                             Fortress::Core::int32 Scale,
                             FColor Foreground,
                             FColor Background,
                             Fortress::Core::int32 &InOutCursorX,
                             Fortress::Core::int32 &InOutCursorY) {
    if (!Surface.IsValid() || Surface.Desc.PixelFormat != EPixelFormat::Masked32 || Text == nullptr || Scale < 1) {
        return false;
    }

    const Fortress::Core::uint32 ForegroundPacked = FVideoSurfaceOps::PackMasked32(Foreground, Surface.Desc.PixelMask);
    const Fortress::Core::uint32 BackgroundPacked = FVideoSurfaceOps::PackMasked32(Background, Surface.Desc.PixelMask);
    const ITextShaper *TextShaper = SelectTextShaper();
    if (TextShaper == nullptr) {
        return false;
    }

    for (Fortress::Core::usize Index = 0; Text[Index] != '\0'; Index++) {
        FShapedGlyph ShapedGlyph{};
        bool HasGlyph = false;
        if (!TextShaper->ShapeCharacter(Text[Index],
                                        Scale,
                                        InOutCursorX,
                                        InOutCursorY,
                                        ShapedGlyph,
                                        HasGlyph)) {
            return false;
        }

        if (!HasGlyph) {
            continue;
        }

        FFontGlyphRaster Glyph{};
        if (!FontManager.TryGetGlyphRaster(ShapedGlyph.Character, Glyph)) {
            continue;
        }

        DrawGlyph(Surface,
                  Glyph,
                  ShapedGlyph.DrawX,
                  ShapedGlyph.DrawY,
                  Scale,
                  ForegroundPacked,
                  BackgroundPacked);
    }

    return true;
}

} // namespace Fortress::Video