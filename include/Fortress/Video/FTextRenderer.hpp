#ifndef FORTRESS_VIDEO_FTEXTRENDERER_HPP
#define FORTRESS_VIDEO_FTEXTRENDERER_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Video/FColor.hpp"
#include "Fortress/Video/FFontManager.hpp"
#include "Fortress/Video/FVideoSurface.hpp"

namespace Fortress::Video {

enum class ETextShaperMode : Fortress::Core::uint8 {
  Basic = 0,
  Wrap = 1,
};

struct FTextShaperConfig {
  ETextShaperMode Mode = ETextShaperMode::Basic;
  Fortress::Core::int32 WrapWidthPixels = 640;
};

class FTextRenderer {
  public:
    static Fortress::Core::int32 GetGlyphAdvance(Fortress::Core::int32 Scale);
    static Fortress::Core::int32 GetLineAdvance(Fortress::Core::int32 Scale);
  static void SetShaperConfig(const FTextShaperConfig &Config);
  static void GetShaperConfig(FTextShaperConfig &OutConfig);
  static const char *GetShaperModeName();
    static bool DrawText(const FVideoSurfaceView &Surface,
                         const FFontManager &FontManager,
                         const char *Text,
                         Fortress::Core::int32 Scale,
                         FColor Foreground,
                         FColor Background,
                         Fortress::Core::int32 &InOutCursorX,
                         Fortress::Core::int32 &InOutCursorY);
};

} // namespace Fortress::Video

#endif