#include "Fortress/Video/FBasicTextShaper.hpp"

namespace Fortress::Video {

static Fortress::Core::int32 GetGlyphAdvance(Fortress::Core::int32 Scale) {
    return 6 * Scale;
}

static Fortress::Core::int32 GetLineAdvance(Fortress::Core::int32 Scale) {
    return 9 * Scale;
}

bool FBasicTextShaper::ShapeCharacter(char Character,
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

    OutGlyph.Character = Character;
    OutGlyph.DrawX = InOutCursorX;
    OutGlyph.DrawY = InOutCursorY;
    OutHasGlyph = true;

    InOutCursorX += GetGlyphAdvance(Scale);
    return true;
}

} // namespace Fortress::Video