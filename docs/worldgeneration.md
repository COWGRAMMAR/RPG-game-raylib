# World Generation — Save/Load Pipeline

## 1. Arsitektur Global

### Konstanta & Path

| Simbol | Nilai | Lokasi |
|--------|-------|--------|
| `SEED_COUNT` | 5 | `include/core/seedmanager.h:22` |
| `WORLDSEED_DIR` | `assets/maps/World_generation/worldseed` | `src/map/worldgenio.cpp:18` |
| `BG_MAP` | `assets/maps/World_generation/background_map.json` | `src/map/worldgenio.cpp:19` |

### Struktur Folder Save

```
assets/maps/World_generation/worldseed/
  save_1/
    meta.json              <- seeds, currentStage, prevStage, currentSlot
    runtime.json           <- per-stage state (chests, crates, bombs, dll)
    maps/
      stage_1.json         <- generated map
      stage_2.json
      ...
  save_2/
    ...
```

### Data Structures Kunci

| Variabel | Type | Definisi | Isi |
|----------|------|----------|-----|
| `g_SeedManager` | `SeedManager` | `src/core/seedmanager.cpp:4` | seeds[5], currentStage, prevStage, currentSlot, isRunActive |
| `gState` | `GameState *` | `src/core/screen_handler.cpp:52` | currentScreen, loading flags, map switch flags |
| `currentMapPath` | `std::string` (static) | `src/map/map.cpp:52` | path ke map yang sedang aktif |
| `tilesonMap` | `TilesonMapData *` | `src/map/map.cpp:34` | data tileson map yang sedang di-load |
| `barrierManager` | `BarrierManager` | `src/map/propsbehavior.cpp:856` | barrier state per stage |

---

## 2. Pipeline: Start → Worldgen → Gameplay

### Flow Diagram

```
MAIN_MENU → "Start" → LOADING → first-time path (assetsLoaded=false)
                                   stage 0: InitTextures()
                                   stage 1: InitMap() → tutorial.json
                                   stage 2: (count)
                                   default: InitAll() + PLAY

MAIN_MENU → "Load"  → LOADING → map-switch path (isSwitchingMap=true)
                                   stage 0: UnloadMap()
                                   stage 1: LoadMap() + RunWorldgen() + LoadRuntimeState()
                                   stage 2: Player Init + Entities
                                   stage 3: Camera + PLAY
```

### 2a. Fresh Start (Start Game)

Button index 0 di main menu:

**`src/ui/mainMenu.cpp:76-78`**
```cpp
case 0:  // Start Game
    state->currentScreen = LOADING;
    break;
```

Loading screen first-time path:

**`src/core/loading_screen.cpp:208-252`**

| Stage | Fungsi | File:Line |
|-------|--------|-----------|
| 0 | `InitTextures()` — load sprites, atlas, tileset textures | `src/core/loading_screen.cpp:212` |
| 1 | `HasSavedState()` → `LoadMap(savedMap)` / `InitMap()` | `src/core/loading_screen.cpp:220-227` |
| 2 | Increment stage (kosong) | `src/core/loading_screen.cpp:233` |
| default | `assetsLoaded = true`, `InitAll()`, `RestoreGameState()`, `InitMainMenu()`, `currentScreen = PLAY` | `src/core/loading_screen.cpp:239-251` |

**Tidak ada worldgen di path ini.** Map yang di-load adalah `assets/maps/tutorial.json`.

### 2b. Worldgen Stage Entry (door trigger)

Saat player masuk pintu di stage worldgen, dipanggil:

**`src/map/worldgenio.cpp:311-334`** — `WorldgenIO::NextStage()`
```
1. SaveRuntimeState(currentStage)         — simpan state stage lama
2. g_SeedManager.NextStage()               — increment stage
3. SaveMeta()                              — simpan seeds + stage ke disk
4. GetStagePath(newStage) → "save_N/maps/stage_X.json"
5. SwitchMap(stagePath, "start")
6. TrimStageStack()
```

