# World Generation — Save/Load Pipeline

## 1. Arsitektur Global

### Konstanta & Path

| Simbol | Nilai | Lokasi |
| --- | --- | --- |
| `SEED_COUNT` | 5 | `include/core/seedmanager.h:22` |
| `WORLDSEED_DIR` | `assets/maps/World_generation/worldseed` | `src/map/worldgenio.cpp:25` |
| `BG_MAP` | `assets/maps/World_generation/background_map.json` | `src/map/worldgenio.cpp:26` |

### Struktur Folder Save

```txt
assets/maps/World_generation/worldseed/
  save_1/
    meta.json               <- seeds, currentStage, prevStage, currentSlot
    maps/
      stage_1.json          <- generated map (copy of BG_MAP)
      stage_2.json
      ...
  save_2/
    ...
```

Runtime state (checkpoints, snapshot) tidak lagi disimpan di folder worldseed. Semua runtime state dikelola oleh **SaveManager** di folder `saves/slot_N/checkpoints/` dan `saves/slot_N/manual/`. Lihat [save-system.md](./save-system.md) untuk detail.

### Data Structures Kunci

| Variabel | Type | Definisi | Isi |
| --- | --- | --- | --- |
| `g_SeedManager` | `SeedManager` | `src/core/seedmanager.cpp:14` | seeds[5], currentStage, prevStage, currentSlot, isRunActive |
| `gState` | `GameState *` | `src/core/screen_handler.cpp:59` | currentScreen, loading flags, map switch flags |
| `currentMapPath` | `std::string` (static) | `src/map/map.cpp:54` | path ke map yang sedang aktif |
| `tilesonMap` | `TilesonMapData *` | `src/map/map.cpp:36` | data tileson map yang sedang di-load |
| `barrierManager` | `BarrierManager` | `src/map/propsbehavior.cpp:1243` | barrier state per stage |

---

## 2. Pipeline: Start → Worldgen → Gameplay

### Flow Diagram

```txt
MAIN_MENU → "Start" → LOADING → HandleInitialLoad (assetsLoaded=false)
                                  stage 0: InitTextures()
                                  stage 1: InitMap() → main_hub.json
                                           atau LoadMap(savedMap) + LoadWorldgenForSave()
                                  stage 2: (count increment)
                                  default: InitAll() + PLAY

MAIN_MENU → "Load"  → SAVE_LOAD → slot dipilih → LOADING → HandleInitialLoad
                                  (sama seperti Start tapi dengan savedMapState terisi)

Door trigger → NextStage() / PrevStage()
           → SwitchMap() → LOADING → HandleMapSwitch (isSwitchingMap=true)
                                  stage 0: UnloadMap()
                                  stage 1: LoadMap() + RunWorldgen() + ApplyPreSpawn()
                                  stage 2: Player Init + Entities + SpawnEnemies + SpawnItemWave
                                  stage 3: Camera + Autosave + PLAY
```

### 2a. Fresh Start (Start Game)

Button index 0 di main menu:

**`src/ui/mainMenu.cpp:105-113`**

```cpp
case 0:  // Start Game
    SetActiveSlot(0);
    ResetMemoryState();
    WorldgenIO::CleanupOrphanedSlots();  // GC: hapus worldseed slot yg tidak terpakai
    Entities::ClearDeadEntities();
    state->enteredLoading = false;
    state->currentScreen = LOADING;
    break;
```

Loading screen mendeteksi `isSwitchingMap = false` dan `assetsLoaded = false`, masuk ke **HandleInitialLoad**:

**`src/core/loading_screen.cpp:352-463`**

| Stage | Fungsi | File:Line |
| --- | --- | --- |
| 0 | `InitTextures()` — load sprites, atlas, tileset textures | `loading_screen.cpp:360` |
| 1 | `HasSavedState()` → `LoadMap(savedMap)` + `LoadWorldgenForSave()` / `InitMap()` | `loading_screen.cpp:366-398` |
| 2 | Increment stage (kosong) | `loading_screen.cpp:400` |
| default | `assetsLoaded = true`, `InitAll()`, `ApplyPreSpawn()`, `RestoreGameState()`, `InitMainMenu()`, `currentScreen = PLAY` | `loading_screen.cpp:406-461` |

