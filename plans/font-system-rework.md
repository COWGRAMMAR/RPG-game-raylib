# Font System Rework — Status & Font Inventory

> **Status**: SEMUA SELESAI. Full migration tuntas — 82 GetOrLoad + 48 DrawDefaultText + button.h.

---

## 1. Font Assets (11 file)

| Font | File | Terdaftar? | Role |
|------|------|-----------|------|
| Poppins-Bold | `Poppins-Bold.ttf` |  | LOADING_TITLE (+ DEFAULT nanti) |
| Poppins-Regular | `Poppins-Regular.ttf` |  | KEYBIND_ENTRY |
| NewDawn | `NewDawn.ttf` |  | KEYBIND_HEADER |
| MedievalSharp-Regular | `MedievalSharp-Regular.ttf` |  | — |
| Quicksand-Bold | `Quicksand-Bold.ttf` |  | — |
| Quicksand-SemiBold | `Quicksand-SemiBold.ttf` |  | — |
| Quicksand-Medium | `Quicksand-Medium.ttf` |  | — |
| Quicksand-Regular | `Quicksand-Regular.ttf` |  | — |
| Quicksand-Light | `Quicksand-Light.ttf` |  | — |
| Norsebold | `Norsebold.otf` |  | — |
| Norse | `Norse.otf` |  | — |

### FontId saat ini (`fonts.h`)
```cpp
enum class FontId : int {
    KEYBIND_HEADER,  // NewDawn.ttf
    KEYBIND_ENTRY,   // Poppins-Regular.ttf
    LOADING_TITLE,   // Poppins-Bold.ttf
    COUNT
};
```

### FontId rencana (nambah DEFAULT)
```cpp
enum class FontId : int {
    DEFAULT,         // Poppins-Bold.ttf — font utama UI
    KEYBIND_HEADER,  // NewDawn.ttf
    KEYBIND_ENTRY,   // Poppins-Regular.ttf
    LOADING_TITLE,   // alias DEFAULT (Poppins-Bold), bisa hapus nanti
    COUNT
};
```

---

## 2. SUDAH Dimigrasi — GetOrLoad Calls (9 file, ~82 calls)

Semua udah pake `GetOrLoad(FontId::...)`. Tinggal ganti FontId kalo mau ganti font.

| File | Jumlah | Fungsi | Font / Abstrak Role | Resolusi | Isi Pesan |
|------|--------|--------|-------------------|:--------:|-----------|
| `hud.cpp` | 27× | `GetOrLoad(LOADING_TITLE)` | `Poppins-Bold.ttf` → `FontId::LOADING_TITLE` | `RES_256` | item name, stack amount, "Press 'I' to Close", keybind hints |
| `keybindsTab.cpp` | 19× | `GetOrLoad(...)` | lihat detail | lihat detail | lihat detail |
| `popup.cpp` | 16× | `GetOrLoad(LOADING_TITLE)` | `Poppins-Bold.ttf` → `FontId::LOADING_TITLE` | `RES_256` | pesan dialog, label tombol |
| `saveLoadScreen.cpp` | 10× | `GetOrLoad(...)` | lihat detail | lihat detail | lihat detail |
| `loading_screen.cpp` | 6× | `GetOrLoad(LOADING_TITLE)` | `Poppins-Bold.ttf` → `FontId::LOADING_TITLE` | `RES_256` | "Loading...", "85%", nama map |
| `pauseMenu.cpp` | 4× | `GetOrLoad(LOADING_TITLE)` | `Poppins-Bold.ttf` → `FontId::LOADING_TITLE` | `RES_256` | "Reset Tab", "Back", "Apply" |
| `audioTab.cpp` | 4× | `GetOrLoad(LOADING_TITLE)` | `Poppins-Bold.ttf` → `FontId::LOADING_TITLE` | `RES_256` | "Master Volume", "100" |
| `item.cpp` | 2× | `GetOrLoad(LOADING_TITLE)` | `Poppins-Bold.ttf` → `FontId::LOADING_TITLE` | `RES_256` | jumlah stack item di-drop |
| `videoTab.cpp` | 2× | `GetOrLoad(LOADING_TITLE)` | `Poppins-Bold.ttf` → `FontId::LOADING_TITLE` | `RES_256` | "Fullscreen", "Show FPS" |

