#include "Fortress/Video/FWrapTextShaper.hpp"

namespace Fortress::Video {

static Fortress::Core::int32 GetGlyphAdvance(Fortress::Core::int32 Scale) {
    return 6 * Scale;
}

static Fortress::Core::int32 GetLineAdvance(Fortress::Core::int32 Scale) {
    return 9 * Scale;
}

void FWrapTextShaper::SetWrapWidthPixels(Fortress::Core::int32 WrapWidthPixels) {
    if (WrapWidthPixels < 0) {
        GWrapWidthPixels = 0;
        return;
    }

    GWrapWidthPixels = WrapWidthPixels;
}

bool FWrapTextShaper::ShapeCharacter(char Character,
                                     Fortress::Core::int32 Scale,
                                     Fortress::Core::int32 &InOutCursorX,
                                     Fortress::Core::int32 &InOutCursorY,
                                     FShapedGlyph &OutGlyph,
                                     bool &OutHasGlyph) const {
    if (Scale < 1) {
        return false;
    }

    OutHasGlyph = false;
    OutGlyph = FShapedGlyph{};

    if (Character == '\n') {
        InOutCursorX = 0;
        InOutCursorY += GetLineAdvance(Scale);
        return true;
    }

    const Fortress::Core::int32 GlyphAdvance = GetGlyphAdvance(Scale);
    if (GWrapWidthPixels > 0 && InOutCursorX > 0 && (InOutCursorX + GlyphAdvance) > GWrapWidthPixels) {
        InOutCursorX = 0;
        InOutCursorY += GetLineAdvance(Scale);
    }

    OutGlyph.Character = Character;
    OutGlyph.DrawX = InOutCursorX;
    OutGlyph.DrawY = InOutCursorY;
    OutHasGlyph = true;

    InOutCursorX += GlyphAdvance;
    return true;
}

} // namespace Fortress::Video