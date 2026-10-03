#include "Fortress/Video/FFontManager.hpp"

namespace Fortress::Video {

#if defined(FORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE)
static EKeyboardFontProfile GGlobalKeyboardFontProfile = EKeyboardFontProfile::Classic;
#endif

static const Fortress::Core::uint8 *GetGlyphRows(char Character) {
    static const Fortress::Core::uint8 GSpace[7] = {0, 0, 0, 0, 0, 0, 0};
    static const Fortress::Core::uint8 GUnknown[7] = {0b11111, 0b00001, 0b00110, 0b00100, 0b00000, 0b00100, 0b00100};

    static const Fortress::Core::uint8 GA[7] = {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001};
    static const Fortress::Core::uint8 GB[7] = {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110};
    static const Fortress::Core::uint8 GC[7] = {0b01111, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b01111};
    static const Fortress::Core::uint8 GD[7] = {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110};
    static const Fortress::Core::uint8 GE[7] = {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111};
    static const Fortress::Core::uint8 GF[7] = {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000};
    static const Fortress::Core::uint8 GG[7] = {0b01111, 0b10000, 0b10000, 0b10011, 0b10001, 0b10001, 0b01110};
    static const Fortress::Core::uint8 GH[7] = {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001};
    static const Fortress::Core::uint8 GI[7] = {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b11111};
    static const Fortress::Core::uint8 GJ[7] = {0b00011, 0b00001, 0b00001, 0b00001, 0b10001, 0b10001, 0b01110};
    static const Fortress::Core::uint8 GK[7] = {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001};
    static const Fortress::Core::uint8 GL[7] = {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111};
    static const Fortress::Core::uint8 GM[7] = {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001};
    static const Fortress::Core::uint8 GN[7] = {0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001};
    static const Fortress::Core::uint8 GO[7] = {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110};
    static const Fortress::Core::uint8 GP[7] = {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000};
    static const Fortress::Core::uint8 GQ[7] = {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101};
    static const Fortress::Core::uint8 GR[7] = {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001};
    static const Fortress::Core::uint8 GS[7] = {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110};
    static const Fortress::Core::uint8 GT[7] = {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100};
    static const Fortress::Core::uint8 GU[7] = {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110};
    static const Fortress::Core::uint8 GV[7] = {0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b01010, 0b00100};
    static const Fortress::Core::uint8 GW[7] = {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010};
    static const Fortress::Core::uint8 GX[7] = {0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b01010, 0b10001};
    static const Fortress::Core::uint8 GY[7] = {0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100};
    static const Fortress::Core::uint8 GZ[7] = {0b11111, 0b00010, 0b00100, 0b00100, 0b01000, 0b10000, 0b11111};

    static const Fortress::Core::uint8 G0[7] = {0b01110, 0b10011, 0b10101, 0b10101, 0b10101, 0b11001, 0b01110};
    static const Fortress::Core::uint8 G1[7] = {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110};
    static const Fortress::Core::uint8 G2[7] = {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111};
    static const Fortress::Core::uint8 G3[7] = {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110};
    static const Fortress::Core::uint8 G4[7] = {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010};
    static const Fortress::Core::uint8 G5[7] = {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110};
    static const Fortress::Core::uint8 G6[7] = {0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110};
    static const Fortress::Core::uint8 G7[7] = {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000};
    static const Fortress::Core::uint8 G8[7] = {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110};
    static const Fortress::Core::uint8 G9[7] = {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110};

    static const Fortress::Core::uint8 GColon[7] = {0, 0b00100, 0, 0, 0b00100, 0, 0};
    static const Fortress::Core::uint8 GDash[7] = {0, 0, 0, 0b11111, 0, 0, 0};
    static const Fortress::Core::uint8 GUDash[7] = {0, 0, 0, 0, 0, 0, 0b11111};

    if (Character >= 'a' && Character <= 'z') {
        Character = static_cast<char>(Character - 'a' + 'A');
    }

    switch (Character) {
        case ' ': return GSpace;
        case ':': return GColon;
        case '-': return GDash;
        case '_': return GUDash;
        case 'A': return GA;
        case 'B': return GB;
        case 'C': return GC;
        case 'D': return GD;
        case 'E': return GE;
        case 'F': return GF;
        case 'G': return GG;
        case 'H': return GH;
        case 'I': return GI;
        case 'J': return GJ;
        case 'K': return GK;
        case 'L': return GL;
        case 'M': return GM;
        case 'N': return GN;
        case 'O': return GO;
        case 'P': return GP;
        case 'Q': return GQ;
        case 'R': return GR;
        case 'S': return GS;
        case 'T': return GT;
        case 'U': return GU;
        case 'V': return GV;
        case 'W': return GW;
        case 'X': return GX;
        case 'Y': return GY;
        case 'Z': return GZ;
        case '0': return G0;
        case '1': return G1;
        case '2': return G2;
        case '3': return G3;
        case '4': return G4;
        case '5': return G5;
        case '6': return G6;
        case '7': return G7;
        case '8': return G8;
        case '9': return G9;
        default: return GUnknown;
    }
}

static bool BuildClassicGlyph(char Character, FFontGlyphRaster &OutGlyph) {
    const Fortress::Core::uint8 *Rows = GetGlyphRows(Character);
    if (Rows == nullptr) {
        return false;
    }

    OutGlyph = FFontGlyphRaster{};
    OutGlyph.Width = 5u;
    OutGlyph.Height = 7u;
    for (Fortress::Core::uint32 Index = 0; Index < OutGlyph.Height; Index++) {
        OutGlyph.Rows[Index] = Rows[Index];
    }

    return true;
}

#if defined(FORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE)
static const char *GetProfileName(EKeyboardFontProfile profile) {
    switch (profile) {
    case EKeyboardFontProfile::Dense:
        return "DENSE";
    case EKeyboardFontProfile::Classic:
    default:
        return "CLASSIC";
    }
}

static bool BuildDenseGlyph(char Character, FFontGlyphRaster &OutGlyph) {
    FFontGlyphRaster BaseGlyph{};
    if (!BuildClassicGlyph(Character, BaseGlyph)) {
        return false;
    }

    OutGlyph = BaseGlyph;
    OutGlyph.Width = 6u;
    for (Fortress::Core::uint32 RowIndex = 0u; RowIndex < OutGlyph.Height; RowIndex++) {
        const Fortress::Core::uint8 BaseRow = static_cast<Fortress::Core::uint8>(BaseGlyph.Rows[RowIndex] & 0x1Fu);
        Fortress::Core::uint8 Expanded = static_cast<Fortress::Core::uint8>((BaseRow << 1u) | (BaseRow >> 4u));
        Expanded = static_cast<Fortress::Core::uint8>((Expanded | (Expanded >> 1u)) & 0x3Fu);
        OutGlyph.Rows[RowIndex] = Expanded;
    }

    return true;
}

static bool BuildProfileGlyph(EKeyboardFontProfile profile, char Character, FFontGlyphRaster &OutGlyph) {
    switch (profile) {
    case EKeyboardFontProfile::Dense:
        return BuildDenseGlyph(Character, OutGlyph);
    case EKeyboardFontProfile::Classic:
    default:
        return BuildClassicGlyph(Character, OutGlyph);
    }
}
#endif

bool FFontManager::Initialize() {
#if defined(FORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE)
    GProfile = GGlobalKeyboardFontProfile;
#endif
    ResetCache();
    return true;
}

bool FFontManager::TryGetGlyphRaster(char Character, FFontGlyphRaster &OutGlyph) const {
#if defined(FORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE)
    if (GProfile != GGlobalKeyboardFontProfile) {
        GProfile = GGlobalKeyboardFontProfile;
        const_cast<FFontManager *>(this)->ResetCache();
    }
#endif
    for (Fortress::Core::uint32 Index = 0; Index < CacheCapacity; Index++) {
        const FGlyphCacheEntry &entry = GCacheEntries[Index];
#if defined(FORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE)
        if (!entry.Valid || entry.Character != Character || entry.Profile != GProfile) {
#else
        if (!entry.Valid || entry.Character != Character) {
#endif
            continue;
        }

        OutGlyph = entry.Glyph;
        GCacheHitCount++;
        return true;
    }

#if defined(FORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE)
    if (!BuildProfileGlyph(GProfile, Character, OutGlyph)) {
#else
    if (!BuildClassicGlyph(Character, OutGlyph)) {
#endif
        GCacheMissCount++;
        return false;
    }

    GCacheMissCount++;

    Fortress::Core::uint32 insertIndex = CacheCapacity;
    if (GEntryCount < CacheCapacity) {
        for (Fortress::Core::uint32 Index = 0; Index < CacheCapacity; Index++) {
            if (!GCacheEntries[Index].Valid) {
                insertIndex = Index;
                break;
            }
        }
    }

    if (insertIndex >= CacheCapacity) {
        insertIndex = GNextEvictIndex;
        GNextEvictIndex = (GNextEvictIndex + 1u) % CacheCapacity;
        GCacheEvictionCount++;
    }

    GCacheEntries[insertIndex] = FGlyphCacheEntry{
        .Valid = true,
        .Character = Character,
#if defined(FORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE)
        .Profile = GProfile,
#endif
        .Glyph = OutGlyph,
    };

    if (GEntryCount < CacheCapacity) {
        GEntryCount++;
    }

    return true;
}

#if defined(FORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE)
bool FFontManager::SetKeyboardFontProfile(EKeyboardFontProfile profile) {
    if (GGlobalKeyboardFontProfile == profile && GProfile == profile) {
        return false;
    }

    GGlobalKeyboardFontProfile = profile;
    GProfile = profile;
    ResetCache();
    return true;
}

const char *FFontManager::GetKeyboardFontProfileName() const {
    return GetProfileName(GProfile);
}

void FFontManager::SetGlobalKeyboardFontProfile(EKeyboardFontProfile profile) {
    GGlobalKeyboardFontProfile = profile;
}

EKeyboardFontProfile FFontManager::GetGlobalKeyboardFontProfile() {
    return GGlobalKeyboardFontProfile;
}

const char *FFontManager::GetGlobalKeyboardFontProfileName() {
    return GetProfileName(GGlobalKeyboardFontProfile);
}
#endif

void FFontManager::GetCacheStats(FFontCacheStats &OutStats) const {
    OutStats = FFontCacheStats{
        .Capacity = CacheCapacity,
        .EntryCount = GEntryCount,
        .HitCount = GCacheHitCount,
        .MissCount = GCacheMissCount,
        .EvictionCount = GCacheEvictionCount,
    };
}

void FFontManager::ResetCache() {
    for (Fortress::Core::uint32 Index = 0; Index < CacheCapacity; Index++) {
        GCacheEntries[Index] = FGlyphCacheEntry{};
    }

    GEntryCount = 0;
    GNextEvictIndex = 0;
    GCacheHitCount = 0;
    GCacheMissCount = 0;
    GCacheEvictionCount = 0;
}

} // namespace Fortress::Video