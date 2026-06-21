---
status: phase-3
phase: 3
updated: 2026-06-20
---

# Minimap Feature

## Goal
Implement minimap overlay (pause gameplay) dengan fog of war, pan, dan player marker aja. 
Overlay persis seperti inventory — toggle on/off, game paused di belakang, Screen-based.
**Gak ada** enemy/item/POI dots — minimap navigation aid, bukan wallhack.

## Session Log [2026-06-19] — Keputusan Final & Architecture

### Keputusan Final (dari diskusi)

| # | Point | Keputusan |
|---|-------|-----------|
| 1 | Zoom | **Fix — gak ada zoom.** Panning aja (geser-geser). |
| 2 | Overlay vs Replacement | **Overlay persis inventory.** Game pause, visible di belakang. |
| 3 | Screen model | **Screen-based** — inherit dari `Screen` class (kayak menu lain). Tapi file di `map/`. |
| 4 | Folder | **`include/map/minimap.h`** + **`src/map/minimap.cpp`** |
| 5 | Scale | **2×2 tile → 1 pixel.** Worldgen 164 tile → 82×82 px. |
| 6 | Grid container | **`std::vector<Color>`** (bukan raw pointer). |
| 7 | Fog key | **Map path** (deterministic: `.tmj` path. Worldgen: path + seed?). |
| 8 | Reveal radius | **10 tiles** (tentative, bisa riset lagi). |
| 9 | Pause menu priority | Minimap gak muncul di atas pause. Pause > minimap. |
| 10 | Toggle behavior | Sama persis inventory. Mutual exclusion udah di input.cpp. |

### Architecture Pattern (dari raylib fog of war example)

```
Grid RT (tile-resolution, 1px/tile)
  ├── Static grid: pre-render sekali pas map load
  ├── Fog layer: 3-state (unexplored/visible/explored)
  └── Draw: Single DrawTexturePro per layer (bukan 27K DrawRectangle)
```

### Perubahan dari Plan Sebelumnya

| Aspek | Sebelum | Sesudah |
|-------|---------|---------|
| Zoom | Zoom 0.25-4.0x | Fix, no zoom |
| Scale | 1 tile = 4px | 2 tile = 1px |
| Camera | Zoom + pan Camera2D | Pan-only Camera2D |
| Grid type | `Color*` raw pointer | `std::vector<Color>` |
| Fog type | `bool*` raw pointer | `std::vector<unsigned char>` (3-state: 0/1/2) |
| File location | `include/ui/minimap.h` | `include/map/minimap.h` |
| Screen inherit | Explicitly NOT Screen | Inherit Screen class |

## Konteks & Arsitektur

### Filosofi: Navigation Aid, Bukan Wallhack

Minimap cuma nampilin **layout statis** (wall/floor) dan **posisi player**. Gak ada:
-  Enemy dots — biar explorasi tetap seru
-  Runtime objects (crate/barrier/door) — dari propbehavior, dinamis
-  Item/POI markers — chest, sign, pass, dll

Player cukup tau **"gue lagi dimana"** dan **"lorong ini ujungnya tembok atau bukan"**. Detail lainnya biar player eksplor sendiri.

### Tile Grid System — 2 Layer Approach

**Layer 1: Tile Layer (GID)** — baseline warna dari tile visual:
- Parse tile layer pake Tileson, ambil GID per tile
- GID → mapping warna: wall=dark gray, floor=light brown, void=black
- Belum tentu akurat kalo tileset gak punya properti wall/floor yang jelas

**Layer 2: Object Layer "obstacle"** — override akurasi wall:
- Tileson `GetObjectsByLayer("obstacle")` — cari rectangle objects
- Tile yang intersect sama obstacle rect → **override jadi WALL_COLOR**
- Crate/barrier (runtime dari propbehavior) TIDAK ada di layer ini — obstacle layer isinya static collision aja
- Jadi gak ada risk nampilin runtime object yang dinamis

**Alur BuildMinimapGrid():**
```
For each 2×2 tile block (bx, by):
  GID = avg dari 4 tile
  color = MapGidToColor(GID)        ← baseline

  for each obstacleRect (cached):
    if BlockIntersectsRect(bx, by, obstacleRect):
      color = WALL_COLOR            ← override
      break

  g_Minimap.grid[by*halfW + bx] = color
```

- **Non-worldgen maps**: obstacle rect dari Tileson object layer
- **Worldgen maps**: cek `g_worldgenCells[]` untuk cell type override + obstacle layer
- **Scale**: 2 tile map = 1 px minimap → grid RT width/2 × height/2

