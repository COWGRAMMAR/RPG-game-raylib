# Font System — Atlas Resolution Cache

> Dokumentasi ini menjelaskan pipeline font system yang udah kita bikin ulang.
> Cocok buat temen-temen yang mau pake font di screen/module baru.

---

## Daftar Isi

1. [Apa masalahnya?](#1-apa-masalahnya)
2. [Arsitektur](#2-arsitektur)
3. [API Reference](#3-api-reference)
4. [Panduan Pemakaian](#4-panduan-pemakaian)
5. [Daftar Font Tersedia](#5-daftar-font-tersedia)
6. [Contoh Lengkap](#6-contoh-lengkap)
7. [Best Practices](#7-best-practices)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. Apa masalahnya?

Dulu kita panggil `LoadFontEx()` berkali-kali di tempat beda tanpa cache. Akibatnya:

- Font yang sama di-load berulang (boros memory)
- Atlas resolution gak konsisten antar screen
- Glyph set di-render ulang tiap ganti screen

**Solusi**: Font system baru dengan cache key `(FontId << 16) | AtlasRes`. Font di-load **lazy** — cuma sekali, terus di-cache.

---

## 2. Arsitektur

### Pipeline

```txt
┌──────────┐      ┌──────────────┐      ┌─────────────┐      ┌────────────┐
│ FontId   │ ──→  │ FontDef      │ ──→  │ GetOrLoad() │ ──→  │ CachedFont │
│ (enum)   │      │ (filename +  │      │ (lazy load) │      │ (cache)    │
│          │      │  defaultRes) │      │             │      │            │
└──────────┘      └──────────────┘      └─────────────┘      └────────────┘
     ▲                    ▲                    │
     │                    │                    │
FontHandle            AtlasRes          DrawTextCached()
(id + res)            (enum)            MeasureTextCached()
```

### Komponen Utama

| Komponen | File | Fungsi |
| --- | --- | --- |
| `FontId` | `include/rendering/fonts.h` | Enum abstract role — gak nyentuh file path |
| `AtlasRes` | `include/rendering/fonts.h` | Resolusi atlas font (128/256/512/1024) |
| `FontDef` | `include/rendering/fonts.h` | Mapping dari FontId ke filename + default resolution |
| `FontHandle` | `include/rendering/fonts.h` | Struct buat nge-pass FontId + AtlasRes sebagai 1 parameter |
| `GetOrLoad()` | `src/rendering/fonts.cpp` | Lazy load + cache lookup |
| `DrawTextCached()` | `src/rendering/fonts.cpp` | DrawTextEx wrapper — otomatis get dari cache |
| `DrawDefaultText()` | `src/rendering/fonts.cpp` | DrawText sederhana pake FontId::DEFAULT |

### Cache Key

Cache pake `std::unordered_map` dengan key `uint32_t`:

```txt
key = (FontId << 16) | AtlasRes
```

Jadi `FontId::KEYBIND_HEADER (1)` + `AtlasRes::RES_256 (256)` = key `0x00010100`.

### Search Path

Font file dicari di 2 path secara berurutan:

1. `assets/fonts/` — path development
2. `build/bin/assets/fonts/` — path setelah build

---

## 3. API Reference

### #1 — `GetOrLoad(FontId id, AtlasRes res)`

Load font dari cache. Kalo belum ada, di-load dari file.

```cpp
Font font = GetOrLoad(FontId::LOADING_TITLE);
// atau dengan resolusi spesifik:
Font font = GetOrLoad(FontId::LOADING_TITLE, AtlasRes::RES_512);
```

### #2 — `GetOrLoad(FontId id)`

Overload — pake default resolution dari `FontDef`.

```cpp
Font font = GetOrLoad(FontId::KEYBIND_HEADER);
```

### #3 — `DrawTextCached(FontHandle fh, const char* text, Vector2 pos, float fontSize, float spacing, Color tint)`

DrawTextEx wrapper — otomatis get font dari cache berdasarkan `FontHandle`.

```cpp
DrawTextCached(
    {FontId::DEFAULT, AtlasRes::RES_256},  // FontHandle
    "Hello World",                          // text
    {100, 200},                             // posisi
    20,                                     // fontSize
    1,                                      // spacing
    WHITE                                   // color
);
```

### #4 — `MeasureTextCached(FontHandle fh, const char* text, float fontSize, float spacing)`

MeasureTextEx wrapper — ngukur ukuran text pake font dari cache.

```cpp
Vector2 size = MeasureTextCached(
    {FontId::LOADING_TITLE, AtlasRes::RES_256},
    "Some Text", 24, 1
);
```

### #5 — `DrawDefaultText(const char* text, int posX, int posY, int fontSize, Color color)`

Convenience function — signature sama persis kayak raylib `DrawText()`, tapi pake `FontId::DEFAULT` (Poppins-Bold).

```cpp
DrawDefaultText("Score: 100", 10, 10, 20, WHITE);
```

### #6 — `UnloadFonts()`

Unload semua font dari cache. Panggil pas game shutdown.

```cpp
UnloadFonts();
```

---

## 4. Panduan Pemakaian

### A. Mau nulis text simpel (gak peduli font)?

Pake `DrawDefaultText()` — signature sama kayak raylib lama, gak perlu edit banyak.

```cpp
// DULU:
DrawText("Hello", 10, 10, 20, WHITE);

// SEKARANG (sama aja):
DrawDefaultText("Hello", 10, 10, 20, WHITE);
```

### B. Mau pake font tertentu (header, keybind, dll)?

Pake `GetOrLoad()` + raylib `DrawTextEx()`.

```cpp
Font font = GetOrLoad(FontId::KEYBIND_HEADER);
DrawTextEx(font, "SETTINGS", {100, 50}, 28, 1, WHITE);
```

### C. Mau pake FontHandle (rapi, 1 parameter)?

Pake `DrawTextCached()` / `MeasureTextCached()`.

```cpp
DrawTextCached(
    {FontId::MEDIEVAL_SHARP, AtlasRes::RES_512},
    "Quest Complete!",
    {screenW/2, screenH/2},
    36, 1, GOLD
);
```

### D. Mau nambah font baru ke sistem?

**Step 1**: Taruh file `.ttf` / `.otf` di `assets/fonts/`.

**Step 2**: Tambah entry di enum `FontId` di `fonts.h`:

```cpp
enum class FontId : int {
    // ... existing entries ...
    MY_NEW_FONT,   // <-- tambah sini
    COUNT
};
```

**Step 3**: Tambah entry di `FONT_DEFS[]` di `fonts.cpp`:

```cpp
static const FontDef FONT_DEFS[(int)FontId::COUNT] = {
    // ... existing entries ...
    {"MyFont-Regular.ttf", "MyFont-Regular", AtlasRes::RES_256},   // MY_NEW_FONT
};
```

> **Urutan enum dan array HARUS sama.** `FONT_DEFS[(int)FontId::MY_NEW_FONT]` harus指向 entry yang bener.

### E. Mau resolusi atlas lebih tinggi?

Lewatin `AtlasRes` tambahan:

```cpp
// Default: RES_256
GetOrLoad(FontId::DEFAULT);

// Higher resolution for larger text:
GetOrLoad(FontId::DEFAULT, AtlasRes::RES_512);
```

Makin tinggi atlas resolution → makin tajam text ukuran besar, tapi makin berat memory.

---

## 5. Daftar Font Tersedia

| FontId | File | Default Res | Kegunaan |
| --- | --- | --- | --- |
| `DEFAULT` | Poppins-Bold.ttf | RES_256 | Font default UI |
| `KEYBIND_HEADER` | NewDawn.ttf | RES_256 | Header keybind list |
| `KEYBIND_ENTRY` | Poppins-Regular.ttf | RES_256 | Entry keybind list |
| `LOADING_TITLE` | Poppins-Bold.ttf | RES_256 | Alias DEFAULT buat loading screen |
| `MEDIEVAL_SHARP` | MedievalSharp-Regular.ttf | RES_256 | Font tematik medieval |
| `QUICKSAND_BOLD` | Quicksand-Bold.ttf | RES_256 | Bold sans-serif |
| `QUICKSAND_SEMIBOLD` | Quicksand-SemiBold.ttf | RES_256 | Semi-bold sans-serif |
| `QUICKSAND_MEDIUM` | Quicksand-Medium.ttf | RES_256 | Medium sans-serif |
| `QUICKSAND_REGULAR` | Quicksand-Regular.ttf | RES_256 | Regular sans-serif |
| `QUICKSAND_LIGHT` | Quicksand-Light.ttf | RES_256 | Light sans-serif |
| `NORSE_BOLD` | Norsebold.otf | RES_256 | Norse bold display |
| `NORSE` | Norse.otf | RES_256 | Norse regular display |

Semua file font ada di `assets/fonts/`.

---

## 6. Contoh Lengkap

### Contoh 1: Screen sederhana

```cpp
#include "fonts.h"

void DrawMyScreen() {
    // Header — pake LOADING_TITLE
    DrawTextCached(
        {FontId::LOADING_TITLE, AtlasRes::RES_256},
        "INVENTORY", {50, 30}, 28, 1, WHITE
    );

    // Body — pake DEFAULT
    DrawDefaultText("Item 1: Sword", 60, 80, 18, LIGHTGRAY);
    DrawDefaultText("Item 2: Shield", 60, 105, 18, LIGHTGRAY);

    // Footer — pake QUICKSAND_LIGHT
    Font light = GetOrLoad(FontId::QUICKSAND_LIGHT);
    DrawTextEx(light, "Press I to close", {50, 300}, 14, 1, GRAY);
}
```

### Contoh 2: Button component

```cpp
#include "fonts.h"
#include "button.h"

void DrawMenuButton() {
    // button.h otomatis pake FontId::DEFAULT kalo gak specify font
    buttonTxt("Play", 100, 200, 24, WHITE, 0.5f);

    // Bisa specify font lain
    Font medieval = GetOrLoad(FontId::MEDIEVAL_SHARP);
    buttonTxt("New Game", 100, 250, 24, WHITE, 0.5f, medieval);
}
```

### Contoh 3: Measure + center text

```cpp
#include "fonts.h"

void DrawCenteredText(const char* text, int y, int fontSize, Color color) {
    Vector2 size = MeasureTextCached(
        {FontId::LOADING_TITLE, AtlasRes::RES_256},
        text, (float)fontSize, 1
    );
    int x = (640 - (int)size.x) / 2;  // 640 = virtual screen width
    DrawTextCached(
        {FontId::LOADING_TITLE, AtlasRes::RES_256},
        text, {(float)x, (float)y}, (float)fontSize, 1, color
    );
}
```

---

## 7. Best Practices

| Situasi | Pake |
| --- | --- |
| Text biasa (gak perlu font khusus) | `DrawDefaultText()` |
| Satu screen pake font yang sama berulang | Simpen `Font font = GetOrLoad(FontId::X)` di awal fungsi |
| Cuma 1-2 kali draw | `DrawTextCached()` langsung |
| Mau ukur text dulu sebelum draw | `MeasureTextCached()` |
| Antar screen pake font beda | Masing-masing panggil `GetOrLoad()` sendiri (cache, jadi aman) |

### Performance Notes

- `GetOrLoad()` setelah font di-cache → O(1) hash lookup
- Atlas resolution naik → texture lebih gede → GPU memory lebih banyak
- Fallback ke `GetFontDefault()` kalo file gak ketemu atau 0 glyphs
- Texture filter selalu `BILINEAR` (smooth scaling)

### Kapan perlu atlas resolution lebih tinggi?

Kalo text kamu di `fontSize` di atas 40-50px dan keliatan pecah/pixelated, naikin `AtlasRes`:

```cpp
// fontSize 60 — butuh atlas tinggi biar tajam
GetOrLoad(FontId::DEFAULT, AtlasRes::RES_512);
```

---

## 8. Troubleshooting

### Font gak muncul / ke load?

Cek:

1. File font ada di `assets/fonts/`? (cek list di section 5)
2. Path `build/bin/assets/fonts/` juga perlu di-copy kalo build beda direktori
3. TraceLog bakal nampilin `"FONTS: %s not found..."` kalo gagal

### Font keliatan pecah?

Naikin atlas resolution:

```cpp
// Daripada RES_256, pake RES_512
GetOrLoad(FontId::X, AtlasRes::RES_512);
```

### Error: "loaded but has 0 glyphs"

Font file mungkin corrupt atau format gak didukung. Coba re-download atau ganti file.

### Mau nambah font?

Ikutin [Panduan D](#d-mau-nambah-font-baru-ke-sistem). Jangan lupa enum dan FONT_DEFS urutannya sama.
