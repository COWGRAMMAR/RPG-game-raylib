# Issue #26 — Minimap Bugs (Post-Phase 3)

## Tipe
Bug / Enhancement
**Status**: Poin 1/2/3/5/6/7 DONE, Poin 4 (fog) partial

---

## 1. ~~Panel Minimap Terlalu Kecil~~  DONE

Panel diubah ke fixed size 500×460 (`MINIMAP_VIEWPORT_W/H`). Grid discale dengan `PANEL_SCALE = TILE_PX * ZOOM` (16×1 = 16 px/tile). Layout di `CalculateLayout()`.

Variabel kontrol:
- `MINIMAP_TILE_PX` = 16 — detail per tile di grid texture (build-time)
- `MINIMAP_ZOOM` = 1.0f — render multiplier (float, fixed no dynamic zoom)
- `MINIMAP_PANEL_SCALE` = derived = `TILE_PX * ZOOM`

---

## 2. ~~Player Gak Bisa Gerak Pas Minimap Buka~~  DONE

Penanganan granular via `input.cpp UpdateState()`:

```
g_MinimapScreen.IsActive() → block: E/interact, combat, pickup, use item
                           → allow: WASD movement, map pan/drag
```

`main.cpp:319` — `UpdateLogicAll()` guard udah di-refactor. Player bisa jalan, sisanya di-block.

---

## 3. ~~Inventory × Minimap Mutual Exclusion~~  DONE

Dua arah:
- `input.cpp`: `IsToggleMap()` ngecek `!IsInventoryOpen()` — M gak bisa buka minimap kalo inventory buka
- `MinimapSystem::Update()` line 779: kalo inventory kebuka, minimap auto tutup

Bonus: pause × minimap mutex juga jalan (main.cpp:294-296).

---

## 4. Fog of War — ~~Enabled + Y-flip Fix~~  (Initial reveal need)

**Status:** Fungsi fog semua exist dan working. Yang udah:

### Enabled
- `RenderFogLayer()` — aktif (uncommented)
- `DrawFogLayer()` — aktif (uncommented)
- `UpdateMinimapFog()` — jalan tiap frame dari `MinimapSystem::Update()` + `MinimapScreen::Update()`

### Y-flip Fix
DrawFogLayer src rect height `-(float)(h * TILE_PX)` — render texture di raylib butuh negative height (sama kaya screen_handler.cpp:474).

### Reveal Position Fix
`UpdateMinimapFog()` pake `PlayerInstance.GetCenter()` instead of `GetPosition()` — sama kaya marker fix. Fog reveal center sekarang akurat.

### Initial Reveal —  PENDING
Pas new game, `ResetMinimapFog()` set SEMUA tile UNEXPLORED. Player buka minimap → semua hitam kecuali area yang tereveal setelah beberapa frame `UpdateMinimapFog`.

**Proposed fix:** Panggil `UpdateMinimapFog(playerX, playerY)` sekali setelah `ResetMinimapFog()` di `InitWithMap()` atau `Init()`, biar spawn area langsung kelihatan.

### Known issue: Duplikasi call
`UpdateMinimapFog()` dipanggil dari 2 tempat — `MinimapSystem::Update()` (tiap frame global) dan `MinimapScreen::Update()` (cuma pas aktif). Kalo minimap aktif, fog update dobel per frame → harmless (idempotent) tapi wasteful.

### Interface untuk save system (READY)
| Kebutuhan | Status |
|---|---|
| `g_Minimap.fogCache` — `unordered_map<string, vector<u8>>` |  |
| `g_Minimap.fog` — `vector<u8>` per map |  |
| `SaveMinimapFogToCache(mapPath)` |  |
| `RestoreMinimapFogFromCache(mapPath)` |  |
| `FogState` enum (UNEXPLORED=0, VISIBLE=1, EXPLORED=2) |  |

---

## 5. ~~Player Marker Offset Salah~~  DONE

### Marker position
`DrawPlayerMarker()` pake `PlayerInstance.GetCenter()` (Position.x + HitboxOffsetX + HitboxWidth/2) — bukan `GetPosition()` (top-left sprite). Fix yang sama diterapin di `UpdateView()` (camera follow center pake position + FRAME_SIZE/2).

### Camera follow
`UpdateView()` — kalo scaled grid > viewport, `panOffset` center ke player. Kalo grid < viewport, center ke grid.

### Marker radius
`MINIMAP_MARKER_RADIUS` = 10.0f — konstanta independen, gak terikat PANEL_SCALE.

---

## 6. ~~Floor_a Map Pake Data main_hub~~  DONE

`InitWithMap()` dipanggil di `loading_screen.cpp HandleMapSwitch` setelah `RebuildObstacleCache`. Juga ada di `screen_handler.cpp InitAll()`.

---

## 7. ~~Warna Tile Gak Dibeda-bedain~~  DONE

### Tile coloring — sampled dari tileset
- `BuildGridTexture()` sample tiap GID unik dari tileset images → pre-sample TILE_PX×TILE_PX block
- Multi-layer compositing bottom-up (Painter's Algorithm dengan alpha threshold 128)
- TEXTURE_FILTER_POINT (gak blur)
- GID cache: `tileSampleCache` — di-populate per-GID, cleared tiap map switch

### Removed
- `MapGidToColor()` function
- Obstacle overlay (red pixel loop on gCollisionCache.rects)
- Unused includes

---

## Technical Notes

### Draw Pipeline (current)
```
Layer 0: Fullscreen black tint
Layer 1: Background artwork (mapBG.png) — decorative
  RenderFogLayer() → fogRT (offscreen, di luar scissor)
  BeginScissorMode(viewRect):
Layer 2: gridTexture (sampled tiles, POINT filter)
Layer 3: fogRT overlay (Y-flip neg height)
Layer 4: Player marker (dot + glow)
  EndScissorMode()
Layer 5: Legend
```

### Key Constants (current)
| Constant | Value | File |
|---|---|---|
| `MINIMAP_TILE_PX` | 16 | minimap.h |
| `MINIMAP_ZOOM` | 1.0f | minimap.h |
| `MINIMAP_PANEL_SCALE` | `TILE_PX * ZOOM` (16.0f) | minimap.h |
| `MINIMAP_VIEWPORT_WIDTH` | 500 | minimap.h |
| `MINIMAP_VIEWPORT_HEIGHT` | 460 | minimap.h |
| `MINIMAP_REVEAL_RADIUS` | 10 | minimap.h |
| `MINIMAP_FOG_EXPLORED_ALPHA` | 0.6f | minimap.h |
| `MINIMAP_MARKER_RADIUS` | 10.0f | minimap.h |

### Key Files
| File | Fungsi |
|---|---|
| `include/map/minimap.h` | Konstant, struct, class declaration |
| `src/map/minimap.cpp` | Full implementation (797 lines) |
| `src/core/main.cpp` | Update loop, mutex guards |
| `src/rendering/hud.cpp` | DrawPlayerHUD → MinimapSystem::Draw() |
| `src/systems/input.cpp` | IsToggleMap + mutex logic |
| `src/core/loading_screen.cpp` | HandleMapSwitch → InitWithMap |