### Fog of War

- **RenderTexture2D** di tile-resolution (setengah ukuran grid karena 2:1)
- **3-state fog:**
  - `0` = unexplored → BLACK
  - `1` = visible → BLANK (transparan)
  - `2` = explored (pernah visible) → Fade(BLACK, 0.6f)
- **Update**: dari player position tiap frame, reveal radius 10 tiles
- **Persistence**: `std::unordered_map<std::string, std::vector<unsigned char>>` keyed by map path
  - Simpan di GameSnapshot (seperti barrierMap pattern)
  - Worldgen maps: perlu handle unique key (path + worldseed?)
- **Bilinear filter** pas scaling → fog edge smooth otomatis

### Markers (Cuma Player)

| Type | Warna | Notes |
|------|-------|-------|
| Player | Green / Bright | Satu-satunya marker di minimap |

**Gak ada** enemy, item, runtime object, atau POI markers — dijaga biar minimap tetap minimal dan game tetap menantang.

## Implementation Plan

### Phase 1: Grid Build System [DONE]

Build tile classification grid from collision data at map load time.

- [x] 1.1 Create `include/map/minimap.h` dengan `MinimapData` struct:
  ```cpp
  struct MinimapData {
      int gridWidth, gridHeight;          // map tile dim / 2 (2:1 scale)
      int mapTileWidth, mapTileHeight;    // asli
      std::vector<Color> grid;            // row-major
      std::vector<unsigned char> fog;     // 3-state (0/1/2)
      std::unordered_map<std::string, std::vector<unsigned char>> fogCache;
  };
  ```
- [x] 1.2 `BuildMinimapGrid()` function:
  - Layer 1: Iterate tile GIDs, map to color via `MapGidToColor()` (GID 0=black, 1-15=tan floor, 16-47=dark brown wall, 48+=gray)
  - Layer 2: `BlockIntersectsObstacle()` helper via `CheckCollisionRecs`
  - Iterate 2×2 tile blocks, obstacle override → WALL_COLOR
  - Fill `g_Minimap.grid` at half resolution
- [ ] 1.3 Hook ke `LoadMap()` (loading_screen.cpp) — rebuild tiap ganti map **PENDING coupling**
- [x] 1.4 Pre-render static grid ke grid `RenderTexture2D` via `PreRenderGrid()`
- [x] 1.5 `extern MinimapData g_Minimap` global

### Phase 2: MinimapScreen Class [DONE]

- [x] 2.1 `include/map/minimap.h` — class `MinimapScreen`:
  - Pattern: `Show()`/`Hide()`/`IsActive()`/`Update()`/`Draw()` (kaya OptionsScreen, bukan inherit Screen)
  - gridRT + fogRT (RenderTexture2D), panOffset/dragStart/isDragging, panelRect/viewRect
- [x] 2.2 `Init()` — create RTs, `SetTextureFilter(BILINEAR)`, `SetTextureWrap(CLAMP)`, `PreRenderGrid()`
- [x] 2.3 `HandlePan()` — left-click drag, delta panOffset, clamp ke view boundary
- [x] 2.4 `PreRenderGrid()` — `BeginTextureMode(gridRT)` → loop grid → `DrawRectangle(1×1)` → `EndTextureMode`
- [x] 2.5 `UpdateFog()` — player tile pos → reveal radius 10 → update fog 3-state
- [x] 2.6 `RenderFogLayer()` + `DrawFogLayer()` — fogRT update + single `DrawTexturePro` flip-Y
- [x] 2.7 `DrawPlayerMarker()` — green dot (`Color{0, 255, 0, 200}`) at player position scaled
- [x] 2.8 `CalculateLayout()` — compute panelRect + viewRect, Show() centers pan
- [x] 2.9 `Draw()` — scissor mode, clear, DrawTexturePro gridRT + fogRT + player marker
- [x] 2.10 `Shutdown()` — unload render textures
- [x] 2.11 `extern MinimapScreen g_MinimapScreen` global

### Phase 3: Hook Into Game Loop [PENDING — MENUNGGU COUPLING]

Integrasi ke game loop existing. Ini phase paling sensitif karena nyentuh file existing — strict: **jangan break existing flow**.

#### 3.1 Input Binding

Cek existing `InputInstance.IsMapOpen()` / `toggleMap` di `input.cpp`:
- Pastiin `TOGGLE_MAP` keybind udah ada di `PlayerInput`
- Mutual exclusion dengan inventory: `MapOpen`  `InventoryOpen` — cek di `input.cpp` (udah ada dari riset)
- Priority: Pause > Minimap — jangan render minimap kalo pause aktif