**Tidak ada worldgen di path ini.** Map yang di-load adalah `assets/maps/main_hub.json` (melalui `InitMap()` di `map.cpp:280-285`), bukan `tutorial.json`.

Jika save game ditemukan (`HasSavedState()` true), maka `LoadMap(savedMap)` dipanggil dan untuk worldgen saves akan memanggil `LoadWorldgenForSave()` untuk restore seeds + regenerate world.

### 2b. Worldgen Stage Entry (door trigger) — Boss Stage

Saat player mencapai stage terakhir (boss) dan menyelesaikannya:

**`src/map/worldgenio.cpp:186-188`** — setelah boss dikalahkan:
```cpp
InputInstance.ResetMenuFlags();
g_SeedManager.ResetRun();
InitMainMenu(gState);        // <-- baru: re-init main menu state
gState->currentScreen = MAIN_MENU;
return;
```

`InitMainMenu(gState)` memastikan state main menu di-reset sebelum kembali — mencegah stale state dari sesi sebelumnya.

Saat player memasuki door yang terhubung ke stage worldgen, fungsi `NextStage()` atau `PrevStage()` dipanggil:

**`src/map/worldgenio.cpp:178-200`** — `WorldgenIO::NextStage()`

```txt
1. Cek oldStage >= SEED_COUNT - 1 (boss stage):
     → ResetRun(), currentScreen = MAIN_MENU (return ke lobby)
2. g_SeedManager.NextStage()             — increment stage
3. SaveMeta()                            — simpan seeds + stage ke disk
4. GetStagePath(newStage) → SwitchMap(stagePath, "start")
5. TrimStageStack()                      — sisakan hanya prev stage di history stack
```

**`src/map/worldgenio.cpp:203-220`** — `WorldgenIO::PrevStage()`

```txt
1. Cek CanGoBack()                       — false jika prevStage < 0
2. GoBackStage() → targetStage           — ambil prevStage, reset ke -1 (one-time use)
3. SetCurrentStage(targetStage)          — set stage ke target
4. SaveMeta()
5. GetStagePath(targetStage) → SwitchMap(stagePath, "finish")
6. TrimStageStack()
```

`SwitchMap()` di **`src/map/map.cpp:460-492`**:

```txt
1. Null/empty path guard
2. CaptureSnapshot() + SaveCheckpoint()  — simpan state map lama via SaveManager
3. Push current map ke history stack
4. Set gState->isSwitchingMap = true
5. Set pendingMapPath / pendingDoorName
6. Reset enteredLoading, loadingStage=0, loadingProgress=0, loadingComplete=false
7. loadingText = "Switching map..."
8. currentScreen = LOADING
```

Perbedaan dari versi lama: state map tidak lagi disimpan via `SaveEnemiesForMap()` / `SaveItemsForMap()`. Semua state ditangani oleh `SaveManager::CaptureSnapshot()` + `SaveCheckpoint()`.

### 2c. Map Switch Loading (worldgen entry / load game)

Loading screen mendeteksi `isSwitchingMap == true` dan masuk ke **HandleMapSwitch**:

**`src/core/loading_screen.cpp:132-264`**

| Stage | Fungsi | Detail |
| --- | --- | --- |
| 0 | `UnloadMap()` + `spawnFlowFields.clear()` | `loading_screen.cpp:138-147` |
| 1 | `LoadMap(path)` + `SetCurrentMapPath()` | `loading_screen.cpp:149-197` |

Stage 1 — worldgen trigger + pre-spawn restore (`loading_screen.cpp:158-177`):

```cpp
if (state->pendingMapPath.find("worldseed/save_") != std::string::npos)
{
    int stageIdx = g_SeedManager.GetCurrentStage();
    uint64_t seed = g_SeedManager.GetSeed(stageIdx);
    RunWorldgen(seed, stageIdx == SeedManager::SEED_COUNT - 1);

    // Reset barrier state — jangan bawa stale state dari map sebelumnya
    barrierManager.SetCleared(false);
    barrierManager.SetHasReLocked(false);

    // Apply checkpoint pre-spawn SEBELUM SpawnObject
    // agar chest/bomb/crate yang sudah dikonsumsi tidak spawn ulang
    {
        GameSnapshot chkSnap;
        if (SaveManager::HasCheckpoint(state->pendingMapPath, -1))
        {
            chkSnap = SaveManager::LoadCheckpoint(state->pendingMapPath, -1);
            SaveManager::ApplyPreSpawn(chkSnap);
        }
    }
}
```