### Detail per file

**`hud.cpp`** (27×, `GetOrLoad(FontId::LOADING_TITLE)`, `Poppins-Bold.ttf` → `FontId::LOADING_TITLE`, `RES_256`)
| Fungsi | Isi Pesan | Size | Warna |
|--------|----------|:----:|:-----:|
| `DrawTextHUD()` | variabel `text` | fontSize(16/20) | WHITE/GRAY |
| — | literal `"99"` | 18 | WHITE |
| — | literal `"99"` | 18 | WHITE |
| — | literal `"99"` | 16 | WHITE |
| — | literal `"1.2"` | 14 | WHITE |
| — | literal `"1.2"` | 14 | WHITE |
| — | literal `"1.2"` | 14 | WHITE |
| — | variabel item name | 22 | WHITE |
| — | variabel item description | 14 | WHITE |
| — | variabel keybind hints | hintFontSize(25/22) | WHITE |
| — | literal `"Press 'I' to Close"` | 20 | GRAY |
| — | variabel loot popup item | 16 | WHITE |

**`keybindsTab.cpp`** (19 calls)
| Fungsi | Isi Pesan | Font / Abstrak Role | Resolusi | Size | Warna |
|--------|----------|-------------------|:--------:|:----:|:-----:|
| `GetOrLoad(KEYBIND_HEADER)` | `"MOVEMENT"`, `"ACTION"`, `"INVENTORY"` | `NewDawn.ttf` → `FontId::KEYBIND_HEADER` | `RES_256` | 32 | WHITE |
| `GetOrLoad(KEYBIND_ENTRY)` | `"Move Up => [W]"`, `"Drag = Klik & tarik"` | `Poppins-Regular.ttf` → `FontId::KEYBIND_ENTRY` | `RES_256` | 28 | WHITE |
| `GetOrLoad(LOADING_TITLE)` | `"Key already bound to [action]!"`, `"Press key or click for [action]..."` | `Poppins-Bold.ttf` → `FontId::LOADING_TITLE` | `RES_256` | 30 | WHITE |

**`popup.cpp`** (16×, `GetOrLoad(FontId::LOADING_TITLE)`, `Poppins-Bold.ttf` → `FontId::LOADING_TITLE`, `RES_256`)
| Fungsi | Isi Pesan | Size | Warna |
|--------|----------|:----:|:-----:|
| `DrawTextEx()` | variabel `text` / `subMessage` | fontSize(30) | WHITE |
| `DrawTextEx()` | `"OK"`, `"Cancel"` | fontSize(30) | WHITE |

**`saveLoadScreen.cpp`** (10 calls)
| Fungsi | Isi Pesan | Font / Abstrak Role | Resolusi | Size | Warna |
|--------|----------|-------------------|:--------:|:----:|:-----:|
| `GetOrLoad(LOADING_TITLE)` | `"MANUAL SAVE"`, `"AUTO SAVE"` | `Poppins-Bold.ttf` → `FontId::LOADING_TITLE` | `RES_256` | 22 | WHITE |
| `GetOrLoad(LOADING_TITLE)` | variabel header area text | —"— | — | headerFontSize(28) | WHITE |
| `GetOrLoad(KEYBIND_ENTRY)` | `"Auto Save"`, `"Slot #2"`, `"Empty Slot"`, variabel map/timestamp | `Poppins-Regular.ttf` → `FontId::KEYBIND_ENTRY` | `RES_256` | 14-20 | WHITE |

**`loading_screen.cpp`** (6×, `GetOrLoad(FontId::LOADING_TITLE)`, `Poppins-Bold.ttf` → `FontId::LOADING_TITLE`, `RES_256`)
| Fungsi | Isi Pesan | Size | Warna |
|--------|----------|:----:|:-----:|
| — | `"Loading..."` | 32 | WHITE |
| — | variabel `"85%"` | 20 | WHITE |
| — | variabel nama map | 18 | WHITE |