`SwitchMap()` di **`src/map/map.cpp:457-489`**:
```
1. SaveEnemiesForMap(), SaveItemsForMap()
2. Push current map to history stack
3. Set gState->isSwitchingMap = true
4. Set pendingMapPath / pendingDoorName
5. Reset enteredLoading
6. currentScreen = LOADING
```

### 2c. Map Switch Loading (worldgen entry / load game)

Loading screen mendeteksi `isSwitchingMap == true`:

**`src/core/loading_screen.cpp:85-174`**

| Stage | Fungsi | Detail |
|-------|--------|--------|
| 0 | `UnloadMap()` + `spawnFlowFields.clear()` | `loading_screen.cpp:91-97` |
| 1 | `LoadMap(path)` + `SetCurrentMapPath()` + **`RunWorldgen()`** + **`LoadRuntimeState()`** + `SpawnObject()` + `RebuildObstacleCache()` | `loading_screen.cpp:99-124` |
| 2 | `PlayerInstance.Init()` + `Entities::Clear()` + `Add(PlayerInstance)` + `SpawnEnemiesFromMap()` + `LoadItemsForMap()` | `loading_screen.cpp:126-150` |
| 3 | Camera setup + clear switch flags + `currentScreen = PLAY` | `loading_screen.cpp:152-172` |

**Stage 1 — worldgen trigger** (`loading_screen.cpp:107-113`):
```cpp
if (!isBack && path contains "worldseed/save_")
{
    int stageIdx = g_SeedManager.GetCurrentStage();
    uint64_t seed = g_SeedManager.GetSeed(stageIdx);
    RunWorldgen(seed, stageIdx == SEED_COUNT - 1);   // ← generate map
    WorldgenIO::LoadRuntimeState(stageIdx);            // ← restore runtime
}
```

---

## 3. Fungsi-Fungsi Kunci

### 3a. SeedManager (`src/core/seedmanager.cpp`)

| Fungsi | Line | Deskripsi |
|--------|------|-----------|
| `InitRun(saveSlot)` | 9-21 | Generate SEED_COUNT random seeds, set currentStage=0, slot=saveSlot |
| `GetSeed(stage)` | 23-28 | Return seed untuk stage tertentu |
| `NextStage()` | (seedmanager.h:50-61) | Increment currentStage, simpan prevStage |
| `GoBackStage()` | (seedmanager.h:63-68) | Return prevStage dan reset ke -1, satu kali pakai |
| `SaveMeta(path)` | 30-42 | Simpan seeds[], currentStage, prevStage, currentSlot ke JSON |
| `LoadMeta(path)` | 45-72 | Load dari JSON dan restore state |

### 3b. WorldgenIO (`src/map/worldgenio.cpp`)

| Fungsi | Line | Deskripsi |
|--------|------|-----------|
| `GetMetaPath(slot)` | 37 | Return `worldseed/save_{slot}/meta.json` |
| `GetStagePath(idx)` | 48 | Return `worldseed/save_{slot}/maps/stage_{idx+1}.json` |
| `GetNextAvailableSlot()` | 74 | Scan folder `save_*`, return max+1 |
| `GetTopSlot()` | 107 | Scan folder `save_*`, return max (untuk Load) |
| `InitRun(slot)` | 159 | Buat folder slot, generate worldgen, copy BG map, fix path, save meta |
| `SaveRuntimeState(idx)` | 193 | Simpan chests/crates/bombs/deadEnemies/items/barrier ke runtime.json |
| `LoadRuntimeState(idx)` | 258 | Load runtime.json dan restore semua state |
| `NextStage()` | 311 | Save old stage → increment → SwitchMap ke stage baru |
| `PrevStage()` | 336 | Save old stage → GoBackStage → SwitchMap ke stage sebelumnya |