| Stage | Fungsi | Detail |
| --- | --- | --- |
| 2 | `PlayerInstance.Init()` + `Entities::Clear()` + `Add(Player)` | `loading_screen.cpp:199-238` |
| | `SpawnEnemiesFromMap()` + `SpawnItemWave()` + `ApplyCheckpointData()` | |
| | `CaptureSnapshot()` + `SaveInitial()` | |
| 3 | Camera setup + clear switch flags + `SaveAutosave()` + `currentScreen = PLAY` | `loading_screen.cpp:240-263` |

### 2d. Fast Path (assets already loaded)

Saat player kembali dari main menu atau OPTIONS tanpa perlu reload assets (`assetsLoaded = true`, `!isSwitchingMap`):

**`src/core/loading_screen.cpp:268-348`** — `HandleFastPath()`

```txt
1. loadingStage = max, loadingComplete = true, currentScreen = PLAY
2. UnloadMap()
3. Jika HasSavedState():
     LoadMap(savedMapPath) + SetCurrentMapPath()
     Jika worldseed path → LoadWorldgenForSave()
     Jika tidak → InitMap()
4. Jika !HasSavedState():
     PlayerInstance.ResetForNewGame()
     InitMap()
     Clear workspace
5. ApplyPreSpawn() dari manual save
6. InitAll() + RestoreGameState()
7. Entities::PruneDeadEntities()
8. SaveInitial() + CaptureInitialSnapshot()
9. Jika HasSavedState() → MirrorToWorkspace()
10. InitMainMenu()
```

---

## 3. Fungsi-Fungsi Kunci

### 3a. SeedManager (`src/core/seedmanager.cpp`)

| Fungsi | Line | Deskripsi |
| --- | --- | --- |
| `g_SeedManager` | 14 | Instance global SeedManager |
| `InitRun(saveSlot)` | 20-31 | Generate SEED_COUNT random seeds, set currentStage=0, slot=saveSlot, isRunActive=true |
| `GetSeed(stage)` | 35-40 | Return seed untuk stage tertentu (0 jika out of range) |
| `NextStage()` | (seedmanager.h:50-57) | Increment currentStage, simpan prevStage |
| `GoBackStage()` | (seedmanager.h:63-68) | Return prevStage dan reset ke -1, satu kali pakai |
| `CanGoBack()` | (seedmanager.h:60) | Return prevStage >= 0 |
| `ResetRun()` | (seedmanager.h:74-79) | Set isRunActive=false, currentStage=0, prevStage=-1 |
| `SaveMeta(path)` | 43-56 | Simpan seeds[], currentStage, prevStage, currentSlot ke JSON |
| `LoadMeta(path)` | 59-86 | Load dari JSON dan restore state |

### 3b. WorldgenIO (`src/map/worldgenio.cpp`)

| Fungsi | Line | Deskripsi |
| --- | --- | --- |
| `GetMetaPath(slot)` | 40 | Return `worldseed/save_{slot}/meta.json` |
| `GetStagePath(idx)` | 51 | Return `worldseed/save_{slot}/maps/stage_{idx+1}.json` |
| `GetNextAvailableSlot()` | 62 | Scan folder `save_*`, return max+1 |
| `GetTopSlot()` | 85 | Scan folder `save_*`, return max (untuk Load) |
| `ClearCache()` | 113 | Hapus file `.cache` dari `saves/cache/enemies` dan `saves/cache/items` |
| `InitRun(slot)` | 130 | ClearCache, generate worldgen, buat folder slot, copy BG map per stage, fix texture paths, save meta |
| `NextStage()` | 178 | Boss-check → increment stage → SaveMeta → SwitchMap → TrimStageStack |
| `PrevStage()` | 203 | CanGoBack → GoBackStage → SetCurrentStage → SaveMeta → SwitchMap("finish") → TrimStageStack |
| `CleanupOrphanedSlots()` | 240 | Hapus worldseed `save_N/` yang tidak direferensi oleh save manual manapun (hanya scan new format `manual/snapshot.json` via `SaveManager::GetManualPath()`) |

