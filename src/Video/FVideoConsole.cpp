#include "Fortress/Video/FVideoConsole.hpp"

#include "Fortress/Video/FTextRenderer.hpp"

namespace Fortress::Video {

bool FVideoConsole::Initialize(FDisplayDevice *inDevice, Fortress::Core::int32 inScale) {
    if (inDevice == nullptr || inScale < 1) {
        return false;
    }

    if (!FontManager.Initialize()) {
        return false;
    }

    Device = inDevice;
    Scale = inScale;
    GlyphAdvance = FTextRenderer::GetGlyphAdvance(Scale);
    LineAdvance = FTextRenderer::GetLineAdvance(Scale);
    CursorX = 0;
    CursorY = 0;
    return true;
}

void FVideoConsole::SetCursor(Fortress::Core::int32 inX, Fortress::Core::int32 inY) {
    CursorX = inX;
    CursorY = inY;
}

void FVideoConsole::SetColors(FColor inForeground, FColor inBackground) {
    Foreground = inForeground;
    Background = inBackground;
}

void FVideoConsole::Print(const char *text) {
    if (Device == nullptr) {
        return;
    }

    Print(Device->GetBackSurface(), text);
}

void FVideoConsole::Print(const FVideoSurfaceView &surface, const char *text) {
    (void)FTextRenderer::DrawText(surface,
                                  FontManager,
                                  text,
                                  Scale,
                                  Foreground,
                                  Background,
                                  CursorX,
                                  CursorY);
}

void FVideoConsole::PrintLine(const char *text) {
    Print(text);
    CursorX = 0;
    CursorY += LineAdvance;
}

void FVideoConsole::PrintLine(const FVideoSurfaceView &surface, const char *text) {
    Print(surface, text);
    CursorX = 0;
    CursorY += LineAdvance;
}

#if defined(FORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE)
bool FVideoConsole::SetKeyboardFontProfile(EKeyboardFontProfile profile) {
    return FontManager.SetKeyboardFontProfile(profile);
}

const char *FVideoConsole::GetKeyboardFontProfileName() const {
    return FontManager.GetKeyboardFontProfileName();
}
#endif

void FVideoConsole::GetFontCacheStats(FFontCacheStats &OutStats) const {
    FontManager.GetCacheStats(OutStats);
}

void FVideoConsole::ResetFontCache() {
    FontManager.ResetCache();
}

} // namespace Fortress::Video