### 3c. Map Operations (`src/map/map.cpp`)

| Fungsi | Line | Deskripsi |
|--------|------|-----------|
| `RunWorldgen(seed, isBoss)` | 286-309 | Generate world pake seed, stamp layout, spawn exit doors |
| `SwitchMap(path, door)` | 457-489 | Save state lama, set pending switch, trigger LOADING screen |
| `GoBack()` | 496-524 | Pop history stack, set isGoingBack, trigger LOADING screen |
| `GetCurrentMapPath()` | 558-561 | Return currentMapPath (static string) |
| `SetCurrentMapPath(path)` | 567-569 | Update currentMapPath |

### 3d. Loading Screen (`src/core/loading_screen.cpp`)

| Fungsi | Line | Deskripsi |
|--------|------|-----------|
| `InitLoadingScreen(state)` | 45-64 | Reset loadingStage/Progress, set text sesuai mode |
| `UpdateLoadingScreen(state)` | 73-253 | Execute loading stages — first-time OR map-switch |
| `RenderLoadingScreen(state)` | 260-277 | Render progress bar + text |
| `IsLoadingComplete(state)` | 285-288 | Return loadingComplete flag |

### 3e. Main Loop (`src/core/main.cpp`)

| Area | Line | Deskripsi |
|------|------|-----------|
| MAIN_MENU handler | 64-70 | Update + Render main menu |
| LOADING handler | 74-88 | InitLoadingScreen (sekali) → Update → Render |
| PLAY handler | 111-168 | Fixed timestep gameplay loop |
| Auto-save on exit | 172-173 | `SaveRuntimeState` jika run masih aktif |

### 3f. Main Menu (`src/ui/mainMenu.cpp`)

| Button | Line | Aksi |
|--------|------|------|
| Start (index 0) | 76-78 | Set `currentScreen = LOADING` |
| **Load (index 1)** | **86-101** | **GetTopSlot → LoadMeta → pendingMapPath → isSwitchingMap → LOADING** |
| Options (index 2) | 79-81 | Set return screen, switch ke OPTIONS |
| Quit (index 3) | 83-85 | CloseWindow |

### 3g. Pause Menu

| Aksi | Line | Fungsi |
|------|------|--------|
| Save | `src/ui/pauseMenu.cpp:362` | `SaveRuntimeState()` + `SaveGameState()` |
| Return to Menu | `src/ui/pauseMenu.cpp:374-376` | Save runtime + game state → MAIN_MENU |
| Close Game | `src/ui/pauseMenu.cpp:385-387` | Save runtime + game state → CloseWindow |

---

## 4. Runtime State (Save/Load Detail)

### Data yang Disimpan di `runtime.json`

Key per stage: `"stage_0"`, `"stage_1"`, dll.

| Field | Type | Deskripsi | Dependencies |
|-------|------|-----------|--------------|
| `chests` | `string[]` | Posisi chest yang sudah di-loot (`"x_y"`) | `ChestManager` |
| `crates` | `string[]` | Posisi crate yang sudah hancur | `CrateManager` |
| `bombs` | `string[]` | Posisi bomb yang sudah meledak | `BombManager` |
| `deadEnemies` | `string[]` | ID `"mapPath_objId"` — enemy mati | `DeadEntities::set` |
| `itemDrops` | `object[]` | `{defId, amount, x, y}` — item di lantai | `ItemDataManager` |
| `barrier` | `{cleared, hasReLocked}` | State barrier manager | `BarrierManager` |

### Save Flow (`SaveRuntimeState`)

**`src/map/worldgenio.cpp:193-252`**
```
1. Baca runtime.json yang sudah ada (atau bikin baru)
2. Buat entry "stage_{index}" berisi state terkini
3. Tulis ulang runtime.json
```

### Load Flow (`LoadRuntimeState`)

