#ifndef FORTRESS_VIDEO_FVIDEOCONSOLE_HPP
#define FORTRESS_VIDEO_FVIDEOCONSOLE_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Video/FColor.hpp"
#include "Fortress/Video/FDisplayDevice.hpp"
#include "Fortress/Video/FFontManager.hpp"
#include "Fortress/Video/FVideoSurface.hpp"

namespace Fortress::Video {

class FVideoConsole {
  public:
    bool Initialize(FDisplayDevice *inDevice, Fortress::Core::int32 inScale = 2);

    void SetCursor(Fortress::Core::int32 inX, Fortress::Core::int32 inY);
    void SetColors(FColor inForeground, FColor inBackground);

    void Print(const char *text);
    void Print(const FVideoSurfaceView &surface, const char *text);
    void PrintLine(const char *text);
    void PrintLine(const FVideoSurfaceView &surface, const char *text);
  #if defined(FORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE)
    bool SetKeyboardFontProfile(EKeyboardFontProfile profile);
    const char *GetKeyboardFontProfileName() const;
  #endif
    void GetFontCacheStats(FFontCacheStats &OutStats) const;
    void ResetFontCache();

    FDisplayDevice *Device = nullptr;
    Fortress::Core::int32 Scale = 2;
    Fortress::Core::int32 CursorX = 0;
    Fortress::Core::int32 CursorY = 0;
    Fortress::Core::int32 GlyphAdvance = 12;
    Fortress::Core::int32 LineAdvance = 16;
    FFontManager FontManager = {};

    FColor Foreground = FColor::RGB(242, 247, 255);
    FColor Background = FColor::RGB(0, 16, 23);
};

} // namespace Fortress::Video

#endif