#### 3.2 Init & Shutdown

Di `main.cpp` atau `screen_handler.cpp`:
```
Init: g_MinimapScreen.Init()     # setelah InitScreen()
Shutdown: g_MinimapScreen.Shutdown()  # sebelum CloseAudioDevice()
```

#### 3.3 Per-Map Build

Di `loading_screen.cpp` pas map selesai di-load (setelah `LoadMap()` + `InitAll()`):
```
BuildMinimapGrid(...)     # Parse tile GID + obstacle rects
g_MinimapScreen.Init()    # Re-create RTs + PreRenderGrid + restore fog
```

#### 3.4 Update + Draw Loop

Di `main.cpp` PLAY state — antara render gameplay world dan render HUD:
```cpp
// Update fog tiap frame
UpdateMinimapFog(playerX, playerY, TILE_SIZE);

// Toggle minimap (dari input, handle di update logic)
if (InputInstance.IsMapOpen() && !IsPauseOpen()) {
    g_MinimapScreen.Update(state, mousePos, mouseClicked);
}

// Draw pas render (di akhir frame, setelah gameplay draw)
if (g_MinimapScreen.IsActive()) {
    g_MinimapScreen.Draw(mousePos);
}
```

Architecture: `g_MinimapScreen.Update()` terima `GameState*` + mouse input; `Draw()` render overlay di atas gameplay.

#### 3.5 Map Switch Handling

Di `HandleMapSwitch()` (loading_screen.cpp):
```
// Sebelum LoadMap: save fog current map
SaveMinimapFogToCache(currentMapPath);

// Setelah LoadMap + init selesai:
g_MinimapScreen.ClearFogFromCache();
BuildMinimapGrid(...);
g_MinimapScreen.Init();         // Re-init dengan fog baru (restore dari cache otomatis)
```

#### 3.6 Files Changed (coupling)

- `src/core/main.cpp` — Init, Shutdown, Update+Draw loop
- `src/map/loading_screen.cpp` — BuildMinimapGrid call, map switch fog save/restore
- `include/core/playerinput.h` / `src/core/input.cpp` — kalo `TOGGLE_MAP` belum ada

### Phase 4: Fog Persistence [PENDING]

Mengikuti pattern save system (slot_-1 workspace, atomic write, CopyWorkspaceTo/MirrorToWorkspace):

#### 4.1 Direktori & File Structure

```
saves/
├── slot_-1/
│   └── minimap/                  # Fog data per map (runtime workspace)
│       └── {sanitized_map_path}.json
└── slot_N/
    └── minimap/                  # Copy dari -1 saat SaveManual
        └── {sanitized_map_path}.json
```

- Key: map path (deterministic) → `SanitizePath(mapPath)` → filename (sama persis pola checkpoint)
- Worldgen: key = mapPath + seed (deterministic dari seed, path aja cukup kaya checkpoint)
- Format JSON: flat `vector<unsigned char>` sebagai array integer + metadata

#### 4.2 Struktur Data JSON

```json
{
  "version": 1,
  "mapPath": "assets/maps/stage_1.json",
  "gridWidth": 82,
  "gridHeight": 82,
  "fogData": [0, 0, 2, 1, 2, 0, ...]   // unsigned char per cell, row-major
}
```

Wrapper struct:
```cpp
struct MinimapSaveData {
    int version;
    std::string mapPath;
    int gridWidth, gridHeight;
    std::vector<unsigned char> fogData;  // flat array
};
```

#### 4.3 SaveManager Methods (di `savemanager.h/cpp`)

```cpp
// Path helper
static std::string GetMinimapDir(int slot);                    // saves/slot_N/minimap/
static std::string GetMinimapPath(const std::string& mapPath, int slot);

// Core save/load
static bool SaveMinimapData(const MinimapSaveData& data, int slot);  // atomic write .tmp + rename
static MinimapLoadData LoadMinimapData(const std::string& mapPath, int slot);
static bool HasMinimapData(const std::string& mapPath, int slot);

// Workspace management (inline dengan CopyWorkspaceTo / MirrorToWorkspace)
static void ClearMinimapWorkspace(int slot);                   // Hapus saves/slot_N/minimap/

// Serialization
static nlohmann::json SerializeMinimap(const MinimapSaveData& data);
static MinimapSaveData DeserializeMinimap(const nlohmann::json& root);
```

#### 4.4 Workspace Integration

