#pragma once
#include "raylib.h"

/*=== Font System — Atlas Resolution Cache ===*/

/**
 * @brief FontId
 * Abstract role untuk font — gak nyentuh file path langsung.
 * Setiap entry punya FontDef yang mapping ke file + default resolution.
 */
enum class FontId : int {
    DEFAULT,         // Poppins-Bold.ttf — font default UI
    KEYBIND_HEADER,  // NewDawn.ttf — header keybinds
    KEYBIND_ENTRY,   // Poppins-Regular.ttf — entry keybinds
    LOADING_TITLE,   // Poppins-Bold.ttf — alias DEFAULT
    HUD_PLAYER,      // Poppins-Bold.ttf — font HUD player

    MEDIEVAL_SHARP,  // MedievalSharp-Regular.ttf
    QUICKSAND_BOLD,  // Quicksand-Bold.ttf
    QUICKSAND_SEMIBOLD, // Quicksand-SemiBold.ttf
    QUICKSAND_MEDIUM,   // Quicksand-Medium.ttf
    QUICKSAND_REGULAR,  // Quicksand-Regular.ttf
    QUICKSAND_LIGHT,    // Quicksand-Light.ttf
    NORSE_BOLD,     // Norsebold.otf
    NORSE,           // Norse.otf

    // Audio Settings tab
    AUDIOSETTS_HEADER,  // NewDawn.ttf, RES_512 — label slider
    AUDIOSETTS_VALUE,   // Poppins-Regular.ttf, RES_256 — value text

    COUNT
};

/**
 * @brief AtlasRes
 * Resolusi atlas font. Nilai langsung dipake sebagai fontSize di LoadFontEx.
 * Makin tinggi => makin tajam text ukuran besar, tapi makin berat memory.
 */
enum class AtlasRes : int {
    RES_128  = 128,
    RES_256  = 256,
    RES_512  = 512,
    RES_1024 = 1024
};

/** @brief Mapping dari FontId ke file + default resolution */
struct FontDef {
    const char* filename;      // nama file aja, e.g. "NewDawn.ttf"
    const char* displayName;   // buat debug/log
    AtlasRes defaultRes;       // resolusi atlas default
};

/** @brief Handle yang di-pass ke DrawTextCached/MeasureTextCached */
struct FontHandle {
    FontId id;
    AtlasRes res;
};

/*=== Core API ===*/

/**
 * @brief Get atau load font dari cache. Lazy load kalo belum ada.
 * @param id FontId (role abstraction)
 * @param res AtlasRes (resolusi atlas)
 * @return Font raylib siap pake
 */
Font GetOrLoad(FontId id, AtlasRes res);

/**
 * @brief GetOrLoad overload — pake defaultRes dari FontDef langsung.
 * @param id FontId (role abstraction)
 * @return Font raylib siap pake
 */
Font GetOrLoad(FontId id);

/**
 * @brief DrawTextEx wrapper — pake FontHandle, otomatis get dari cache.
 * @param fh FontHandle (FontId + AtlasRes)
 */
void DrawTextCached(FontHandle fh, const char* text, Vector2 pos, float fontSize, float spacing, Color tint);

/**
 * @brief MeasureTextEx wrapper — pake FontHandle, otomatis get dari cache.
 * @return Vector2 ukuran text
 */
Vector2 MeasureTextCached(FontHandle fh, const char* text, float fontSize, float spacing);

/**
 * @brief Convenience wrapper — DrawText tapi pake FontId::DEFAULT.
 * Signature sama persis kayak raylib DrawText(), gak perlu casting.
 */
void DrawDefaultText(const char* text, int posX, int posY, int fontSize, Color color);

/** @brief Unload semua cached font */
void UnloadFonts(void);
