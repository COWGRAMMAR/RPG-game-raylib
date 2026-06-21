# Plan: Minimap Bugfixes

## Bug 1 — Player gak bisa gerak pas map open
**File:** `src/core/main.cpp`
**Linenya:** 315
**Akar masalah:** `!g_MinimapScreen.IsActive()` di guard `UpdateLogicAll()` — pas minimap aktif, semua logic (termasuk gerak player) diblokir.
**Fix:** Hapus `!g_MinimapScreen.IsActive()` dari guard. Player harusnya bisa gerak bebas pas map open, sama kayak inventory.
**Resiko:** Minimal — inventory juga gak di-guard, jadi ini konsisten.

## Bug 2 — Pause + minimap mutual exclusion
**File:** `src/core/main.cpp` & `MinimapScreen`
**Akar masalah:** Pas minimap aktif + tekanan Esc, pause menu kebuka di belakang minimap. Dua-duanya aktif barengan.
**Fix:** Mutual exclusion — pas `pauseMenu.Show()` → `g_MinimapScreen.Hide()`. Pas `g_MinimapScreen.Show()` → `pauseMenu.Hide()`.

## Bug 3 — Minimap state leak (kayak inventory #20)
**File:** `src/systems/input.cpp`
**Akar masalah:** `ResetMenuFlags()` cuma reset `InventoryOpen`/`MapOpen` di PlayerInput, gak sentuh `g_MinimapScreen`. Kalo minimap kebuka pas balik ke main menu, state-nya nempel.
**Fix:** Di `PlayerInput::ResetMenuFlags()` tambah `g_MinimapScreen.Shutdown()` — include `map/minimap.h` di input.cpp.

## Bug 4+6 — Map terlalu kecil & scale 1:1
**File:** `include/map/minimap.h`
**Akar masalah:** `MINIMAP_TILE_TO_PX = 2` → 2×2 tile jadi 1 px. Grid setengah ukuran asli.
**Fix:** `MINIMAP_TILE_TO_PX = 1` → 1 tile = 1 px.
**Dampak:** `BuildGridTexture()` sampling 1 tile per grid cell (gak perlu loop MINIMAP_TILE_TO_PX lagi). GridWidth = mapWidth. Tampilan 2× lebih besar.
**Ukuran hasil:** main_hub 30×30 → 30×30 grid × PANEL_SCALE 3 = 90×90 px. Worldgen 164×164 → 492×492 px (muat di 1280×720).

## Bug 5 — Warna testing
**File:** `src/map/minimap.cpp`
**Akar masalah:** `MapGidToColor()` pake warna brown/gelap — susah dibedain.
**Fix:** Green background untuk floor, gray untuk obstacle/wall.

## Execution Plan
1. `include/map/minimap.h` — `MINIMAP_TILE_TO_PX` → 1
2. `src/map/minimap.cpp` — `MapGidToColor()` green/grays; `BuildGridTexture()` simplify sample (1 tile per cell, gak perlu loop 2×2)
3. `src/core/main.cpp` — hapus guard + mutual exclusion pause⇄minimap
4. `src/systems/input.cpp` — `ResetMenuFlags()` + `g_MinimapScreen.Shutdown()`
5. `src/ui/pauseMenu.cpp` — tambah mutual exclusion pas pause toggled
