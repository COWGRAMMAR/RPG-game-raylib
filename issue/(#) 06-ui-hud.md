# UI / HUD — #1, #9, #14, #15, #17 + Reset Buttons + Scroll Indicator

## #1 — Health Bar Enemy Muncul Pas Kena Damage

### Masalah
Health bar enemy cuma muncul pas enemy dalam state chase/attack (line 678 `enemy.cpp`):
```cpp
if (AIState == ENEMY_CHASE || AIState == ENEMY_ATTACK)
{
    // render health bar
}
```
Kalo enemy masih idle/patrol terus kena damage, health bar gak keliatan.

### Expected
Health bar muncul pas enemy **kena damage** — trigger di `TakeDamage()`.

### Fix Proposal
1. Tambah field `HealthBarTimer` di `Enemy` class (default 0)
2. di `Enemy::TakeDamage()`, set `HealthBarTimer = 2.0f` (atau durasi berapa detik)
3. di `Enemy::Render()`, ganti kondisi jadi:
   ```cpp
   if (HealthBarTimer > 0 || AIState == ENEMY_CHASE || AIState == ENEMY_ATTACK)
   ```
4. di `Enemy::Update()`, countdown `HealthBarTimer -= dt`

### File yang diubah
| File | Perubahan |
|------|-----------|
| `include/entities/enemy.h` | Tambah `HealthBarTimer` field |
| `src/entities/enemies/enemy.cpp` | Set timer di `TakeDamage()`, ganti kondisi render |

---

## (BARU) Boss — Health Bar Besar di Layar Utama

### Masalah
Boss sekarang diperlakuin sama persis kayak enemy biasa — health bar kecil di atas kepala.

### Expected
Boss punya **health bar besar di tengah bawah layar utama** (separate dari enemy health bar biasa), muncul pas player masuk detection radius boss, ilang kalo player keluar radius.

### Cara Kerja
1. Di `Enemy::Render()` atau sistem HUD, deteksi: "apakah ada enemy dengan `rank == ENEMY_BOSS` yang player-nya masuk `DetectionRange`?"
2. Kalo iya → render boss health bar di layar utama:
   - Background panel lebar di bagian bawah tengah
   - Nama boss di atas bar
   - Health bar besar (panjang) dengan warna beda (misal orange/gold)
   - Mungkin tambah icon/dekorasi biar "boss vibe"
3. Kalo player keluar detection radius → health bar ilang

### Logic Deteksi
Bisa pake `CheckPlayerLoS()` atau cukup `CheckCollisionCircleRec(player, DetectionRange, boss)` — udah ada di `enemy.cpp:372`.

### File yang mungkin diubah
- `src/rendering/hud.cpp` — render boss health bar di main HUD
- `src/entities/enemies/enemy.cpp` — expose detection status buat HUD

### Catatan
- Boss detection range beda sama enemy biasa — bisa lebih besar
- Health bar boss gak perlu timer (gak ilang selama player masih dalam radius)
- Vibe: kayak game Dark Souls / Monster Hunter — health bar besar pas lawan boss

---

## #9 — HUD Player — Redesain Legend & Icon System

### Masalah
Legend keybind di pojok kanan atas (`hud.cpp:981-1028`) masih pake text doang:
- `[E] Interact`, `[I] Inventory`, `[Q] Drop Item`, `[Shift+Q] Drop All`, `[Esc] Pause`, `[Scroll] Switch Item`
- Gak ada icon visual
- Interact nempel permanen, gak kontekstual
- Keybind text hardcoded, gak dinamis
- Scroll hint gak perlu (hotbar numbers udah cukup)

### Rencana Final (2026-06-14)

#### Layout

```
┌──────────┐  FPS: 60              [Kill: 3/10]
│ [Esc]    │
│  Pause   │
└──────────┘

                    ┌──────────┐
                    │ [E] Buka │    ← conditional (PlayerInstance.canInteract)
                    └──────────┘

┌────┐  ┌────┐ ┌────┐ ┌────┐ ┌────┐  ┌──────────┐
│[I] │  │    │ │    │ │    │ │    │  │ [Q] Drop  │
│Inv │  └────┘ └────┘ └────┘ └────┘  │ [S+Q] All │
└────┘    1      2      3      4      └──────────┘
```

#### Detail per Elemen

| Posisi | Elemen | Font | Perilaku |
|--------|--------|------|----------|
| **Pojok kiri atas** (x=10, y=30) | [Esc] Pause — text-based box | FontId::HUD_HINT | Selalu muncul. Keybind dinamis |
| **Kanan [Esc]** (x=~100, y=30) | FPS counter | font system existing (FPS=14) | Tetap pake yang existing |
| **Pojok kanan atas** | `Kill: X/Y` — kill counter | FontId::HUD_HINT | X = enemies mati, Y = total di current map. Hitung dari `Entities::EnemyRegistry` (count !IsActive). **Zero save data changes.** |
| **Tengah layar** (y=~130) | `[E] Buka` — interact prompt | FontId::HUD_HINT | Hanya render kalo `PlayerInstance.canInteract == true`. Keybind dinamis. |
| **Bottom-center kiri hotbar** (y=~265) | `[I]` text-based placeholder | FontId::HUD_HINT | Placeholder kotak persegi, nanti diganti icon. Keybind dinamis. |
| **Bottom-center kanan hotbar** (y=~265) | `[Q] Drop` + `[Shift+Q] All` stacked | FontId::HUD_HINT | Dua baris, keybind dinamis. DropAll pake combo display. |
| **Bawah tiap slot hotbar** (y=~335) | Angka `1` `2` `3` `4` | FontId::HUD_HINT | Tengah-tengah bawah slot masing-masing. |

#### Yang Didrop dari Legend Lama
- `[E] Interact` → pindah ke tengah layar (kontekstual)
- `[I] Inventory` → pindah ke bottom-center kiri hotbar
- `[Q] Drop Item` / `[Shift+Q] Drop All` → pindah ke bottom-center kanan hotbar
- `[Esc] Pause` → pindah ke kiri atas
- `[Scroll] Switch Item` / Scroll hint → dropped entirely
- Semua teks legend lama (lines ~981-1028)

#### Kill Counter — Data Source
- **Tidak** make variable baru / save data field
- Hitung langsung dari `Entities::GetEnemyRegistry()`:
  - Total = `registry.size()`
  - Dead = count `!enemy->IsActive`
- Zero perubahan di save system. Reset otomatis tiap ganti map.

#### Font System
- 1 font baru: `FontId::HUD_HINT` → Quicksand-SemiBold.ttf, AtlasRes::RES_256
- Daftar di `fonts.h` enum + `fonts.cpp` FontDef array

#### Keybind Dinamis
- SEMUA text keybind pake `keybindManager.GetKeyDisplayName(Action)`
- Otomatis update pas user ganti keybind di settings
- DropAll: combo display `[Shift] + [Q]` (modifier + main key)

#### Asset / Icon
- **Belum ada icon PNG** — semua pake text-based placeholder
- Inventory: kotak doang, nanti ganti icon
- Pause: text box `[Esc] Pause`, nanti ganti icon

### Execution Plan

#### Step 1: Font + Kill Counter Infrastructure
1. `include/core/fonts.h` — tambah `HUD_HINT` ke enum FontId
2. `src/core/fonts.cpp` — tambah entry FontDef untuk HUD_HINT (Quicksand-SemiBold.ttf, RES_256, "mainHUDPlayer")
3. No save data changes — kill count computed from EnemyRegistry

#### Step 2: HUD Render Ulang (hud.cpp)
1. Hapus legend block (lines ~981-1028)
2. Render 5 elemen baru:
   - Pause icon + keybind di (10, 30)
   - Interact prompt di tengah (conditional)
   - Inventory placeholder di bottom-left of hotbar
   - Drop / DropAll di bottom-right of hotbar
   - Kill counter di top-right
3. Hotbar: render angka 1-4 di bawah tiap slot
4. FPS counter tetap di posisi existing (kanan [Esc])

### File yang Diubah
| File | Perubahan |
|------|-----------|
| `include/core/fonts.h` | Tambah `HUD_HINT` ke enum FontId |
| `src/core/fonts.cpp` | Tambah FontDef entry (Quicksand-SemiBold, RES_256) |
| `src/rendering/hud.cpp` | Hapus legend lama, render 5 elemen + kill counter + hotbar numbers |
| `include/rendering/hud.h` | Tidak ada perubahan (fungsi existing cukup) |

---

## #14 — Daftar Keybind — Redesign Layout & Grouping

### Masalah
Belum ada UI yang nampilin daftar keybind lengkap ke player dengan grouping yang jelas.

### Expected
Screen/tab yang nampilin semua action → key yang terdaftar, biar player tau kontrolnya, dikelompokin per kategori.

### Group Mapping Final

| Group | Items | Count |
|-------|-------|-------|
| **Movement** | Move Up, Move Down, Move Left, Move Right | 4 |
| **Action** | Attack, Drink, Interact, Drop, Drop All, Dash, Map | 7 |
| **Inventory** | Inventory Toggle, Slot 1, Slot 2, Slot 3, Slot 4, Merge, Split | 8 |

#### Detail & Keputusan

| Item | Status | Catatan |
|------|--------|---------|
| GO_BACK | **Hidden** | Internal only, gak ditampilin |
| EQUIP_UNEQUIP | **Dropped** | Gak dipake (cuma log) |
| Debug group | **Hidden** | Gak muncul di UI manapun |
| Merge & Split | **Tetep di list** | Informational — mouse-driven (Ctrl+Click, Right-Click Drag) |
| Drop All | **Combo display** | Tampil sebagai `[Shift] + [Q]` (modifier + main key) |
| Hardcoded Raylib input | **Leave as-is** | Gak perlu dipindah ke custom keybind system |
| ResolveAction (Attack/Drink via slot+click) | **Leave as-is** | Gak perlu ditampilin sebagai keybind |

### Layout & Font

- **Group headers**: `fontKeybindHeader` (NewDawn)
- **Entry items**: `fontKeybindEntry` (Poppins-Regular)
- Tiap group dipisah pjarak vertikal
- Entry format: `Action Name — [Key]`

### File yang Diubah
| File | Perubahan |
|------|-----------|
| `src/ui/keybindsTab.cpp` | Implementasi layout grouping + font baru |

---

## #15 — UI Inventory

### Masalah
Legenda inventory sekarang pake teks instruksi di kanan atas (4 hint: Arrange, Merge, Split, Drop). Kurang intuitif dan gak dinamis.

### Legend Sekarang
File: `src/rendering/hud.cpp:553-572`

```
[Left-Click Drag] Arrange    → 🗑️ DROP
[Ctrl+Click] Merge           → ✅ TETAP, dynamic keybind
[Right-Click Drag] Split     → ✅ TETAP, dynamic keybind
[Drop Outside Menu] Drop     → 🗑️ DROP
```

### Rencana Baru

#### 1. Drop Teks Instruksi
- "Arrange" dan "Drop Outside Menu" — di-drop
- Ganti dengan **visual feedback** pas drag item

#### 2. Visual Drop Feedback
**Normal drag** (cursor di atas grid):
- Slot sumber → GOLD tint (`ColorAlpha(GOLD, 0.25f)`)
- Ghost item putih

**Cursor di luar grid** (drop zone):
- Ghost item 36×36 box → **red background fill** + **red outline** (`DrawRectangleLines`)
- Sprite item di tint merah atau tetap putih di atas bg merah
- Slot sumber di grid → **RED tint** ganti GOLD + red outline

#### 3. Merge & Split — Dynamic Keybind
- Merge & Split tetep di legend, tapi pake dynamic keybind text (#9 style)
- Ditaruh di **bawah** area legenda (bukan kanan atas)

#### 4. "Press I to Close" — Dibesarin + Dynamic
- Line 547-550 `hud.cpp`: font dibesarin, ganti font (kemungkinan `fontKeybindEntry`)
- Teks jadi dinamis — ngikut keybind yang ke-*assign*

### File yang Diubah
| File | Perubahan |
|------|-----------|
| `src/rendering/hud.cpp:340-350` | Visual ghost item: red bg + outline pas di drop zone |
| `src/rendering/hud.cpp:420-425` | Slot sumber: red tint ganti gold pas drop zone |
| `src/rendering/hud.cpp:547-551` | "Press I to Close": font bigger, dynamic keybind |
| `src/rendering/hud.cpp:553-572` | Legend: drop Arrange/Drop, Merge/Split dynamic |

---

## #17 — Tombol Show FPS Off Tapi Counter Nyala

### Masalah
1. Run 1: Set Show FPS ON → counter muncul
2. End run, mulai Run 2 (dari menu, gak restar program)
3. Buka settings → button nunjukin "OFF"
4. Tapi counter FPS **tetap muncul** di layar

### Root Cause
**Mismatch antara state global dan variabel lokal di OptionsScreen.**

Aliran data:
| Layer | Variable | Sumber |
|-------|----------|--------|
| Render FPS (`screen_handler.cpp:424`) | `state->showFPS` | **Global GameState** |
| Button UI (`pauseMenu.cpp:194`) | local `showFPS` | **Member OptionsScreen** |

**Flow:**
1. `InitScreen()` (line 175): `state.showFPS = false`
2. `LoadVideoSettings()` (line 128): muat dari JSON → `state.showFPS` jadi `true` (kalo pernah ON)
3. Program gak restart antar run — `state->showFPS` tetap `true`
4. `OptionsScreen::Show()` (line 76-88) **tidak sync** local `showFPS` dari `state`
5. Constructor (line 59) set local `showFPS = false` — dan gak pernah diupdate sampai user ngeklik button
6. Button render pake local `showFPS` → nampilin "OFF"
7. Render FPS counter pake `state->showFPS` → tetap muncul

### Fix
Di `OptionsScreen::Show()`, sync local `showFPS` dari state sebelum ngerender button:

```cpp
void OptionsScreen::Show()
{
    active = true;
    showFPS = state->showFPS;  // sync dari global state
    // ... sisanya
}
```

Atau bisa juga di `DrawVideoTab` — pastiin local variable selalu nyala dari `state->showFPS`.

### File yang Diubah
| File | Perubahan |
|------|-----------|
| `src/ui/pauseMenu.cpp` | `Show()`: tambah param `GameState* state` + sync `showFPS = state->showFPS` |
| `include/ui/pauseMenu.h` | `Show()` signature: tambah `GameState* state` |
| `src/core/main.cpp` | Caller `Show()`: pass `&state` |

### Status:  Fixed di commit `fix ui #17`
- Local `showFPS` di-sync dari `state->showFPS` setiap kali options dibuka

---

## New: Reset Tab / Reset All Buttons — Ganti Look

### Masalah
Tombol "Reset Tab" dan "Reset All" di options screen pake tampilan default/basic.

### Expected
Desain ulang button — font, warna, styling yang lebih cocok sama tema game.

### Catatan
Font udah siap:

| Variable | Font | File |
|----------|------|------|
| `fontKeybindEntry` | Poppins-Regular | `include/core/fonts.h` |
| `fontKeybindHeader` | NewDawn | `include/core/fonts.h` |
| `fontLoadingTitle` | Poppins-Bold | `include/core/fonts.h` |

### File yang mungkin diubah
- `src/ui/keybindsTab.cpp` — mungkin ada di sini atau di pause menu component

---

## (FIXED) Keybinds Scroll Indicator — Teks ke Visual

### Status:  Fixed 2026-06-16
- GenImageColor procedural thumb → scrollBar.png asset + SetTextureFilter POINT
- scrollBG.png buat background scroll (gray rect)
- Thumb idle tint {110,110,110}, hover/drag WHITE
- Font: KEYBIND_ENTRY → Quicksand-Bold, dark brown KEYB_LABEL_COLOR
- Toast + popup font → KEYBIND_ENTRY (Quicksand-Bold)
- Hover selection: rounded rect 0.25f, alpha {138,135,128,100}
- Popup centered relatif content area (contentW, CONTENT_TOP, CONTENT_H)
- Listening green bg tetep, key color selalu dark brown
- SAVESLOT_TEXT font untuk saveLoadScreen

---

## Ringkasan File

| # | Tipe | Deskripsi | File Terkait |
|---|------|-----------|--------------|
| 1 | Revisi | Health bar enemy pas kena damage | `enemyRenderer.cpp` |
| 9 | Revisi | HUD icon baru | Nunggu asset |
| 14 | Revisi | Daftar keybind | `ui/keybindsTab.cpp` |
| 15 | Revisi | UI inventory | `ui/inventoryScreen.cpp` |
| 17 | Bug | FPS counter gak mati | `optionsScreen.cpp`, `hudRenderer.cpp` |
| - | Revisi | Reset buttons look | `keybindsTab.cpp` |
| - | (V) | Scroll indicator visual | `keybindsTab.cpp` |

## Status

| #   | Status  |
| --- | ------- |
| 1   | Fixed di commit sebelumnya — HealthBarTimer 2s, muncul pas kena damage |
| —   | Fixed di commit a860500 — HealthBarTimer=0 langsung pas Health<=0 sebelum death anim, biar health bar gak nongol selama death animasi |
| 9   | Fixed — redesain layout + 3 PNG icon (bagIcon, settingsIcon, killCount) |
| 14  | Fixed di commit sebelumnya — sections, font, hover, refactor 6 sub-functions |
| 15  | Fixed — INVENTORY_UI font, rounded corners, stack color #999, legend reposition, ghost feedback, extracted DrawInventory helpers |
| 17  | Fixed di commit sebelumnya — sync showFPS dari state di Show() |
| Boss HP bar | Fixed — DrawBossHPBar() di hud.cpp, trigger detection range / CELL_BOSS prefab, barY=30 |
| Reset buttons | Pending — styling font/warna |
| Scroll indicator | Fixed — scrollbar visual (scrollBar.png), scroll BG gray rect, drag interaction |
| #08 Inventory UI | Fixed — semua item di issue #08 sudah diimplementasi lewat #15 |
