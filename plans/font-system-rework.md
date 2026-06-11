# Font System Rework — Implementation Plan

## Goal
Bikin font system dengan atlas resolution caching: `FontId` (abstract role) → `FontDef` (asset), lazy load per resolution, wrapper `DrawTextCached()`.

---

## 1. Data Structures

### `enum class FontId`
```cpp
enum class FontId : int {
    KEYBIND_HEADER,  // NewDawn.ttf
    KEYBIND_ENTRY,   // Poppins-Regular.ttf
    LOADING_TITLE,   // Poppins-Bold.ttf
    COUNT
};
```
Abstract role — gak nyentuh file path langsung.

### `enum class AtlasRes`
```cpp
enum class AtlasRes : int {
    RES_128  = 128,
    RES_256  = 256,
    RES_512  = 512,
    RES_1024 = 1024
};
```
Nilai langsung = `fontSize` parameter di `LoadFontEx`.

### `struct FontDef`
```cpp
struct FontDef {
    const char* filePath;
    const char* displayName; // debug/log
    AtlasRes defaultRes;
};
```
Array static, indexed by `(int)FontId`. Urutan harus match enum.

### `struct FontHandle`
```cpp
struct FontHandle {
    FontId id;
    AtlasRes res;
};
```
Yang di-pass ke `DrawTextCached()`.

### `struct CachedFont`
```cpp
struct CachedFont {
    Font font;
    int refCount;  // optional, buat debug
};
```
Isi cache.

---

## 2. Cache System

### Key design
 `std::pair<FontId, AtlasRes>` **gak punya default hash** di C++ — bakal gagal compile.
Pake **combined `uint32_t` key**:
```
uint32_t key = ((uint32_t)id << 16) | (uint32_t)res;
              ^^^^^^^^^^^^^^^^^^^^   ^^^^^^^^^^^^^^^^
              FontId di upper 16bit  AtlasRes di lower 16bit
```

### Storage
```cpp
static std::unordered_map<uint32_t, CachedFont> s_FontCache;
```

### Lazy Load Logic
```
GetOrLoad(FontId id, AtlasRes res):
    uint32_t key = PackKey(id, res);
    if key in s_FontCache → return s_FontCache[key].font
    path = FONT_DEFS[(int)id].filePath
    f = LoadFontEx(path, (int)res, 0, 0)
    if f.glyphCount == 0:
        TraceLog(WARNING, "... fallback to default")
        f = GetFontDefault()
    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR)
    s_FontCache[key] = { f, 1 }
    return f
```

### Init / Unload
```cpp
void InitFonts() {
    // lightweight — cuma clear cache flag, gak load apapun
    s_FontsInitialized = true;
}

void UnloadFonts() {
    for (auto& [key, cached] : s_FontCache)
        UnloadFont(cached.font);
    s_FontCache.clear();
}
```

---

## 3. Public API

### `DrawTextCached(FontHandle fh, const char* text, Vector2 pos, float fontSize, float spacing, Color tint)`
- Panggil `GetOrLoad(fh.id, fh.res)` — dapet `Font`
- `DrawTextEx(font, text, pos, fontSize, spacing, tint)`
- Overload: versi `DrawTextCached(FontId id, AtlasRes res, ...)` langsung

### `FontHandle FontDefault(FontId id)`
- Return `{ id, FONT_DEFS[(int)id].defaultRes }`

### `Vector2 MeasureTextCached(FontHandle fh, const char* text, float fontSize, float spacing)`
- Panggil `GetOrLoad` → `MeasureTextEx(font, text, fontSize, spacing)`

---

## 4. File Changes

| File | Action |
|------|--------|
| `include/rendering/fonts.h` | **Replace** — definisi FontId, AtlasRes, FontDef, FontHandle, API |
| `src/rendering/fonts.cpp` | **New** — cache logic, GetOrLoad, DrawTextCached, Init/Unload |
| `src/rendering/animation.cpp` | **Edit** — hapus `InitFonts()` call dari `InitTextures()`, hapus font globals |
| `src/rendering/animation.cpp` | **Reassign** — font global definitions jadi include fonts.h aja |
| `src/core/main.cpp` | **Edit** — `InitFonts()` tetap dipanggil, UnloadFonts() tetap |
| `src/core/screen_handler.cpp` | **Edit** — tetap panggil UnloadFonts() |
| `src/core/loading_screen.cpp` | **Edit** — `InitFonts()` tetap, tapi sekarang lightweight |

### Call site migration (ganti `DrawTextEx(fontLoadingTitle,...)` → `DrawTextCached(...)`):

| File | Jumlah | FontId target |
|------|--------|---------------|
| `hud.cpp` | ~8 | LOADING_TITLE |
| `keybindsTab.cpp` | ~10 | KEYBIND_HEADER / KEYBIND_ENTRY / LOADING_TITLE |
| `loading_screen.cpp` | ~3 | LOADING_TITLE |
| `saveLoadScreen.cpp` | ~7 | LOADING_TITLE + KEYBIND_ENTRY |
| `popup.cpp` | ~2 | LOADING_TITLE |
| `audioTab.cpp` | ~2 | LOADING_TITLE |
| `videoTab.cpp` | ~2 | LOADING_TITLE |
| `pauseMenu.cpp` | ~4 | LOADING_TITLE |
| `item.cpp` | ~1 | LOADING_TITLE |

### Known gaps (di-skip di rework ini)
- `button.h` — semua button masih pake `GetFontDefault()`. Akan di-consolidate terpisah.
- `debugmode.cpp` / `combatTurn.cpp` / `effects.cpp` / `screen_handler.cpp` — masih pake `DrawText` default font. Belum kena migrasi.
- Font fallback system (kalo glyph gak ada di font yg di-load).

---

## 5. Migration Strategy

**Phase 1** — Bikin file + cache system (gak pecahin existing code):
1. Tulis `fonts.h` (replace) + `fonts.cpp` (new)
2. Jaga `fontKeybindHeader/fontKeybindEntry/fontLoadingTitle` sebagai global sementara dari `GetOrLoad` dengan default res
3. `InitFonts()` jadi lightweight
4. Build → verify gak ada yg broken

**Phase 2** — Call site migration:
1. Per file: ganti `DrawTextEx(fontLoadingTitle, ...)` → `DrawTextCached({FontId::LOADING_TITLE, AtlasRes::RES_256}, ...)`
2. Per file: ganti `DrawTextEx(fontKeybindHeader, ...)` → `DrawTextCached({FontId::KEYBIND_HEADER, AtlasRes::RES_256}, ...)`
3. Per file: ganti `DrawTextEx(fontKeybindEntry, ...)` → `DrawTextCached({FontId::KEYBIND_ENTRY, AtlasRes::RES_256}, ...)`

**Phase 3** — Cleanup:
1. Hapus font globals dari `animation.cpp`
2. Hapus `InitFonts()` call dari `InitTextures()`
3. Hapus `fonts.h` lama + include referencenya dari animation.cpp

---

## 6. Edge Cases

| Case | Handling |
|------|----------|
| Font file missing | `glyphCount == 0` → `GetFontDefault()` fallback |
| InitFonts() dipanggil multiple times | Idempotent — cuma set flag, gak redundant load |
| Cache full / memory pressure | Lazy load — cuma fonts yg dipake aja di-cache |
| Thread safety | Single-threaded (raylib), gak perlu lock |
| 0 glyphs dari LoadFontEx | Fallback + TraceLog warning |
| `std::pair` gak punya default hash | Pake `uint32_t` combined key `(id<<16 \| res)` |