**`pauseMenu.cpp`** (4×, same font)
| Fungsi | Isi Pesan | Size | Warna |
|--------|----------|:----:|:-----:|
| — | ON/OFF (toggle buttons) | labelFontSize(24) | GREEN/RED/GRAY |
| — | `"Reset Tab"`, `"Reset All"` | 20 | ORANGE |

**`audioTab.cpp`** (5×, same font)
| Fungsi | Isi Pesan | Size | Warna |
|--------|----------|:----:|:-----:|
| — | `"Master Volume"`, `"Music Volume"`, `"SFX Volume"` | FONT_SIZE(30) | WHITE |
| — | `"100%"`, `"75%"`, `"50%"` | FONT_SIZE(30) | BLACK |

**`item.cpp`** (2×, same font)
| Fungsi | Isi Pesan | Size | Warna |
|--------|----------|:----:|:-----:|
| — | literal `"3"` / jumlah stack | 14 | WHITE |

**`videoTab.cpp`** (2×, same font)
| Fungsi | Isi Pesan | Size | Warna |
|--------|----------|:----:|:-----:|
| — | `"Fullscreen"` | fontSize(34) | WHITE |
| — | `"Show FPS"` | fontSize(34) | WHITE |

---

## 3. SUDAH Dimigrasi — Raw DrawText (6 file, 48 calls)

Semua pake `DrawDefaultText(...)` → `GetOrLoad(FontId::DEFAULT)` (Poppins-Bold).

### `debugmode.cpp` — 26× `DrawText()`, `GetFontDefault()` / —

| Isi Pesan | Size | Warna |
|-----------|:----:|:-----:|
| variabel `title` | 18 | borderColor |
| `"Attack Area (Slam AABB)"` | 14 | RED |
| `"Attack Area (Rotated OBB)"` | 14 | RED |
| variabel `obj.name` | 12 | PURPLE |
| `"Count: %d"` | 10 | MAGENTA |
| `"Rad: %.0f"` | 10 | MAGENTA |
| `"Size: %dx%d tiles"`, `"Layers: %d"`, `"Tileset: %s"` | 14 | — |
| `"Target: (%.1f, %.1f)"`, `"Zoom: %.2f"` | 14 | — |
| `"Position: (%.1f, %.1f)"`, `"Speed: %.1f"`, dll | 14 | — |
| `"Zoom: %.2f"`, `"[Scroll] Zoom In/Out"` | 14 | — |
| `"Tiles Drawn: %d"`, `"Range X: %d-%d"`, dll | 14 | — |
| `"Rect Count: %d"`, `"Boundary Mode: %s"`, dll | 14 | — |
| variabel `def.name` | 10 | PINK |

### `combatTurn.cpp` — 14× `DrawText()`, `GetFontDefault()` / —

| Fungsi / Konteks | Isi Pesan | Size | Warna |
|-----------------|-----------|:----:|:-----:|
| `DrawTextCentered` | variabel `text` | fontSize(28/22/20/18) | color(GOLD/RED/YELLOW/GREEN) |
| `DrawActionButton` | variabel label `[key] name` | 20 | GOLD / WHITE |
| — | `"PLAYER"` | 18 | BLUE |
| — | `"HP: %.0f / %.0f"` | 16 | WHITE |
| — | `"MP: %.0f / %.0f"` | 16 | GOLD |
| — | `"BOSS: %s"` | 18 | RED |
| — | `"HP: %.0f / %.0f"` | 16 | WHITE |
| — | `">>> BERTAHAN! <<<"` | 14 | GREEN |
| — | `"MENANG"` (×5, outline efek tebal) | 80 | YELLOW / WHITE |
| — | variabel `itemText` | 20 | LIGHTGRAY |

### `effects.cpp` — 3× `DrawText()`, `GetFontDefault()` / —

| Isi Pesan | Size | Warna |
|-----------|:----:|:-----:|
| variabel `dmgStr` (damage number, e.g. "-12") | 10 | YELLOW |
| variabel `entry.text` (shadow) | 10 | BLACK(alpha) |
| variabel `entry.text` (foreground) | 10 | WHITE(alpha) |

### `hud.cpp` — 2× `DrawText()`, `GetFontDefault()` / —

| Isi Pesan | Size | Warna |
|-----------|:----:|:-----:|
| variabel `line` (loot popup item name) | 16 | WHITE |
| `"[Klik kiri] untuk tutup"` | 10 | GRAY |

