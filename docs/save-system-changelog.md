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

---

## Wave 1 — Data Safety Fixes (2026-06-05)

| Item | Detail |
|---|---|
| **Commit** | `9617d40` |
| **Fokus** | Memisahkan tanggung jawab `ClearSavedState()` dan memperbaiki inisialisasi variabel yang rawan undefined behavior |

### Perubahan

#### 1. Split `ClearSavedState()` Menjadi 3 Fungsi

**Masalah:** `ClearSavedState()` sebelumnya melakukan terlalu banyak hal dalam satu fungsi — mereset state memory player, camera, map, DAN menghapus worldseed folder. Ini menyebabkan crash di worldgen run ke-2 (Bug #3) dan menyulitkan kontrol granular.

**Perbaikan:** Dipisah menjadi 3 fungsi spesifik:

| Fungsi | Tanggung Jawab |
|---|---|
| `ResetPlayer()` | Reset in-memory player state (`hasSavedState`, HP, posisi, inventory, dash cooldown, mana regen timer) |
| `ResetCamera()` | Reset camera state dan target position |
| `ResetMap()` | Reset map state (`currentMapPath`, `deadEntities`, `chestsOpened`, `bombConsumedPositions`, `crateConsumedPositions`, `mapHistory`, `worldgenPending` flag) + hapus worldseed folder |

Masing-masing fungsi hanya mereset satu aspek, sehingga caller bisa memilih fungsi mana yang perlu dipanggil sesuai konteks.

**File:** `include/core/game_state_saver.h`, `src/core/game_state_saver.cpp`

#### 2. Pindah Inisialisasi Camera Cache

**Masalah:** Camera cache diinisialisasi di loading screen, tapi belum tersedia saat restart flow dimulai — menyebabkan posisi camera tidak konsisten setelah restart.

**Perbaikan:** Inisialisasi camera cache dipindah ke game init path (`screen_handler.cpp`), sehingga selalu tersedia sebelum restart flow atau load game dijalankan.

**File:** `src/core/screen_handler.cpp`

#### 3. Default `healthRegenTimer = 0.0f`

**Masalah:** `healthRegenTimer` tidak diinisialisasi secara eksplisit. Saat `ResetForNewGame()` dipanggil, timer bisa berisi nilai sisa dari sesi sebelumnya — menyebabkan regen health yang tidak terduga.

**Perbaikan:** Set default `healthRegenTimer = 0.0f` di player initialization dan di `ResetForNewGame()`.

**File:** `src/entities/player.cpp`

---

## Wave 2 — Save Format v3 + Utility Functions (2026-06-05)

| Item | Detail |
|---|---|
| **Commits** | `8e9586d`, `3def89e`, `1b01b2e` |
| **Fokus** | Upgrade save format ke v3, tambah fungsi utility untuk path routing dan display name |

### Perubahan Save Format (v2 → v3)

`SAVE_VERSION` dinaikkan dari 2 ke 3. Field baru ditambahkan ke `manual.json`:

| Field | Tipe | Deskripsi |
|---|---|---|
| `version` | int | 3 (sebelumnya 2) — schema guard, file v1/v2 langsung ditolak |
| `slotIndex` | int | Nomor slot (0-4) untuk routing multi-slot |
| `saveType` | string | "manual" atau "autosave" |
| `playTime` | float | Placeholder untuk total play time (belum diimplementasi penuh) |
| `mapDisplayName` | string | Nama map human-readable untuk preview UI SaveLoadScreen |
| `worldgenSlot` | int | Mapping slot save ke folder worldseed (worldseed/save_N/) |

### Fungsi Baru

| Fungsi | File | Header | Deskripsi |
|---|---|---|---|
| `GetActiveSlot()` | `src/core/game_state_saver.cpp` | `include/core/game_state_saver.h` | Return `g_ActiveSaveSlot` (-1 jika tidak ada slot aktif) |
| `SetActiveSlot(int slot)` | `src/core/game_state_saver.cpp` | `include/core/game_state_saver.h` | Set `g_ActiveSaveSlot` + `g_SaveSlotActive`. Slot 0-4 valid, -1 untuk nonaktifkan |
| `IsSlotActive()` | `src/core/game_state_saver.cpp` | `include/core/game_state_saver.h` | Return true jika `g_ActiveSaveSlot` di range 0-4 |
| `GetSlotPath(int slot, const string& type)` | `src/core/game_state_saver.cpp` | `include/core/game_state_saver.h` | Generate path: `saves/slot_N/manual/manual.json` atau `saves/slot_N/autosave/` |
| `GetMapDisplayName(const string& mapPath)` | `src/map/map.cpp` | `include/map/map.h` | Ekstrak nama map dari path: `assets/maps/forest.json` menjadi `forest` |

### Variabel Global Baru

| Variabel | Tipe | File | Deskripsi |
|---|---|---|---|
| `g_ActiveSaveSlot` | `int` | `game_state_saver.h` | -1 = tidak aktif, 0-4 = slot manual aktif |
| `g_SaveSlotActive` | `bool` | `game_state_saver.h` | true jika slot aktif |

### Alur Active Slot Tracking

```txt
SaveLoadScreen: pilih slot → SetActiveSlot(slot) → SaveGameState()
WriteAutosave:  cek g_ActiveSaveSlot >= 0 → tulis ke slot aktif
Return to Menu: SaveGameState + WriteSaveFile → SetActiveSlot(-1)
New Game:       SetActiveSlot(0) → ResetPlayer() → ResetMap()
```

---

## Wave 3 — Per-Slot Directory Routing (2026-06-05)

| Item | Detail |
|---|---|
| **Commits** | `a15840b`, `601f360` |
| **Fokus** | Routing semua operasi save/load ke direktori per-slot |

### Struktur Direktori Baru

Mulai v3, setiap slot memiliki direktori sendiri yang terisolasi:

```
saves/
+-- slot_0/
|   +-- manual/
|   |   +-- manual.json          # Full state game (version=3)
|   +-- autosave/
|   |   +-- autosave_01-01-2025-12-00-00.json
|   |   +-- ...                  # Maksimal 5 file per slot
|   +-- enemies/
|   |   +-- <sanitized_map_path>.json
|   +-- items/
|       +-- <sanitized_map_path>.json
+-- slot_1/
|   +-- ...
+-- slot_4/
|   +-- ...
+-- .migration_completed_v3
```

### Fungsi Baru

| Fungsi | Deskripsi |
|---|---|
| `EnsureSlotDirectory(int slot)` | Buat struktur `saves/slot_N/{manual,autosave,enemies,items}/` |

### Perubahan Path

| Sebelum (v2) | Sesudah (v3) |
|---|---|
| `saves/manual/slot0.json` | `saves/slot_N/manual/manual.json` |
| `saves/autosave/autosave_*.json` | `saves/slot_N/autosave/autosave_*.json` |
| `saves/enemies/<path>.json` | `saves/slot_N/enemies/<path>.json` |
| `saves/items/<path>.json` | `saves/slot_N/items/<path>.json` |

### Autosave Per-Slot Rotation

`WriteAutosave()` menulis dengan timestamp rotation:

- **Path**: `saves/slot_N/autosave/autosave_DD-MM-YYYY-HH-MM-SS.json`
- **Guard**: Hanya bekerja jika `g_ActiveSaveSlot >= 0`
- **Prune**: Maksimal 5 file per slot, terlama dihapus
- **Atomic write**: Semua file via `.tmp` + rename
- **Pemicu**: Map switch, periodic 60s timer, fresh game start

### Isolasi Per-Slot

Setiap slot 0-4 terisolasi penuh:

1. **Path routing**: Semua operasi via `GetSlotPath()` yang menghasilkan path berdasarkan slot aktif
2. **Active slot tracking**: `SetActiveSlot(N)` mengelola slot aktif global
3. **Worldgen mapping**: `worldgenSlot` di `manual.json` memetakan slot ke `worldseed/save_N/`
4. **Reset terpisah**: `ResetPlayer()` hanya reset state memory, `ResetMap()` bersihkan worldseed

---

## Wave 4 — v2→v3 Migration Pipeline (2026-06-05)

| Item | Detail |
|---|---|
| **Commit** | `87768b0` |
| **Fokus** | Pipeline otomatis untuk migrasi save format lama (v2) ke struktur per-slot (v3) |

### Fungsi Baru

| Fungsi | Header | Deskripsi |
|---|---|---|
| `NeedsMigration()` | `include/core/game_state_saver.h` | Return true jika `saves/manual/slot0.json` ada DAN sentinel belum ada |
| `RunMigration()` | `include/core/game_state_saver.h` | Eksekusi 4-langkah migrasi. Return false jika ada gagal (atomic abort) |
| `MarkMigrationComplete()` | `include/core/game_state_saver.h` | Tulis sentinel `saves/.migration_completed_v3` |

### Sentinel File

File kosong `saves/.migration_completed_v3` mencegah migrasi berjalan ulang. Jika sentinel sudah ada, `NeedsMigration()` return false.

### Pipeline (4 Langkah)

```txt
Startup Game → NeedsMigration()?
  TIDAK → Lanjut normal
  YA → RunMigration()
         Langkah 1 (Task 14):
           Copy saves/manual/slot0.json (v2)
           → saves/slot_0/manual/manual.json (v3)
           Tambah field: slotIndex=0, saveType="manual",
           playTime=0, mapDisplayName="", worldgenSlot=-1
           Atomic write via .tmp + rename

         Langkah 2 (Task 15):
           Rename saves/enemies/* → saves/slot_0/enemies/

         Langkah 3 (Task 16):
           Rename saves/items/* → saves/slot_0/items/

         Langkah 4 (Task 17):
           Hapus: saves/manual/, saves/enemies/, saves/items/ (lama)
           Tulis: saves/.migration_completed_v3

         BERHASIL → Lanjut startup normal
         GAGAL   → Log warning, migrasi akan dicoba lagi di startup berikutnya
```

### Keamanan (Atomic)

- Jika Langkah 1 gagal (disk full, file corrupt), pipeline berhenti dan return false
- Langkah 2-3 menggunakan rename (cepat, atomik di filesystem yang sama)
- Sentinel hanya ditulis setelah semua langkah berhasil
- Save lama tetap utuh jika migrasi gagal

### Catatan

- Migrasi hanya untuk slot 0 (slot tunggal dari sistem v2)
- Slot 1-4 tetap kosong untuk save baru
- Worldgen data di `saves/manual/` tidak dimigrasi — worldgen pakai sistem per-slot `worldseed/save_N/`

---

## Wave 5 — SaveLoadScreen UI (2026-06-05)

| Item | Detail |
|---|---|
| **Commits** | `959d1e6`, `694234f`, `d88611c`, `b8d182f`, `fd7e2c7` |
| **Fokus** | UI layar Save/Load dengan slot grid, wiring ke pause menu dan main menu |

### File Baru

| File | Deskripsi |
|---|---|
| `include/ui/saveLoadScreen.h` | Deklarasi class `SaveLoadScreen`, enum `SaveLoadMode` |
| `src/ui/saveLoadScreen.cpp` | Implementasi UI: slot grid, popup konfirmasi, metadata refresh |

### Class Overview

| Aspek | Deskripsi |
|---|---|
| **Screen state** | `SAVE_LOAD` (di enum `ScreenState`) |
| **Global instance** | `SaveLoadScreen saveLoadScreen` di `src/core/main.cpp` |
| **Mode** | `SAVE_MODE` (simpan) atau `LOAD_MODE` (muat) |

### Method

| Method | Deskripsi |
|---|---|
| `Show()` | Aktifkan layar, muat texture, hitung dimensi, refresh metadata semua slot |
| `Hide()` | Nonaktifkan layar |
| `Update()` | Handle input: klik slot, popup, save/load. Handle back button. |
| `Draw()` | Render header, slot grid, back button, popup konfirmasi |
| `SetReturnScreen(screen)` | Set layar tujuan saat BACK diklik |
| `SetMode(mode)` | Set SAVE_MODE atau LOAD_MODE |
| `CalculateDimensions()` | Hitung posisi/ukuran elemen UI (850x500 area) |
| `GetSlotAtPosition(pos)` | Deteksi slot yang diklik berdasarkan posisi mouse |
| `DrawSlotBox(index, x, y, occupied, mapName, timestamp, mousePos, enabled)` | Gambar satu slot box |
| `DrawSlotGrid(mousePos)` | Gambar seluruh grid slot (5 manual + 5 autosave) |
| `RefreshSlotMetadata()` | Scan `saves/slot_N/manual/manual.json` tiap slot, baca mapDisplayName + timestamp |

### Mode Operasi

**SAVE_MODE:**
- 5 slot manual (0-4) bisa di-save
- Slot autosave (5-9) dinonaktifkan
- Slot kosong: langsung save tanpa konfirmasi
- Slot terisi: popup "Overwrite existing save?"
- Setelah save: `SetActiveSlot()`, `SaveGameState()`, tutup layar

**LOAD_MODE:**
- Semua slot (0-9) yang terisi data ditampilkan
- Slot kosong dinonaktifkan
- Slot terisi: popup "Load this save?" sebelum load
- Setelah load: `SetActiveSlot()`, `ReadSaveFile()`, `RestoreGameState()`

### Layout UI

```
+------------------------------------------+
|           SAVE GAME / LOAD GAME           |
|                                          |
|  MANUAL SAVE                             |
|  +----------+ +----------+ +----------+  |
|  | Slot 0   | | Slot 1   | | Slot 2   |  |
|  | tutorial | | forest   | | (empty)  |  |
|  +----------+ +----------+ +----------+  |
|  +----------+ +----------+               |
|  | Slot 3   | | Slot 4   |               |
|  | (empty)  | | cave     |               |
|  +----------+ +----------+               |
|                                          |
|  AUTO SAVE                               |
|  +----------+ +----------+ +----------+  |
|  | Slot 5   | | Slot 6   | | Slot 7   |  |
|  +----------+ +----------+ +----------+  |
|  +----------+ +----------+               |
|  | Slot 8   | | Slot 9   |               |
|  +----------+ +----------+               |
|                                          |
|  [BACK]                                   |
+------------------------------------------+
```

### Wiring ke Menu

| Entry Point | File | Mode |
|---|---|---|
| Pause Menu -> Save Game | `src/ui/pauseMenu.cpp` case 1 | SAVE_MODE |
| Pause Menu -> Load Game | `src/ui/pauseMenu.cpp` case 2 | LOAD_MODE |
| Main Menu -> Load Game | `src/ui/mainMenu.cpp` case 1 | LOAD_MODE |

Semua mengubah `state->currentScreen = SAVE_LOAD`. Main loop di `main.cpp` menangani rendering/update `SAVE_LOAD` state dengan memanggil `saveLoadScreen` methods.

### Commit Details per Task

| Task | Commit | Deskripsi |
|---|---|---|
| Task 18 | `959d1e6` | Struktur awal SaveLoadScreen (.h + .cpp), layout slot grid, slot box rendering |
| Task 19+20 | `694234f` | Mode operasi (SAVE_MODE/LOAD_MODE), popup konfirmasi, save/load logic |
| Task 21+22+25 | `d88611c` | `RefreshSlotMetadata()`, `GetSlotAtPosition()`, back button, auto-refresh |
| Task 23 | `b8d182f` | Wiring ke pause menu: Save Game -> SAVE_MODE, Load Game -> LOAD_MODE |
| Task 24 | `fd7e2c7` | Wiring ke main menu: Load Game -> LOAD_MODE, ganti case 1 yang lama |

---

## Ringkasan: Status Concern Sebelumnya

| Concern Sebelumnya | Status Setelah Waves 1-5 |
|---|---|
| **C1**: Cancel Load hapus worldseed (mainMenu.cpp cancel popup) | **Resolved**. `ClearSavedState()` dipisah jadi `ResetPlayer()` + `ResetMap()`. Cancel popup hanya reset memory, tidak hapus worldseed. |
| **C3**: Single save slot (`saves/manual/slot0.json`) | **Resolved**. 5 manual slot + 5 autosave slot, per-slot directory isolation, SaveLoadScreen UI untuk pilih slot. |
| **C4**: Worldseed cleanup terlalu agresif (hapus semua `save_N`) | **Resolved**. `ResetMap()` dengan slot-specific cleanup, worldgen mapping via `worldgenSlot` field. |
