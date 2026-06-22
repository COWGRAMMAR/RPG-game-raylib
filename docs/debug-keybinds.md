# Debug Keybinds

Dokumen ini mencatat semua keybind di Breach & Loot, termasuk yang tidak muncul di UI Settings > Keybinds. Ada dua kategori: action enum yang bisa di-rebind (tapi beberapa disembunyikan dari UI), dan debug toggle yang hardcoded di luar keybind manager.

## Daftar Lengkap Action Enum

Tabel berikut adalah semua nilai dalam `enum Action` di `keybindManager.h`. Kolom Section menunjukkan letaknya di UI settings (`sections[]` di `keybindsTab.cpp`).

| Index | Action | Default Key | Section (keybindsTab) |
|-------|--------|-------------|----------------------|
| 0 | MOVE_UP | W | MOVEMENT |
| 1 | MOVE_DOWN | S | MOVEMENT |
| 2 | MOVE_LEFT | A | MOVEMENT |
| 3 | MOVE_RIGHT | D | MOVEMENT |
| 4 | INTERACT | E | ACTION |
| 5 | TOGGLE_INVENTORY | I | INVENTORY |
| 6 | TOGGLE_MAP | M | ACTION |
| 7 | DROP_ITEM | Q | ACTION |
| 8 | DROP_ALL | Left Ctrl | ACTION |
| 9 | ATTACK_DRINK | Mouse Left | ACTION |
| 10 | DASH | Mouse Right | ACTION |
| 11 | HOTBAR_SLOT_1 | 1 | INVENTORY |
| 12 | HOTBAR_SLOT_2 | 2 | INVENTORY |
| 13 | HOTBAR_SLOT_3 | 3 | INVENTORY |
| 14 | HOTBAR_SLOT_4 | 4 | INVENTORY |
| 15 | PAUSE_MENU | Escape | -- (tidak ada di UI) |
| 16 | TOGGLE_FULLSCREEN | F11 | -- (tidak ada di UI) |
| 17 | ACTION_COUNT | -- (sentinel) | -- |

Semua action dari index 0 sampai 14 muncul di UI settings lewat struct `sections[]` di `keybindsTab.cpp`. Action 15 (PAUSE_MENU) dan 16 (TOGGLE_FULLSCREEN) sengaja tidak dimasukkan ke section manapun.

## PAUSE_MENU -- Bisa Di-rebind, Tapi Tidak Lewat UI

PAUSE_MENU (index 15) berbeda dari yang dideskripsikan di dokumen lama. Sekarang ia adalah action biasa di enum:

- Default key: `KEY_ESCAPE`, diset di `keybindManager.cpp` lewat `InitDefaults()`.
- **Tidak hardcoded.** `input.cpp` baris 56 membaca dari keybindManager: `Current.pauseMenu = IsKeyPressed(keybindManager.GetKeycode(PAUSE_MENU))`.
- Hasilnya dicek di `main.cpp` baris 271: `if (InputInstance.GetState().pauseMenu)` untuk toggle pause menu.
- Karena tidak ada di `sections[]` di `keybindsTab.cpp`, action ini tidak bisa di-rebind lewat UI Settings.
- Tapi karena datanya tetap disimpan ke `saves/settings/keybindsTab.json`, kamu bisa mengubah binding-nya secara manual di file JSON.

Satu-satunya fungsi Escape yang benar-benar hardcoded adalah di `keybindsTab.cpp` baris 167, sebagai cancel key di rebind listener. Ini tidak terkait dengan PAUSE_MENU.

Jika suatu saat ingin mengekspos PAUSE_MENU atau TOGGLE_FULLSCREEN ke UI, caranya dengan menambahkan index-nya ke array `sections[]` di `keybindsTab.cpp`.

## Debug Toggle -- Hardcoded, Bukan Action Enum

Ini adalah bagian yang paling banyak berubah dari dokumen lama. Tidak ada action khusus debug di enum Action. Semua fungsi debug di-handle oleh `Debug::Toggle()` di `debugmode.cpp` dengan input yang hardcoded.

### Aktifkan Debug Mode

**LCtrl + LShift + \** (backslash)

Kombinasi ini men toggle `isDebugMode` di `Debug::Toggle()` (baris 276). Ketiga tombol harus ditekan bersamaan. Tidak ada action di keybindManager untuk ini.

### World Overlay (saat Debug Mode ON)

`Debug::DrawWorldOverlay()` (baris 489) merender overlay di world space:

| Elemen | Warna | File:Baris |
|--------|-------|------------|
| Player hitbox | LIME (hijau) | debugmode.cpp:503 |
| Player hurtbox | YELLOW (kuning) | debugmode.cpp:504 |
| Magnet radius | GOLD (garis lingkaran) | debugmode.cpp:508 |
| Hitbox corner points | LIME (lingkaran) | debugmode.cpp:511-514 |
| Collision layer | RED | debugmode.cpp:517 |
| Object layer | SKYBLUE | debugmode.cpp:518 |
| Trap layer | BEIGE | debugmode.cpp:519 |
| Item layer | PINK | debugmode.cpp:520 |
| Exit layer | BLACK | debugmode.cpp:521 |
| Attack area | RED (filled + outline) | debugmode.cpp:522 |
| Map bounds | GREEN (rectangle) | debugmode.cpp:548 |
| Item hitboxes | PINK (outline + label) | debugmode.cpp:551-558 |

