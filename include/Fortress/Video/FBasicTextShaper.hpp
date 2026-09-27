#ifndef FORTRESS_VIDEO_FBASICTEXTSHAPER_HPP
#define FORTRESS_VIDEO_FBASICTEXTSHAPER_HPP

#include "Fortress/Video/ITextShaper.hpp"

namespace Fortress::Video {

class FBasicTextShaper final : public ITextShaper {
  public:
    bool ShapeCharacter(char Character,
                        Fortress::Core::int32 Scale,
                        Fortress::Core::int32 &InOutCursorX,
                        Fortress::Core::int32 &InOutCursorY,
                        FShapedGlyph &OutGlyph,
                        bool &OutHasGlyph) const override;
};

} // namespace Fortress::Video

#endif