**Catatan:** `HandleMapSwitch` dan `LoadRuntimeState` sudah tidak ada. Map switch handling sekarang ada di `loading_screen.cpp:132` (`HandleMapSwitch`). Runtime state dikelola oleh SaveManager.

### 3c. Map Operations (`src/map/map.cpp`)

| Fungsi | Line | Deskripsi |
| --- | --- | --- |
| `InitMap()` | 280-285 | Load `assets/maps/main_hub.json` sebagai map awal |
| `RunWorldgen(seed, isBoss)` | 288-331 | Generate world pake seed, stamp layout, spawn exit doors |
| `SwitchMap(path, door)` | 460-492 | CaptureSnapshot + SaveCheckpoint, set pending switch, trigger LOADING screen |
| `TrimStageStack()` | 501-520 | Sisakan cuma entry teratas di stack riwayat |
| `GetCurrentMapPath()` | 526-529 | Return currentMapPath (static string) |
| `SetCurrentMapPath(path)` | 535-538 | Update currentMapPath |

**Perubahan:** `GoBack()` (dahulu di line 496-524) sudah dihapus. Navigasi mundur stage sekarang menggunakan `GoBackStage()` di SeedManager dan `PrevStage()` di WorldgenIO.

### 3d. Loading Screen (`src/core/loading_screen.cpp`)

| Fungsi | Line | Deskripsi |
| --- | --- | --- |
| `InitLoadingScreen(state)` | 51-70 | Reset loadingStage/Progress, set text sesuai mode (map-switch / fast-path / initial) |
| `ExtractStageFromPath(mapPath)` | 77-90 | Ekstrak index stage (0-based) dari map path worldgen |
| `LoadWorldgenForSave(mapPath, slot)` | 96-128 | LoadMeta + extract stage + RunWorldgen + itemDefs.Load |
| `HandleMapSwitch(state)` | 132-264 | Eksekusi 4-stage map switch (Unload → Load+Worldgen → Player/Entities → Finalize) |
| `HandleFastPath(state)` | 268-348 | Eksekusi fast path saat assets sudah ter-load (kembali dari menu) |
| `HandleInitialLoad(state)` | 352-463 | Eksekusi 3-stage initial load (InitTextures → Map → InitAll) |
| `UpdateLoadingScreen(state)` | 475-495 | Dispatcher — pilih mode berdasarkan flag (isSwitchingMap / assetsLoaded / initial) |
| `RenderLoadingScreen(state)` | 548-606 | Render progress bar + text + map name |
| `IsLoadingComplete(state)` | 614-617 | Return loadingComplete flag |

### 3e. Main Loop (`src/core/main.cpp`)

| Area | Line | Deskripsi |
| --- | --- | --- |
| MAIN_MENU handler | 214-222 | UpdateGame + UpdateMainMenu + RenderMainMenuToVirtualScreen |
| LOADING handler | 223-237 | InitLoadingScreen (jika enteredLoading false atau loadingComplete) → Update → Render |
| PLAY handler | 258-334 | Fixed timestep gameplay loop dengan pause menu, autosave timer (60 detik), dan death check |
| SAVE_LOAD handler | 345-361 | Display SaveLoadScreen, handle user input untuk save/load slot |
| Auto-save on exit | 365-369 | Jika run aktif: `CaptureSnapshot()` + `SaveManual()` ke slot aktif saat `CloseWindow()` |

### 3f. Main Menu (`src/ui/mainMenu.cpp`)

