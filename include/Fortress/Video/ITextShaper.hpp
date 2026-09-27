#ifndef FORTRESS_VIDEO_ITEXTSHAPER_HPP
#define FORTRESS_VIDEO_ITEXTSHAPER_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Video {

struct FShapedGlyph {
    char Character = 0;
    Fortress::Core::int32 DrawX = 0;
    Fortress::Core::int32 DrawY = 0;
};

class ITextShaper {
  public:
    virtual bool ShapeCharacter(char Character,
                                Fortress::Core::int32 Scale,
                                Fortress::Core::int32 &InOutCursorX,
                                Fortress::Core::int32 &InOutCursorY,
                                FShapedGlyph &OutGlyph,
                                bool &OutHasGlyph) const = 0;

  protected:
    ~ITextShaper() = default;
};

} // namespace Fortress::Video

#endif