# Issue #15 — Inventory State Leak ke New Game

## Tipe
Bug
**Status**: Selesai (12 Jun 2026) — ResetMenuFlags() di pauseMenu, gameOverScreen, interaction, worldgenio

## Deskripsi
Player buka inventory → buka pause menu → balik ke main menu → start new game → inventory udah kebuka duluan (state gak di-reset).

## Root Cause
`InputInstance.InventoryOpen` gak pernah di-reset pas balik ke main menu.

### Alur:
1. Player tekan Tab → `InputInstance.UpdateState()` toggle `InventoryOpen = true` (input.cpp:69)
2. Player tekan Escape → pause menu muncul, `InventoryOpen` tetap `true`
3. Player klik "Return to Main Menu" → `pauseMenu.Hide()` cuma set `active = false` (pauseMenu.cpp:493)
4. `state->currentScreen = MAIN_MENU` (pauseMenu.cpp:622)
5. Player klik "New Game" → loading screen → `state->currentScreen = PLAY`
6. Game loop render inventory karena `InputInstance.IsInventoryOpen()` masih `true`

### State Variables Bermasalah
```cpp
// input.h:140-141
bool InventoryOpen = false;  // ← gak di-reset
bool MapOpen = false;        // ← mungkin juga
```

Satu-satunya tempat `InventoryOpen = false` di-set:
- `input.cpp:69` — toggle `!InventoryOpen` (kebalikannya aja)
- `input.cpp:80` — cuma kalo MapOpen toggled

### Fix
Reset `InventoryOpen = false` (dan `MapOpen = false` untuk safety) pas balik ke MAIN_MENU. Bisa di:
- `PauseMenu::HandleButtonClick()` saat klik "Return to Main Menu"
- Atau di `main.cpp` game loop saat deteksi transisi ke MAIN_MENU

## Prioritas
Sedang