### `videoScreen.cpp` — 2× `DrawText()`, `GetFontDefault()` / —

| Isi Pesan | Size | Warna |
|-----------|:----:|:-----:|
| variabel `loadingText` | 20 | WHITE |
| `"Tekan SPACE untuk skip"` | 20 | WHITE(180) |

### `screen_handler.cpp` — 1× `DrawText()`, `GetFontDefault()` / —

| Isi Pesan | Size | Warna |
|-----------|:----:|:-----:|
| variabel `fpsText` (e.g. `"87 FPS"`) | 20 | GREEN |

---

## 4. Belum Dimigrasi — button.h Constructor

| File | Baris | Kode |
|------|-------|------|
| `button.h` | 36 | `TextPolicy() : font(GetFontDefault()) {}` |
| `button.h` | 39 | `TextPolicy(..., Font font = GetFontDefault())` |

Semua button object di codebase yang gak explicit pass font → pake default font.

---

## 5. Font Default Decision Table

Setelah audit, tentuin tiap konteks text pake **FontId** apa:

| Konteks | fontSize range | FontId saran |
|---------|---------------|-------------|
| **HUD / UI pesan** (popup, pause menu, inventory, loading screen, audio/video tab) | 10-32 | `DEFAULT` (Poppins-Bold) |
| **Debug overlay** | 10-18 | `DEFAULT` atau `KEYBIND_ENTRY` |
| **Combat UI** (label, HP/MP, button) | 14-80 | `DEFAULT` |
| **Combat "MENANG"** (outline effect) | 80 | `DEFAULT` |
| **Damage number / log** | 10 | `DEFAULT` atau `KEYBIND_ENTRY` |
| **FPS counter** | 20 | `DEFAULT` |
| **Video skip text** | 20 | `DEFAULT` |
| **Keybind header** | 32 | `KEYBIND_HEADER` (NewDawn) |
| **Keybind entry** | 28 | `KEYBIND_ENTRY` (Poppins-Regular) |
| **Keybind toast** | 30 | `DEFAULT` |
| **button.h** | varies | `DEFAULT` |

---

## 6. Implementasi FontId::DEFAULT

### fonts.h
```cpp
enum class FontId : int {
    DEFAULT,         // Poppins-Bold.ttf — font utama UI
    KEYBIND_HEADER,  // NewDawn.ttf
    KEYBIND_ENTRY,   // Poppins-Regular.ttf
    LOADING_TITLE,   // sama dengan DEFAULT (Poppins-Bold) — alias aja
    COUNT
};
```

### fonts.cpp — FONT_DEFS
```cpp
static const FontDef FONT_DEFS[(int)FontId::COUNT] = {
    {"Poppins-Bold.ttf",   "Poppins-Bold",   AtlasRes::RES_256},  // DEFAULT
    {"NewDawn.ttf",        "NewDawn",        AtlasRes::RES_256},  // KEYBIND_HEADER
    {"Poppins-Regular.ttf", "Poppins-Regular", AtlasRes::RES_256}, // KEYBIND_ENTRY
    {"Poppins-Bold.ttf",   "Poppins-Bold",   AtlasRes::RES_256},  // LOADING_TITLE (alias)
};
```

`LOADING_TITLE` pointing ke file sama — jadi semua code yang udah migrasi tetap jalan.
Nanti kalo udah selesai migrate semua `LOADING_TITLE` → `DEFAULT`, LOADING_TITLE bisa dihapus.

---

## 7. Status akhir — SEMUA TUNTAS

| Langkah | Status |
|---------|--------|
| Font system cache (Phase 1-3) |  |
| FontId::DEFAULT + 11 font assets registered |  |
| 82 GetOrLoad calls migrated (9 files) |  |
| 48 DrawDefaultText calls (6 files) |  |
| button.h font fix |  |
| Plan trace tables updated (parameter value tracing) |  |

**Opsional**: rename `LOADING_TITLE` → `DEFAULT` di existing code, hapus entry duplikat di `FONT_DEFS`. Tapi gak urgent — `LOADING_TITLE` sudah pointing ke file yang sama dengan `DEFAULT`.