Player hurtbox overlay (YELLOW) ditambahkan di PR #82 (akbarazy -- enemy hitbox/hurtbox combat rework).

### Sub-toggle (hanya berfungsi saat debug mode ON)

| Tombol | Fungsi | Variable | File:Baris |
|--------|--------|----------|------------|
| `]` (right bracket) | Toggle overlay flow field enemy | `showFlowFieldOverlay` | debugmode.cpp:282 |
| `[` (left bracket) | Toggle overlay flow field player | `showFlowFieldOverlayPlayer` | debugmode.cpp:287 |
| `K` | Kill player (HP langsung 0) | `PlayerInstance.Health = 0` | debugmode.cpp:292 |

Semua input di atas hardcoded langsung di `Debug::Toggle()`, menggunakan `IsKeyPressed()` raylib, bukan lewat keybindManager. Tidak ada cara untuk mengubahnya tanpa mengedit source code.

## Kenapa Debug Toggle Tidak Ada di UI Settings?

Dulu versi lama Action enum punya entry REVIVE, TEST_LOSE_HP, GO_BACK, DEBUG_TOGGLE, DEBUG_TOGGLE_ENEMY, DEBUG_TOGGLE_PLAYER dengan index 15-21. Semua itu sudah dihapus saat Action enum dibersihkan. Sekarang debug system berdiri sendiri di `debugmode.cpp`, terpisah dari keybindManager.

Akibatnya:
- Tidak bisa di-rebind sama sekali (kecuali edit source code).
- Tidak muncul di UI Settings.
- Tidak tersimpan di `keybindsTab.json`.

## Sections Coverage di keybindsTab.cpp

Struktur `sections[]` di `keybindsTab.cpp`:

```cpp
static const int movementIndices[] = {0, 1, 2, 3};         // MOVE_UP, DOWN, LEFT, RIGHT
static const int actionIndices[] = {9, 10, 4, 7, 8, 6};    // ATTACK_DRINK, DASH, INTERACT, DROP_ITEM, DROP_ALL, TOGGLE_MAP
static const int inventoryIndices[] = {5, 11, 12, 13, 14}; // TOGGLE_INVENTORY, HOTBAR_SLOT_1,2,3,4
```

- **MOVEMENT**: mencakup 4 action (0-3).
- **ACTION**: mencakup 6 action (4, 6, 7, 8, 9, 10) -- gabungan combat dan TOGGLE_MAP.
- **INVENTORY**: mencakup 5 action (5, 11, 12, 13, 14) -- TOGGLE_INVENTORY dan 4 hotbar slot.
- **Tidak tercakup**: PAUSE_MENU (15), TOGGLE_FULLSCREEN (16), dan ACTION_COUNT (17).

Ada juga 3 entry info (non-rebindable) di section INVENTORY: Drag Item, Split Item, Merge Item.

## File Sumber Terkait

| File | Peran |
|------|-------|
| `include/systems/keybindManager.h` | Definisi enum `Action` (MOVE_UP=0 ... ACTION_COUNT=17) dan class KeybindManager |
| `src/systems/keybindManager.cpp` | Default bindings (`InitDefaults()`), action names, JSON persistence ke `saves/settings/keybindsTab.json` |
| `include/systems/input.h` | Struct `InputState` -- field `pauseMenu` dan semua flag input |
| `src/systems/input.cpp` | Polling PAUSE_MENU dari keybindManager (baris 56), mapping key action ke InputState |
| `include/debug/game_debug.h` | Deklarasi `Debug::Toggle()`, `isDebugMode`, `showFlowFieldOverlay`, `showFlowFieldOverlayPlayer` |
| `src/debug/debugmode.cpp` | Implementasi semua debug toggle keys (baris 271-297) |
| `src/ui/keybindsTab.cpp` | Struct `sections[]` hanya mencakup index 0-14; rebind listener dengan hardcoded Escape cancel |
| `src/core/main.cpp` | Pengecekan PAUSE_MENU (baris 271) dan TOGGLE_FULLSCREEN (baris 168) di game loop |
| `src/core/screen_handler.cpp` | Panggilan `DebugInstance.Toggle()` dan `DebugInstance.Draw()` di rendering (baris 416-417) |

## Catatan

- Debug toggle (LCtrl+LShift+\) menggunakan kombinasi keyboard, bukan mouse.
- Keybind normal disimpan ke `saves/settings/keybindsTab.json`. Mengedit file JSON secara manual bisa mengubah binding PAUSE_MENU atau TOGGLE_FULLSCREEN meskipun tidak ada di UI.
- Reset Defaults di UI Settings > Keybinds mereset semua keybind ke nilai default dari `InitDefaults()`.
- PAUSE_MENU dan TOGGLE_FULLSCREEN ikut di-reset karena ada di enum `Action` dan di-loop oleh `InitDefaults()`.
- Debug toggle hardcoded tidak terpengaruh oleh Reset Defaults.
