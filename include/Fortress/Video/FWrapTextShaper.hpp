#ifndef FORTRESS_VIDEO_FWRAPTEXTSHAPER_HPP
#define FORTRESS_VIDEO_FWRAPTEXTSHAPER_HPP

#include "Fortress/Video/ITextShaper.hpp"

namespace Fortress::Video {

class FWrapTextShaper final : public ITextShaper {
  public:
    bool ShapeCharacter(char Character,
                        Fortress::Core::int32 Scale,
                        Fortress::Core::int32 &InOutCursorX,
                        Fortress::Core::int32 &InOutCursorY,
                        FShapedGlyph &OutGlyph,
                        bool &OutHasGlyph) const override;

    void SetWrapWidthPixels(Fortress::Core::int32 WrapWidthPixels);

  private:
    Fortress::Core::int32 GWrapWidthPixels = 640;
};

} // namespace Fortress::Video

#endif