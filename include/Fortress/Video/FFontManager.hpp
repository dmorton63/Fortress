#ifndef FORTRESS_VIDEO_FFONTMANAGER_HPP
#define FORTRESS_VIDEO_FFONTMANAGER_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Video {

#if defined(FORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE)
enum class EKeyboardFontProfile : Fortress::Core::uint8 {
  Classic = 0,
  Dense,
};
#endif

struct FFontGlyphRaster {
    Fortress::Core::uint8 Width = 0;
    Fortress::Core::uint8 Height = 0;
    Fortress::Core::uint8 Rows[8] = {};
};

struct FFontCacheStats {
  Fortress::Core::uint32 Capacity = 0;
  Fortress::Core::uint32 EntryCount = 0;
  Fortress::Core::uint64 HitCount = 0;
  Fortress::Core::uint64 MissCount = 0;
  Fortress::Core::uint64 EvictionCount = 0;
};

class FFontManager {
  public:
    bool Initialize();
    bool TryGetGlyphRaster(char Character, FFontGlyphRaster &OutGlyph) const;
#if defined(FORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE)
    bool SetKeyboardFontProfile(EKeyboardFontProfile profile);
    const char *GetKeyboardFontProfileName() const;
  static void SetGlobalKeyboardFontProfile(EKeyboardFontProfile profile);
  static EKeyboardFontProfile GetGlobalKeyboardFontProfile();
  static const char *GetGlobalKeyboardFontProfileName();
#endif
    void GetCacheStats(FFontCacheStats &OutStats) const;
    void ResetCache();

  private:
    static constexpr Fortress::Core::uint32 CacheCapacity = 96u;

    struct FGlyphCacheEntry {
        bool Valid = false;
        char Character = 0;
  #if defined(FORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE)
      EKeyboardFontProfile Profile = EKeyboardFontProfile::Classic;
  #endif
        FFontGlyphRaster Glyph = {};
    };

  #if defined(FORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE)
    mutable EKeyboardFontProfile GProfile = EKeyboardFontProfile::Classic;
  #endif
    mutable FGlyphCacheEntry GCacheEntries[CacheCapacity] = {};
    mutable Fortress::Core::uint32 GEntryCount = 0;
    mutable Fortress::Core::uint32 GNextEvictIndex = 0;
    mutable Fortress::Core::uint64 GCacheHitCount = 0;
    mutable Fortress::Core::uint64 GCacheMissCount = 0;
    mutable Fortress::Core::uint64 GCacheEvictionCount = 0;
};

} // namespace Fortress::Video

#endif