**`src/map/worldgenio.cpp:258-304`**
```
1. Buka runtime.json
2. Cari entry "stage_{index}"
3. Restore ke masing-masing manager:
   a. chestManager.SetConsumed(chests)
   b. crateManager.SetDestroyed(crates)
   c. bombManager.SetExploded(bombs)
   d. Entities::SetDeadEntries(deadEnemies)
   e. itemData.SetItemDrops(itemDrops)
   f. barrierManager.SetCleared() / SetHasReLocked()
```

### Meta Save (`SaveMeta`)

**`src/core/seedmanager.cpp:30-42`**
```
File: meta.json
{
    "seeds": [u32, u32, u32, u32, u32],
    "currentStage": int,
    "prevStage": int,
    "currentSlot": int
}
```

---

## 5. Bug: Load Game Crash

### Lokasi

**`src/ui/mainMenu.cpp:86-101`** — Button handler Load Game
**`src/core/loading_screen.cpp:85-174`** — Map switch path

### Root Cause

Saat "Load Game" diklik dari main menu:

1. `mainMenu.cpp:94` — Set `pendingMapPath` dan `isSwitchingMap = true`
2. `main.cpp:77-81` — `enteredLoading == false` → panggil `InitLoadingScreen()`
3. `loading_screen.cpp:85` — Deteksi `isSwitchingMap == true` → **masuk map switch path**
4. `loading_screen.cpp:99-124` — Stage 1: LoadMap + RunWorldgen **TANPA InitTextures() dulu**
5. **`loading_screen.cpp:129`** — `PlayerInstance.Init()` → akses `loadedAnimationSets["knight"]` yang **belum ada** (textures never loaded)

**Masalah:** Path `isSwitchingMap` (line 85) mengambil alih sebelum `assetsLoaded` check (line 180). `InitTextures()` yang hanya dipanggil di first-time path (line 212) **tidak pernah dijalankan**. Akibatnya:
- `loadedAnimationSets["knight"]` default-constructed → pointer ke data kosong
- Saat `Player::Render()` → `DrawAnimation(Anim, tint)` → akses animSet kosong → **CRASH**

### Crash Trace

```
mainMenu.cpp:94   → isSwitchingMap = true, currentScreen = LOADING
loading_screen.cpp:45  → InitLoadingScreen()
loading_screen.cpp:85  → masuk map switch path (BUKAN first-time)
loading_screen.cpp:101 → LoadMap() OK (file exists)
loading_screen.cpp:111 → RunWorldgen() OK
loading_screen.cpp:129 → PlayerInstance.Init() → loadedAnimationSets["knight"] CRASH
                       → Texture/AnimationSet tidak pernah di-load
```

### Files Terkait

| File | Line | Fungsi |
|------|------|--------|
| `src/ui/mainMenu.cpp` | 86-101 | `Load Game` button handler |
| `src/core/loading_screen.cpp` | 85 | `if (isSwitchingMap || isGoingBack)` — priority check |
| `src/core/loading_screen.cpp` | 212 | `InitTextures()` — hanya di first-time path |
| `src/core/main.cpp` | 77-81 | LOADING state handler |
| `src/entities/player.cpp` | 23-87 | `Player::Init()` — akses loadedAnimationSets |
| `include/core/screen.h` | 54-77 | GameState struct definition |

### Fix yang Dibutuhkan

1. **Opsi A**: Di `loading_screen.cpp:85-174` — sebelum stage 1 untuk `isSwitchingMap`, panggil `InitTextures()` jika `!assetsLoaded`.
2. **Opsi B**: Di `loading_screen.cpp:85` — tambah check: jika `!assetsLoaded` dan `isSwitchingMap`, jalankan first-time path dulu (stage 0–3) baru lanjut map switch stages.
3. **Opsi C**: Di `mainMenu.cpp:86-101` — set `assetsLoaded = false` dan bypass `isSwitchingMap`, langsung panggil first-time loading seperti Start Game tapi dengan `pendingMapPath` override.
