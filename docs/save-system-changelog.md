# Save System Changelog — Restart + Cache + Bug Fix

> Catatan perubahan sistem save, cache, dan restart dari sesi integrasi unit test & bug fixing.
> Lengkap dengan pipeline, bug yang diperbaiki, dan concern untuk developer selanjutnya.

---

## Daftar Isi

1. [Pipeline Restart](#pipeline-restart)
2. [Pipeline Save/Load Worldgen (vs Non-Worldgen)](#pipeline-saveload-worldgen-vs-non-worldgen)
3. [Bugs Fixed](#bugs-fixed)
4. [Perubahan yang Ditambahkan](#perubahan-yang-ditambahkan)
5. [Concern / Catatan untuk Developer](#concern--catatan-untuk-developer)

---

## Pipeline Restart

```
Pause Menu → Tombol Restart
  │
  ├─ [1] Clear runtime state
  │   (Entities::Clear, itemData, ClearTileProps, DeadEntities,
  │    chestManager, spikeManager, bombManager, crateManager, barrierManager)
  │
  ├─ [2] SpawnEnemiesFromMap()
  │   ├─ Worldgen:       spawn dari RNG + seed → bisa beda tiap spawn
  │   └─ Non-worldgen:   spawn dari JSON statis map → selalu sama
  │
  ├─ [3] Load cache (.cache) — overlay state di atas hasil spawn
  │   ├─ Ada:  restore enemy & item ke kondisi saat capture
  │   └─ Gak:  fallback SpawnItemWave() (spawn item fresh)
  │
  ├─ [4] Reset player
  │   ├─ ResetForNewGame() + Init(state, SPAWN_OBJECT_NAME)
  │   ├─ hasDroppedItems = false
  │   └─ Camera reset ke posisi player
  │
  ├─ [5] Re-init dunia
  │   ├─ SpawnObject()
  │   ├─ RebuildObstacleCache()
  │   └─ globalFlowField.Invalidate()
  │
  ├─ [6] Re-capture cache (biar restart berikutnya pake state segar)
  │
  └─ PLAY
```

### Bedanya Worldgen vs Non-Worldgen

| Aspek | Worldgen | Non-worldgen |
|---|---|---|
| **Sumber spawn enemy** | RNG dari seed → bisa beda tiap spawn | JSON statis map → selalu sama |
| **Urgensi cache** | ✅ Wajib — biar restart deterministik | ❌ Opsional — spawn dari JSON selalu sama |
| **Map layout** | Tetap (worldseed hasil RunWorldgen) | Tetap (loaded dari Tiled JSON) |
| **Fallback kalo .cache gak ada** | Musuh/item bisa beda tiap restart | Musuh/item selalu sama |

### Kenapa Cache Diperlukan

Tanpa cache, restart di worldgen bakal spawn enemy/item beda karena RNG ulang. Ini bisa dieksploitasi — player restart berkali-kali sampe dapet layout musuh yang enak. Cache menyimpan snapshot enemy + item pas pertama kali masuk map (setelah InitAll), dan merestore-nya pas restart.

---

## Pipeline Save/Load (Worldgen vs Non-Worldgen)

### Save (Pause → Save / Return to Menu)

```
SaveGameState()
  ├─ Baca player state → savedPlayerState
  ├─ Baca enemy registry → savedEnemyStates
  ├─ Baca active items → savedItemStates
  ├─ Baca map state → savedMapState (path, deadEntities, chestsOpened, dll)
  │
  ├─ Worldgen: WorldgenIO::SaveRuntimeState(currentStage)
  │   → simpan chests, crates, bombs, deadEnemies, itemDrops, barrier
  │     ke worldseed/save_N/runtime.json
  │
  └─ WriteSaveFile("saves/manual/slot0.json")
```

### Load (Main Menu → Load Game)

```txt
Fast path (assets udah diload sebelumnya):
  │
  ├─ UnloadMap()
  ├─ LoadMap(savedMapState.mapPath)
  ├─ BuildMapObjectIndex()
  │
  ├─ Worldgen?  →  RunWorldgen(seed, isBoss) + LoadRuntimeState(stageIdx)
  │               ↗  regenerate layout + restore runtime (chests, dead, dll)
  │
  ├─ SetWorldgenPending() — guard biar RestoreDeadEntities di-skip
  │
  ├─ RestoreDeadEntities() — skip kalo worldgen
  ├─ InitAll() — spawn enemies, items, cache
  ├─ RestoreGameState() — overwrite player posisi, HP, inventory
  ├─ PruneDeadEntities() — safety net
  │
  └─ PLAY
```

Normal path (map switch):
```txt
  Stage 1: LoadMap + RunWorldgen (kalo worldgen) + LoadRuntimeState
  Stage 2: Init player, LoadEnemiesForMap/LoadItemsForMap (dari .json save per-map)
           → capture cache (.cache)
  Stage 3: WriteAutosave + PLAY
```

---

## Bugs Fixed

### Bug #1 — ClearCache() Hapus Semua File

| Item | Detail |
|---|---|
| **Lokasi** | `src/map/worldgenio.cpp:110-122` — `WorldgenIO::ClearCache()` |
| **Gejala** | `ClearCache()` di `InitRun()` hapus SEMUA file di `saves/enemies/` dan `saves/items/` (termasuk file `.json` save per-map), bukan cuma file `.cache` |
| **Akibat** | Save state enemy/item per-map ilang → pas load game, musuh/item spawn fresh dari awal |
| **Fix** | Filter dengan `entry.path().extension() == ".cache"` — cuma delete file yang berakhiran `.cache` |
| **Commit** | Sesi ini |

### Bug #2 — Layout Prefab Hilang Pas Load Game Worldgen

| Item | Detail |
|---|---|
| **Lokasi** | `src/core/loading_screen.cpp` — fast path (line ~230) |
| **Gejala** | Load game mid-worldgen lewat fast path → layout prefab (rooms, walls, doors) ilang, player TP di tengah map kosong |
| **Akar** | Fast path cuma `LoadMap()` + `BuildMapObjectIndex()`, gak manggil `RunWorldgen()` — map cuma `background_map.json` flat |
| **Fix** | Tambah blok `RunWorldgen(seed, isBoss)` + `WorldgenIO::LoadRuntimeState(stageIdx)` di fast path, persis kayak normal switch path |
| **Commit** | Sesi ini |

### Bug #3 — Crash Worldgen Run Ke-2

| Item | Detail |
|---|---|
| **Lokasi** | `src/systems/interaction.cpp:102` + `src/core/game_state_saver.cpp:838` |
| **Gejala** | Main menu → New Game → worldgen run 1 sukses → main menu lagi → New Game → worldgen run 2 crash |
| **Akar** | `ClearSavedState()` hapus worldseed folder `save_N`, tapi `SeedManager::isRunActive` masih `true`. Pas run ke-2 di `interaction.cpp:102` ngecek `if (!IsRunActive())` → `false` (skip `InitRun()`) → pake slot lama yang udah didelete → crash |
| **Fix** | `g_SeedManager.ResetRun()` di `ClearSavedState()` — reset `isRunActive = false`, `currentStage = 0`, `prevStage = -1` |
| **Commit** | Sesi ini |

### Bug #4 — Cache Basi Setelah Load Game / Restart

| Item | Detail |
|---|---|
| **Lokasi** | `src/core/loading_screen.cpp` fast path, `src/ui/pauseMenu.cpp` restart flow |
| **Gejala** | Setelah load game (fast path) atau restart, file `.cache` masih pake snapshot dari sesi sebelumnya, bukan kondisi terkini |
| **Akibat** | Restart kedua atau ketiga restore state yang salah |
| **Fix** | Re-capture `.cache` di 2 tempat: (1) akhir fast path setelah `PruneDeadEntities()`, (2) akhir restart flow setelah `RebuildObstacleCache()` |
| **Commit** | Sesi ini |

### Bug #5 — WinMain Infinite Loop (Unit Test)

| Item | Detail |
|---|---|
| **Lokasi** | `tests/constants_test.cpp` |
| **Gejala** | Stack overflow (`0xC00000FD`) pas jalan `test_constants.exe` |
| **Akar** | MinGW-UCRT CRT punya `main()` wrapper yang panggil `WinMain()`. `WinMain` kita manggil `main()` lagi → infinite loop |
| **Fix** | Panggil `doctest::Context::run()` langsung dari `WinMain`, tanpa lewat `main()` |

---

## Perubahan yang Ditambahkan

### File Baru

| File | Deskripsi |
|---|---|
| `lib/doctest/doctest.h` | Unit test framework v2.4.11, header-only |
| `tests/constants_test.cpp` | ~100 test cases, 135 assertions untuk compile-time constants |
| `.github/workflows/build.yml` | CI/CD: build + test di Windows & Linux |
| `setup.ps1` | Tambah fungsi `Install-Doctest` |

### File Diubah

| File | Perubahan |
|---|---|
| `src/map/worldgenio.cpp:110-122` | `ClearCache()` — filter `.cache` aja |
| `src/core/loading_screen.cpp` | Fast path: +`RunWorldgen()` + re-capture cache |
| `src/ui/pauseMenu.cpp` | Restart flow: +re-capture cache |
| `src/core/game_state_saver.cpp:861` | `ClearSavedState()`: +`g_SeedManager.ResetRun()` |
| `include/map/propsbehavior.h` | Constants `SpikeManager`, `BombManager`, `CrateManager`, `BarrierManager` jadi `public:` |
| `CMakeLists.txt` | Target `test_constants` (Unity Build OFF) |
| `src/map/worldgenio.h:71` | Doxygen comment diperjelas: "hapus hanya file .cache" |

### Constants yang Di-test (135 assertions, 39 test cases)

| Kategori | Jumlah |
|---|---|
| Player constants | 11 |
| Combat constants | 18 |
| Enemy constants | 15 |
| Item constants | 19 |
| Map constants | 23 |
| Prop constants | 19 |
| Flow field / AI | 10 |
| Seed / Save | 3 |
| Misc (screen, physics) | 17 |

---

## Concern / Catatan untuk Developer

### 1. Cancel Load Hapus Worldseed (mainMenu.cpp:176)

`ClearSavedState()` dipanggil saat user cancel popup Load Game — otomatis hapus SEMUA `worldseed/save_*`. Efeknya: kalo user iseng klik Load terus cancel, worldgen run yang udah berjam-jam ilang.

**Rekomendasi:** Pisahin `ClearSavedState()` jadi 2 fungsi:
- `ResetMemoryState()` — clear in-memory state (hasSavedState, DeadEntities, etc.), tanpa hapus worldseed
- `ResetWorldseed()` — hapus worldseed folder (panggil cuma di New Game, bukan cancel load)

### 2. Cache vs Save Separation

| Aspek | `.cache` files | `.json` save files |
|---|---|---|
| **Lokasi** | `saves/enemies/*.cache` | `saves/enemies/<path>.json` |
| **Isi** | Snapshot enemy+item pas InitAll | Persisten state per-map |
| **Dibersihin** | `ClearCache()` (filtered) | `remove_all()` di mainMenu New Game |
| **Siklus** | Dibuat pas InitAll / load / restart | Dibuat pas map switch / save |

> ⚠️ Keduanya di FOLDER YANG SAMA (`saves/enemies/` dan `saves/items/`). Hati-hati jangan sampe fungsi cleanup salah sasaran.

### 3. Single Save Slot

Saat ini manual save cuma `saves/manual/slot0.json`. Untuk multi-slot:

- `WriteSaveFile()` / `ReadSaveFile()` perlu parameter path slot
- UI pilih slot di pauseMenu + mainMenu
- Setiap slot butuh worldseed dir sendiri (`save_N`) kalo worldgen

### 4. Worldseed Multiple Slot Isolation

`ClearSavedState()` hapus SEMUA `worldseed/save_*` tanpa pandang bulu. Kalo future support multiple worldgen run, setiap run butuh:
- `worldseed/save_N/` sendiri (udah ada)
- `saves/manual/slotN.json` sendiri (belum, masih slot0 doang)
- Isolasi pas New Game: cuma hapus slot yang dipilih, bukan semua

### 5. WinMain Infinite Loop (Unit Test — MinGW-UCRT)

MinGW-UCRT versi tertentu punya CRT wrapper `main()` yang panggil `WinMain()`. Jangan panggil `main()` dari `WinMain()` — panggil `doctest::Context::run()` langsung. Detail ada di `tests/constants_test.cpp:18-28`.

### 6. Restart Flow Notes

- Restart **tidak** manggil `ClearSavedState()` — worldseed folder tetap utuh
- Restart **tidak** manggil `ClearCache()` — file `.cache` lama dihapus otomatis pas re-capture (overwrite file)
- Cache di-re-capture di AKHIR restart, jadi restart berikutnya pake state fresh
- Kalo `.cache` gak ada (misal didelete manual), fallback ke `SpawnItemWave()` — untuk worldgen, musuh tetap dari `SpawnEnemiesFromMap()` (RNG ulang)