Di `CopyWorkspaceTo(slot)` dan `MirrorToWorkspace(sourceSlot)`:
- Copy seluruh `minimap/` subfolder dari source ke target (sama seperti checkpoints/manual/autosave)
- `ClearWorkspaceManual/Autosave/Checkpoints` → perlu tambah `ClearMinimapWorkspace()`

Atau alternatif simpel: fog data tetep di `fogCache` in-memory, dan di-copy bareng snapshot. Tapi karena user request subfolder terpisah, better explicit.

#### 4.5 Flow

| Event | Action |
|---|---|
| Map switch (keluar) | `SaveMinimapFogToCache(mapPath)` → `SaveMinimapData(minimapData, -1)` |
| Map switch (masuk) | `minimapData = LoadMinimapData(mapPath, -1)` → `RestoreMinimapFogFromCache()` |
| SaveManual | CopyWorkspaceTo(slot) otomatis bawa minimap/ (kaya checkpoints) |
| Load save / MirrorToWorkspace | Clear minimap/ -1 → copy dari slot_N |
| Restart | Clear minimap/ -1 → fog fresh (restart = new exploration) |
| New game | Clear semua, fog fresh |

#### 4.6 Files Changed

- `include/core/savemanager.h` — tambah method minimap + include minimap.h
- `src/core/savemanager.cpp` — implementasi save/load/workspace
- `include/map/minimap.h` — tambah `MinimapSaveData` struct (optional, bisa di savemanager.h)
- `src/map/minimap.cpp` — fungsi helper mapping antara fogCache  MinimapSaveData
- `src/map/loading_screen.cpp` — call sites: map switch, new game, restart
- `src/core/main.cpp` — autosave timer mungkin perlu sync

### Phase 5: Polish [PENDING]

- [ ] 5.1 Border/frame, background panel overlay (kayak inventory bg)
- [ ] 5.2 Close via TOGGLE_MAP / ESC
- [ ] 5.3 Ukuran panel fix (82×82 px + padding + border)
- [ ] 5.4 Map name label (opsional)

## Riset Tersisa

- [ ] Reveal radius final: 10 tiles (perlu di-test in-game)
- [ ] Color constants: WALL_COLOR, FLOOR_COLOR, DETAIL_COLOR, dll

## Reference — Raylib Official Fog of War (textures_fog_of_war.c)

