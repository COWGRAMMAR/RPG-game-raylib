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
Legend keybind di pojok kanan atas (`hud.cpp:808-843`) masih pake text doang:
- `[E] Interact`, `[I] Inventory`, `[Q] Drop Item`, `[Shift+Q] Drop All`, `[Esc] Pause`, `[Scroll] Switch Item`
- Gak ada icon visual
- Interact nempel permanen, gak kontekstual
- Keybind text hardcoded, gak dinamis

### Rencana Baru

#### Layout Final

```
┌─────────┐                                    ┌─────────┐
│ [Esc]   │                                    │  ║ HP  ║  │
│  Pause  │                                    └─────────┘
└─────────┘
                  ┌──────────┐
                  │ [E] Buka │   ← Interact (kontekstual — cuma muncul kalo ada yg bisa di-interact)
                  └──────────┘


               ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐
     [I]  ─────│1│ │2│ │3│ │4│ │5│───── [Q] Drop
     Inven     └─┘ └─┘ └─┘ └─┘ └─┘       [Shift+Q] All
```

#### Detail per Elemen

| Posisi | Konten | Tipe | Perilaku |
|--------|--------|------|----------|
| **Kiri atas** | Icon Pause + text keybind | Icon + text dinamis | Selalu muncul |
| **Tengah layar** (area boss HP bar) | Interact prompt | Text kontekstual | Muncul pas player nyentuh hitbox door/chest/sign |
| **Bottom-center** (sejajar hotbar) | Drop Item + Drop All | Text dinamis | Selalu muncul |
| **Bottom-center** | Inventory icon + text keybind | Icon + text dinamis | Sejajar hotbar, di kiri hotbar |
| **Hotbar** | Slot diperbesar + keybind number per slot | Slot + text dinamis | Keybind number di bawah tiap slot |

#### Yang Didrop
- **Interact** dari legend (pindah ke tengah layar kontekstual)
- **Drop Item / Drop All** dari top-right (pindah ke bawah sejajar hotbar)
- **Switch Item / Scroll** — dropped entirely (anggap obvious + hotbar numbers udah cukup)

#### Keybind Dinamis
Semua text keybind terikat ke sistem keybind (#12):
- Pas user ganti keybind, text otomatis update
- Contoh: Interact dari E → F, prompt `[E] Buka` jadi `[F] Buka`

#### File yang Diubah
| File | Perubahan |
|------|-----------|
| `src/rendering/hud.cpp` | Drop legend lama (line 808-843), render ulang layout baru |
| `include/rendering/hud.h` | Kalo perlu fungsi baru |
| `src/rendering/hud.cpp` (DrawPlayerHUD) | Render Pause icon di kiri atas |
| `src/rendering/hud.cpp` (DrawPlayerHUD) | Render Inventory icon sejajar hotbar |
| `src/rendering/hud.cpp` (DrawPlayerHUD) | Render Drop/DropAll di bottom-center |
| `src/rendering/hud.cpp` | Render Interact kontekstual (deteksi prop hitbox) |
| `src/rendering/hud.cpp` (DrawHotbar) | Perbesar slot + render keybind di bawah |

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

## New: Keybinds Scroll Indicator — Teks ke Visual

### Masalah
Scroll indicator di keybinds tab masih pake teks `^^^` / `vvv` buat nandain bisa di-scroll.

```cpp
// keybindsTab.cpp :182-192 (kira-kira)
DrawText("^^^", ...);
```

### Expected
Ganti jadi visual scrollbar — vertical slider tipis di pojok kanan yang nunjukin posisi scroll.

### File yang mungkin diubah
- `src/ui/keybindsTab.cpp` — bagian scroll indicator

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
| - | Revisi | Scroll indicator visual | `keybindsTab.cpp` |