| Button | Line | Aksi |
| --- | --- | --- |
| Start (index 0) | 105-113 | `SetActiveSlot(0)`, `ResetMemoryState()`, `CleanupOrphanedSlots()`, `ClearDeadEntities()`, set `enteredLoading=false`, `currentScreen = LOADING` |
| **Load (index 1)** | **114-118** | **Set `previousScreen = MAIN_MENU`, buka `SaveLoadScreen` dalam `LOAD_MODE`, `currentScreen = SAVE_LOAD`** |
| Options (index 2) | 119-122 | Set `previousScreen`, switch ke OPTIONS |
| Quit (index 3) | 123-125 | CloseWindow |

**Perubahan Load button:** Tidak lagi langsung `GetTopSlot → LoadMeta → pendingMapPath → LOADING`. Sekarang membuka SaveLoadScreen agar user bisa memilih slot. SaveLoadScreen yang akan mengatur state dan transisi ke LOADING.

### 3g. Pause Menu (`src/ui/pauseMenu.cpp`)

Semua tombol pause menu ditangani oleh `HandleButtonClick()` di `pauseMenu.cpp:528-569`:

| Aksi | Kode | Line | Fungsi |
| --- | --- | --- | --- |
| Resume | case 0 | 532-534 | `Hide()` |
| Save | case 1 | 535-541 | Buka SaveLoadScreen dalam mode SAVE_MODE |
| Load | case 2 | 543-548 | Buka SaveLoadScreen dalam mode LOAD_MODE |
| Settings | case 3 | 549-553 | Switch ke OPTIONS |
| Restart | case 4 | 554-557 | Tampilkan popup konfirmasi restart |
| Return to Menu | case 5 | 558-561 | Tampilkan popup konfirmasi → `CleanupOrphanedSlots()` → `MAIN_MENU` |
| Close Game | case 6 | 562-566 | `SaveGameState()` + `CleanupOrphanedSlots()` + `CloseWindow()` |

**Detail Return to Menu** (`mainMenu.cpp:634-648`):
```txt
1. Popup "Return to main menu?"
2. Konfirmasi → ResetMenuFlags, enteredLoading=false, loadingStage=0, loadingProgress=0, loadingComplete=false
3. CleanupOrphanedSlots()
4. currentScreen = MAIN_MENU
5. Hide()
```

---

## 4. Runtime State (Save/Load Detail)

Runtime state dunia (chests, crates, enemies, items, barrier) di-handle oleh **SaveManager** melalui snapshot/checkpoint system. Tidak ada lagi `runtime.json` per-stage.

### Snapshot System (SaveManager)

Runtime state disimpan sebagai *checkpoints* dalam `GameSnapshot`:

| Komponen | Cara Restore |
| --- | --- |
| Chest yang sudah di-loot | `SaveManager::ApplyPreSpawn(snap)` — restore chest states sebelum spawn |
| Crate hancur | Sama, dalam `ApplyPreSpawn()` |
| Enemy mati | `Entities::SetDeadEntries()` via `ApplyPreSpawn()` |
| Item drops di lantai | `ItemData::SetItemDrops()` via `ApplyPreSpawn()` |
| Barrier state | `BarrierManager::SetCleared()` / `SetHasReLocked()` via `ApplyPreSpawn()` |
| Player posisi/inventory | `ApplyPostSpawn(snap)` — setelah enemy/item spawn |

### Alur Map Switch (checkpoint)

```txt
1. SwitchMap() dipanggil
2. SaveManager::CaptureSnapshot() — ambil snapshot state saat ini
3. SaveManager::SaveCheckpoint(snap, path, -1) — simpan ke slot runtime
4. Push current map ke history stack
5. Set LOADING screen dengan pendingMapPath
6. HandleMapSwitch stage 1:
     a. LoadMap(path) + RunWorldgen() (jika worldseed)
     b. ApplyPreSpawn(chkSnap) — restore consumed state SEBELUM SpawnObject
     c. SpawnObject() + RebuildObstacleCache()
7. HandleMapSwitch stage 2:
     a. ApplyPreSpawn(chkSnap) — restore dead entities
     b. SpawnEnemiesFromMap() + SpawnItemWave()
     c. ApplyCheckpointData(chkSnap) — restore player/enemy/item state
     d. SaveInitial() untuk restart
8. HandleMapSwitch stage 3:
     a. Camera setup
     b. SaveAutosave()
     c. currentScreen = PLAY
```