[Source](https://github.com/raysan5/raylib/blob/master/examples/textures/textures_fog_of_war.c)

```cpp
// Render texture ukuran tile-resolution (1px = 1 tile)
RenderTexture2D fogOfWar = LoadRenderTexture(map.tilesX, map.tilesY);
SetTextureFilter(fogOfWar.texture, TEXTURE_FILTER_BILINEAR);
SetTextureWrap(fogOfWar.texture, TEXTURE_WRAP_CLAMP);

// Fog states: 0=unvisited(BLACK), 1=visible(BLANK), 2=fogged(Fade(BLACK,0.8f))

// Render fog layer (1px per tile)
BeginTextureMode(fogOfWar);
    ClearBackground(BLANK);
    for (y) for (x)
        if (fog[y*w+x] == 0) DrawRectangle(x, y, 1, 1, BLACK);
        else if (fog[y*w+x] == 2) DrawRectangle(x, y, 1, 1, Fade(BLACK, 0.8f));
EndTextureMode();

// Draw ke screen — single call, bilinear smooth scaling
DrawTexturePro(fogOfWar.texture,
    (Rectangle){ 0, 0, (float)fogOfWar.texture.width, -(float)fogOfWar.texture.height },
    (Rectangle){ 0, 0, (float)map.tilesX*MAP_TILE_SIZE, (float)map.tilesY*MAP_TILE_SIZE },
    (Vector2){ 0, 0 }, 0.0f, WHITE);
```

## Reference — Arthur Engine Minimap (C++ raylib)

[Source](https://github.com/IsaMaharramov/Arthur) — real dungeon crawler raylib.

```cpp
// Per-tile DrawRectangle, scale = tile size on minimap
int mapScale = (currentLevel <= 2) ? 6 : 3;
for (int mx = 0; mx < map.width; mx++)
    for (int my = 0; my < map.height; my++)
        DrawRectangle(offsetX + mx*mapScale, offsetY + my*mapScale,
                      mapScale-1, mapScale-1, tileColor);
// Player dot
DrawCircle(offsetX + (int)(player.x*mapScale), offsetY + (int)(player.y*mapScale), 3, GREEN);
// Enemy dots per type — color coded
DrawRectangle(offsetX + (int)(enemy.x*mapScale)-1, ... 3, 3, dotColor);
```
Pattern: minimap ke 3 layer — tile base, player dot, enemy dots.

## Research Findings (2026-06-19)

### 1. Pre-render Optimization
**Confirmed** — raylib official example uses `RenderTexture2D` at tile-resolution (1px/tile), bilinear filter for smooth scaling, single `DrawTexturePro`. Independent research (Stack Overflow, DeepWiki) confirms:
- GPU draw calls bottleneck bukan texture size — 27K `DrawRectangle` per frame vs 2 `DrawTexturePro`
- RenderTexture2D batches data ke GPU sekali, kemudian draw sebagai single quad
- Cocok buat minimap: static grid pre-render sekali, fog layer update tiap frame

### 2. Fog of War — 3 State
**Confirmed** — pattern konsisten di semua sumber:
- `0 = UNEXPLORED` → BLACK / opaque
- `1 = VISIBLE` → BLANK (transparan, current LOS)
- `2 = EXPLORED/FOGGED` → semi-transparan (Fade(BLACK, 0.6~0.8))
- Transition: every frame, old `1` → `2`, then current tiles around player → `1`
- **Persistence**: simpan fog array `unsigned char[]` (flat, width*height) ke save file via `SaveFileData()` / `LoadFileData()`

### 3. Camera2D Pan-only (no zoom)
- Set `camera.zoom = 1.0f` — never change
- Pan: `camera.target = GetScreenToWorld2D(...)` on mouse drag
- Bounds clamping: compute world extent via `GetWorldToScreen2D(minWorld)` / `GetWorldToScreen2D(maxWorld)`
- No rotation, no zoom — pure offset

### 4. Fog Persistence with Save System
- Fog array per map → `std::unordered_map<std::string, std::vector<unsigned char>>` di GameSnapshot
- `unsigned char` (not `bool`) — `vector<bool>` bit-packed, akses lambat dan gak bisa reference
- Key: map path buat deterministic, map path + seed buat worldgen
- Save: serialize fog array via `SaveFileData()` atau binary serialize ke snapshot

### 5. GitHub Raylib Minimap Examples
Repo relevan:
- **IsaMaharramov/Arthur** — C++ raylib dungeon crawler dengan minimap per-tile DrawRectangle, color-coded enemy dots
- **SanchoCC/Fow** — C++ raylib tactical game dengan fog of war + minimap (4 stars, active)
- **N0zye/raycasting** — raylib raycasting dengan minimap FOV cone

## Perubahan dari Riset

| Aspek | Sebelum | Sesudah |
|-------|---------|---------|
| Fog container | `std::vector<bool>` | `std::vector<unsigned char>` (bit-packed vector<bool> lambat) |
| Fog persist key | Path + seed? | Final: path aja (worldgen deterministic dari seed — path cukup) |
| Reveal radius | 10 tiles (tentative) | Tetep 10 tiles, nanti test in-game |
| POI colors | Table defined | Tetep, pake enum + switch |
| Minimap RT filter | Default | `TEXTURE_FILTER_BILINEAR` + `TEXTURE_WRAP_CLAMP` |

## Notes

- 2026-06-17: `TOGGLE_MAP` keybind, `InputState::toggleMap`, `PlayerInput::MapOpen` already exist.
- 2026-06-17: `MapOpen` mutual exclusion with `InventoryOpen` already handled in input.cpp.
- 2026-06-19: **Architecture pivot**: no zoom, 2:1 scale, RenderTexture2D pre-render pattern.
- 2026-06-19: Folder changed from `ui/` to `map/`.
- 2026-06-19: Grid type changed from raw pointer to `std::vector<Color>`.
- 2026-06-19: Fog changed from `std::vector<bool>` to `std::vector<unsigned char>`.
- 2026-06-19: Inherit `Screen` class (not standalone class).
- 2026-06-19: Raylib fog of war example adopted as reference pattern.
- 2026-06-19: 2×2 tile → 1px — corridor 1-tile-wide bakal ilang (acceptable tradeoff).
- 2026-06-19: Research complete — raylib official + Arthur Engine + Fow repo confirmed patterns.
- 2026-06-19: **Content scope finalized** — wall layout + player marker ONLY. No enemies, items, runtime objects, POI. Navigation aid, not wallhack.
- 2026-06-19: Input data: GID dari tile layer (bukan object layer) untuk bedain wall vs floor. Runtime objects (propbehavior rectangles) gak digambar.
