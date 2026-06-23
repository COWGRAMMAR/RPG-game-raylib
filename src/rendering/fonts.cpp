/**
 * @file fonts.cpp
 * @brief Implementasi Font System dengan Atlas Resolution Cache
 *
 * Cache key: (FontId << 16) | AtlasRes sebagai uint32_t.
 * Font di-load lazy via GetOrLoad() dengan default resolution dari FontDef.
 */
#include "fonts.h"
#include <unordered_map>
#include <cstdio>

/*=== Font Definitions ===*/
static const FontDef FONT_DEFS[(int)FontId::COUNT] = {
    {"Poppins-Bold.ttf", "Poppins-Bold", AtlasRes::RES_256},             // DEFAULT
    {"NewDawn.ttf", "NewDawn", AtlasRes::RES_256},                       // KEYBIND_HEADER
    {"Quicksand-Bold.ttf", "Quicksand-Bold", AtlasRes::RES_256},         // KEYBIND_ENTRY
    {"Poppins-Bold.ttf", "Poppins-Bold", AtlasRes::RES_256},             // LOADING_TITLE
    {"Poppins-Bold.ttf", "HUD_Player", AtlasRes::RES_256},               // HUD_PLAYER
    {"Poppins-Bold.ttf", "InventoryUI", AtlasRes::RES_256},              // INVENTORY_UI
    {"MedievalSharp-Regular.ttf", "MedievalSharp", AtlasRes::RES_256},   // MEDIEVAL_SHARP
    {"Quicksand-Bold.ttf", "Quicksand-Bold", AtlasRes::RES_256},         // QUICKSAND_BOLD
    {"Quicksand-SemiBold.ttf", "Quicksand-SemiBold", AtlasRes::RES_256}, // QUICKSAND_SEMIBOLD
    {"Quicksand-Medium.ttf", "Quicksand-Medium", AtlasRes::RES_256},     // QUICKSAND_MEDIUM
    {"Quicksand-Regular.ttf", "Quicksand-Regular", AtlasRes::RES_256},   // QUICKSAND_REGULAR
    {"Quicksand-Light.ttf", "Quicksand-Light", AtlasRes::RES_256},       // QUICKSAND_LIGHT
    {"Norsebold.otf", "Norsebold", AtlasRes::RES_256},                   // NORSE_BOLD
    {"Norse.otf", "Norse", AtlasRes::RES_256},                           // NORSE
    {"NewDawn.ttf", "NewDawn", AtlasRes::RES_512},                       // AUDIOSETTS_HEADER
    {"Poppins-Regular.ttf", "Poppins-Regular", AtlasRes::RES_256},       // AUDIOSETTS_VALUE
    {"NewDawn.ttf", "NewDawn", AtlasRes::RES_512},                       // VIDEOSETTS_LABEL
    {"Poppins-Regular.ttf", "Poppins-Regular", AtlasRes::RES_256},       // SAVESLOT_TEXT
    {"Poppins-Bold.ttf", "MinimapUI", AtlasRes::RES_256},                // MINIMAP_UI
};

/*=== Cache System ===*/
struct CachedFont
{
    Font font;
    int refCount;
};

static std::unordered_map<uint32_t, CachedFont> s_FontCache;

/*=== Search Paths ===*/
static const char *FONT_SEARCH_PATHS[] = {
    "assets/fonts/",
    "build/bin/assets/fonts/"};

/*=== Static Helpers ===*/

/** @brief Pack FontId + AtlasRes jadi uint32_t key buat cache lookup */
static uint32_t PackKey(FontId id, AtlasRes res)
{
    return ((uint32_t)id << 16) | (uint32_t)res;
}

/**
 * @brief Cari font file di semua search path
 * @param filename Nama file font (e.g. "NewDawn.ttf")
 * @param outBuf Output buffer untuk path lengkap yang valid
 * @return true kalo ketemu
 */
static bool FindFontPath(const char *filename, char *outBuf, size_t bufSize)
{
    for (int pass = 0; pass < 2; pass++)
    {
        snprintf(outBuf, bufSize, "%s%s", FONT_SEARCH_PATHS[pass], filename);
        if (FileExists(outBuf))
            return true;
    }
    return false;
}

/*=== Public API ===*/

Font GetOrLoad(FontId id, AtlasRes res)
{
    uint32_t key = PackKey(id, res);
    auto it = s_FontCache.find(key);
    if (it != s_FontCache.end())
    {
        return it->second.font;
    }

    Font result = {0};
    const FontDef &def = FONT_DEFS[(int)id];
    char path[256];

    if (FindFontPath(def.filename, path, sizeof(path)))
    {
        result = LoadFontEx(path, (int)res, 0, 0);
    }
    else
    {
        TraceLog(LOG_WARNING, "FONTS: %s not found in any search path", def.filename);
    }

    // Fallback kalo font gagal load / 0 glyphs
    if (result.glyphCount == 0)
    {
        TraceLog(LOG_WARNING, "FONTS: %s loaded but has 0 glyphs (using default font fallback)", def.displayName);
        result = GetFontDefault();
    }

    SetTextureFilter(result.texture, TEXTURE_FILTER_BILINEAR);
    s_FontCache[key] = {result, 1};
    return result;
}

/** @brief GetOrLoad overload — pake defaultRes dari FontDef langsung. */
Font GetOrLoad(FontId id)
{
    return GetOrLoad(id, FONT_DEFS[(int)id].defaultRes);
}

void DrawTextCached(FontHandle fh, const char *text, Vector2 pos, float fontSize, float spacing, Color tint)
{
    Font font = GetOrLoad(fh.id, fh.res);
    DrawTextEx(font, text, pos, fontSize, spacing, tint);
}

Vector2 MeasureTextCached(FontHandle fh, const char *text, float fontSize, float spacing)
{
    Font font = GetOrLoad(fh.id, fh.res);
    return MeasureTextEx(font, text, fontSize, spacing);
}

/** @brief Convenience wrapper — DrawText pake FontId::DEFAULT, signature sama kayak raylib DrawText. */
void DrawDefaultText(const char *text, int posX, int posY, int fontSize, Color color)
{
    DrawTextEx(GetOrLoad(FontId::DEFAULT), text, {(float)posX, (float)posY}, (float)fontSize, 1, color);
}

void UnloadFonts(void)
{
    for (auto &[key, cached] : s_FontCache)
    {
        UnloadFont(cached.font);
    }
    s_FontCache.clear();
    TraceLog(LOG_INFO, "FONTS: Unloaded all cached fonts");
}
