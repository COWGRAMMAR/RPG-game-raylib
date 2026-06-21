# Issue #26 — Minimap Bugs (Post-Phase 3)

## Tipe
Bug / Enhancement
**Status**: Belum dikerjakan

---

## 1. Panel Minimap Terlalu Kecil

**Deskripsi:** Minimap cuma 90×90 pixel untuk main_hub (30×30 tile). Kesulitan liat detail.

**Variabel kontrol:**
- `MINIMAP_PANEL_SCALE` (sekarang `3`) — `include/map/minimap.h:31`
- `MINIMAP_PANEL_PADDING` (sekarang `10`) — `include/map/minimap.h:33`
- `CalculateLayout()` — `src/map/minimap.cpp:292`

Scale 3 → 1 tile = 3 pixel. Untuk map gede (164 tile), scale 3 aja udah 492px.

**Opsi fix:** Scale dinamis berdasarkan ukuran map.

---

## 2. Player Gak Bisa Gerak Pas Minimap Buka

**Deskripsi:** `!g_MinimapScreen.IsActive()` di `main.cpp:319` nge-block `UpdateLogicAll()` entirely.

**Yang diinginkan:** Player bisa gerak (jalan) pas minimap buka, tapi interaksi (E key, serang, pickup) dimatiin.

**Root Cause:** Satu guard untuk semua logic. Perlu granularitas antara movement vs interaction.

---

## 3. Inventory  Minimap Mutual Exclusion

**Deskripsi:** Bisa buka inventory (Tab) dan minimap (M) bersamaan — konflik input.

**Belum ada kode mutex. Yang udah ada:**
- Pause  Minimap mutex (`main.cpp:294-296`)
- Perlu tambah: kalo inventory buka → M gak boleh buka minimap
- Kalo minimap buka → Tab tutup minimap dulu sebelum buka inventory

---

## 4. Fog of War Nutupin Grid (UNEXPLORED Opaque)

**Deskripsi:** Fog layer render UNEXPLORED tile sebagai `{20,20,20,255}` (opaque hitam). Pas minimap pertama dibuka, SEMUA tile UNEXPLORED — grid ketutup fog.

**Fog pipeline:**
- `RenderFogLayer()` — render fog state ke fogRT (`minimap.cpp:318`)
- `DrawFogLayer()` — draw fogRT di atas grid texture (`minimap.cpp:345`)
- UpdateMinimapFog jalan setiap frame, reveal radius=10 tile (`minimap.cpp:114`)

**Fix possible:**
- Kurangi `UNEXPLORED` alpha jadi semi-transparan
- Atau reveal radius diperbesar pas Show()
- Atau fog di-off kalo player belum explore (full map visible)

**Confirmed:** Grid texture rendering works (test dengan fog disabled — "full biru").

---

## 5. Player Marker Offset Salah

**Deskripsi:** Posisi player dot di minimap gak center. Kayaknya top-left grid yang ditampilin, bukan posisi player.

**Root Cause:** `Show()` (`minimap.cpp:251`) set `panOffset` ke `(viewRect.width - scaledW) / 2` — nge-center ke grid, bukan ke player. Untuk grid yang lebih kecil dari viewRect, ini fine. Untuk grid gede (scaledW > viewRect), player dot bisa di luar view.

**Fix:** `Show()` harus center `panOffset` ke tile posisi player, bukan ke grid center.

---

## 6. Floor_a Map Pake Data main_hub

**Deskripsi:** Pas ganti map ke floor_a, minimap masih nampilin grid main_hub. Seperti `InitWithMap()` gak dipanggil untuk floor_a.

**Loading paths di loading_screen.cpp:**
- `HandleMapSwitch` — panggil `InitAll()` (otomatis `MinimapSystem::InitWithMap()`)
- `HandleFastPath` — panggil `InitAll()` (`screen_handler.cpp`) — ada `InitWithMap()`
- `HandleInitialLoad` — panggil `InitAll()` — ada `InitWithMap()`

**Kemungkinan:** floor_a punya loading path yang beda atau tilesonMap masih nunjuk ke main_hub. Perlu tracing.

---

## 7. Warna Tile Gak Dibeda-bedain

**Deskripsi:** Semua GID>0 di-render biru (debug). Tidak ada perbedaan warna antara lantai, tembok, obstacle.

**Rencana asli:**
1. Mapping GID ke warna dasar (jenis lantai/tembok)
2. Obstacle collision dari `gCollisionCache.rects` di-overlay sebagai warna obstacle

**GID ranges** — tileset di Tiled maps punya GID ranges berbeda. Perlu dicek:
- GID 1-N = tileset1 (floor)
- GID N+1-M = tileset2 (walls)
- GID = 0 = void

**Perlu:**
- `MapGidToColor()` dengan range-based mapping
- `BuildGridTexture()` obstacle overlay dari `gCollisionCache.rects` (koordinat pixel, TILE_SIZE=16)

---

## Technical Notes

### Grid Texture Pipeline (confirmed working)
```
BuildGridTexture() → GenImageColor() + LoadTextureFromImage() → gridTexture
```

### Draw Pipeline
```
1. Panel background  → DrawRectangleRec (panelRect)
2. RenderFogLayer()  → fogRT (offscreen)
3. ScissorMode       → viewRect
4. DrawTexturePro    → gridTexture
5. DrawFogLayer()    → fogRT.texture
6. DrawPlayerMarker()
7. EndScissorMode
```

### Key Files
| File | Fungsi |
|---|---|
| `include/map/minimap.h` | Konstant, struct, class declaration |
| `src/map/minimap.cpp` | Full implementation (575 lines) |
| `src/core/main.cpp:292-296` | Update + pause mutex |
| `src/core/main.cpp:319` | UpdateLogicAll guard |
| `src/rendering/hud.cpp` | MinimapSystem::Draw() dari DrawPlayerHUD |

### Related Constants
| Constant | Value | File:Line |
|---|---|---|
| `MINIMAP_TILE_TO_PX` | 1 | minimap.h:25 |
| `MINIMAP_REVEAL_RADIUS` | 10 | minimap.h:27 |
| `MINIMAP_FOG_EXPLORED_ALPHA` | 0.6f | minimap.h:29 |
| `MINIMAP_PANEL_SCALE` | 3 | minimap.h:31 |
| `MINIMAP_PANEL_PADDING` | 10 | minimap.h:33 |