### Alur Save (via SaveLoadScreen)

```txt
1. Pause Menu → Save → SaveLoadScreen (mode SAVE_MODE)
2. User pilih slot, konfirmasi
3. SaveManager: collect state dari semua manager → simpan ke file save slot
4. Meta otomatis disimpan oleh SeedManager via SaveMeta()
```

### Alur Load (via SaveLoadScreen)

```txt
1. Main Menu / Pause Menu → Load → SaveLoadScreen (mode LOAD_MODE)
2. User pilih slot, konfirmasi
3. ReadSaveFile() → Parse JSON → Isi savedMapState + savedPlayerState
4. currentScreen = LOADING
5. HandleInitialLoad / HandleFastPath:
     a. LoadMap(path) + SetCurrentMapPath()
     b. Jika worldseed → LoadWorldgenForSave() (LoadMeta + RunWorldgen)
     c. ApplyPreSpawn() dari manual save
     d. InitAll() + RestoreGameState()
```

### Meta Save (`SaveMeta`)

**`src/core/seedmanager.cpp:43-56`**

```json
File: meta.json
{
    "seeds": [u32, u32, u32, u32, u32],
    "currentStage": int,
    "prevStage": int,
    "currentSlot": int
}
```

Meta file disimpan di `worldseed/save_{slot}/meta.json`. Tidak menyimpan runtime state — hanya seeds dan stage tracking.

---

## 5. Bug Analysis

### Bug Historis: Crash saat "Load Game" dari Main Menu (sudah diperbaiki)

Versi lama dari loading screen memiliki bug dimana path `isSwitchingMap` (map switch) mengambil alih sebelum `assetsLoaded` check. Saat "Load Game" diklik dari main menu, `isSwitchingMap = true` menyebabkan loading screen masuk ke map switch path **tanpa** memanggil `InitTextures()` terlebih dahulu. Akibatnya `PlayerInstance.Init()` crash karena `loadedAnimationSets["knight"]` belum di-load.

### Resolusi di Arsitektur Saat Ini

Restrukturasi loading screen menjadi tiga mode terpisah (HandleInitialLoad, HandleFastPath, HandleMapSwitch) menyelesaikan masalah ini:

1. **Start Game** → `HandleInitialLoad` → selalu panggil `InitTextures()` di stage 0 sebelum apapun
2. **Load Game** → `SaveLoadScreen` → `HandleInitialLoad` → sama, `InitTextures()` dipanggil pertama
3. **Map Switch** (in-game, door trigger) → `HandleMapSwitch` → hanya jalan setelah `assetsLoaded = true` (sudah pernah melalui HandleInitialLoad)

### Crash Guard: Worldseed Directory Terhapus

**`src/core/loading_screen.cpp:407-416`**

Jika `LoadMap()` gagal karena worldseed directory tidak ada (corrupted save), `tilesonMap` tetap null. HandleInitialLoad mendeteksi ini dan menampilkan pesan error lalu kembali ke MAIN_MENU:

```cpp
if (tilesonMap == nullptr)
{
    state->loadingText = "Load failed -- corrupted save, returning to menu...";
    state->loadingComplete = true;
    state->assetsLoaded = true;
    state->currentScreen = MAIN_MENU;
    InitMainMenu(state);
    break;
}
```

### Files Terkait (Arsitektur Saat Ini)

| File | Line | Fungsi |
| --- | --- | --- |
| `src/ui/mainMenu.cpp` | 114-118 | `Load Game` button — buka SaveLoadScreen |
| `src/core/loading_screen.cpp` | 352-463 | `HandleInitialLoad` — initial loading + crash guard |
| `src/core/loading_screen.cpp` | 132-264 | `HandleMapSwitch` — map switch loading |
| `src/core/loading_screen.cpp` | 360 | `InitTextures()` — dipanggil di stage 0 initial load |
| `src/core/main.cpp` | 223-237 | LOADING state handler — re-init jika selesai |
| `src/entities/player.cpp` | 23-87 | `Player::Init()` — akses loadedAnimationSets |
| `include/core/screen.h` | 59-84 | GameState struct definition |
