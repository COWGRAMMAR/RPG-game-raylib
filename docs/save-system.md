# Save System — Dokumentasi Arsitektur

## 1. Ikhtisar

Save system menggunakan **SaveManager** (static class) + **GameSnapshot** (struct) sebagai single source of truth untuk semua data persistensi game. Semua state runtime (player, enemies, items, props, map history, barrier) ditangkap dalam satu snapshot dan disimpan/load lewat API terpusat.

**File utama:**
- `include/core/savemanager.h` — deklarasi `GameSnapshot` + `SaveManager`
- `src/core/savemanager.cpp` — implementasi (~1113 baris)
- `include/core/game_state_saver.h` — old format backward compat + `RestoreGameState()`
- `src/core/game_state_saver.cpp` — old format + dual-path `RestoreGameState()`

---

## 2. GameSnapshot

Struct `GameSnapshot` merepresentasikan **seluruh state runtime game** pada satu titik waktu.

### Field

| Field | Tipe | Deskripsi |
|-------|------|-----------|
| `version` | int | Versi format (`SNAPSHOT_VERSION = 1`) |
| `playerPosition` | Vector2 | Posisi player di world |
| `playerHealth` | float | HP saat ini |
| `playerMana` | float | Mana saat ini |
| `playerMaxHealth` | float | Max HP |
| `playerMaxMana` | float | Max Mana |
| `hotbar[4]` | InventoryItem[] | Hotbar inventory |
| `bag[12]` | InventoryItem[] | Full bag inventory |
| `animState` | struct | State, direction, isDead, activeSlot |
| `dashCooldown` | float | Remaining dash cooldown |
| `manaRegenTimer` | float | Timer delay before mana regen |
| `swingAttack` | json | Serialized attack state (active, timer, duration, raycastAngle, center, pressHeld) |
| `enemies` | vector\<SavedEnemyState\> | Posisi, HP, UUID, AI state, timers |
| `items` | vector\<SavedItemState\> | Posisi, definitionId, amount, UUID |
| `chestConsumed` | unordered_set\<string\> | Posisi chest yang sudah dibuka |
| `bombConsumed` | unordered_set\<string\> | Posisi bomb yang sudah hancur |
| `crateConsumed` | unordered_set\<string\> | Posisi crate yang sudah hancur |
| `barrierCleared` | bool | BarrierManager cleared state |
| `barrierHasReLocked` | bool | BarrierManager re-lock state |
| `deadEntities` | set\<string\> | Entity ID yang mati |
| `mapPath` | string | Path ke file map saat ini |
| `cameraTarget` | Vector2 | Posisi target kamera |
| `cameraZoom` | float | Zoom kamera |
| `mapHistory` | vector\<MapHistoryEntry\> | Stack riwayat map |
| `mapDisplayName` | string | Nama map human-readable |
| `slotIndex` | int | Nomor slot (-1 = unassigned) |
| `worldgenSlot` | int | Mapping ke worldseed slot (-1 = not worldgen) |
| `stageIndex` | int | Stage index (-1 = full snapshot, >=0 = worldgen per-stage) |
| `showFPS` | bool | Status toggle FPS |
| `timestamp` | string | ISO 8601 timestamp saat capture |

### Snapshot Versioning

- **`SNAPSHOT_VERSION = 1`** — format saat ini
- Validasi ketat: file dengan version mismatch langsung return `GameSnapshot()` kosong

---

## 3. SaveManager — API

### Core Methods

```cpp
// Capture seluruh state runtime
static GameSnapshot CaptureSnapshot();

// Capture initial snapshot (setelah spawn pertama, untuk restart)
static bool CaptureInitialSnapshot(int slot);

// Manual save/load (source of truth untuk full save/load)
static bool SaveManual(const GameSnapshot& snap, int slot);
static GameSnapshot LoadManual(int slot);
static bool HasManual(int slot);
static bool HasAnySave(int slot);
static bool DeleteSlot(int slotIndex);

// Autosave (captures internally, rotating max 5 files per slot)
static bool SaveAutosave(int slot);

// Checkpoint (per-map cache untuk door transition)
static bool SaveCheckpoint(const GameSnapshot& snap, const std::string& mapPath, int slot);
static GameSnapshot LoadCheckpoint(const std::string& mapPath, int slot);
static bool HasCheckpoint(const std::string& mapPath, int slot);

// Initial snapshot (restart cache replacement)
static bool SaveInitial(const GameSnapshot& snap, int slot);
static GameSnapshot LoadInitial(int slot);
static bool HasInitial(int slot);

// Utility
static void CleanupTmpFiles();  // Hapus semua file .tmp di saves/
```

### Snapshot Apply Methods

| Method | Use Case | Yang di-restore |
|--------|----------|-----------------|
| `ApplyPreSpawn(snap)` | Sebelum `InitAll` / `SpawnEnemiesFromMap` / `SpawnObject` | deadEntities, chest/bomb/crate consumed positions, barrier state |
| `ApplyPostSpawn(snap)` | Setelah `InitAll` (full state restore) | Player stats/inventory/position/animation/combat, enemies (by UUID then MapObjectID+Name), items (full replacement), consumed props, barrier, camera, mapHistory |
| `ApplyCheckpointData(snap)` | Map transition (partial restore) | Enemies (by UUID then MapObjectID+Name), items (by UUID then index), consumed props |

### Path Structure

```
saves/
├── settings/
│   └── settings.json
├── slot_0/
│   ├── manual/
│   │   ├── snapshot.json              # Manual save (source of truth)
│   │   └── snapshot_initial.json      # Initial snapshot (restart cache)
│   ├── autosave/
│   │   ├── snapshot_DD-MM-YYYY-HH-MM-SS.json  # Autosave (rotating, max 5)
│   │   └── ...
│   └── checkpoints/
│       ├── {sanitized_mapPath}.json   # Per-map state cache
│       └── ...
├── slot_1/
│   └── ...
├── slot_2/
│   └── ...
├── slot_3/
│   └── ...
└── slot_4/
    └── ...
```

### Atomic Writes

Semua write menggunakan file sementara (`.tmp`) + rename:

1. Write ke `snapshot.json.tmp`
2. Flush + close
3. Rename `snapshot.json.tmp` → `snapshot.json`

Ini mencegah file korup jika crash di tengah write. `CleanupTmpFiles()` tersedia untuk membersihkan file `.tmp` orphan.

---

## 4. Per-slot Isolation

Setiap save slot (0-4) punya direktori sendiri (`saves/slot_N/`):

- `SaveManual(snap, 0)` → tulis `saves/slot_0/manual/snapshot.json`
- `LoadCheckpoint("assets/maps/forest.json", 1)` → baca `saves/slot_1/checkpoints/assets_maps_forest_json.json`
- `DeleteSlot(2)` → hapus `saves/slot_2/` + cleanup worldseed orphan
- `HasAnySave(3)` → true jika ada manual atau initial snapshot di slot 3

### Active Slot Tracking

`g_ActiveSaveSlot` (global, di `game_state_saver.h`) menandai slot yang sedang aktif:

- Di-set di `SaveLoadScreen` (LOAD flow) dan `main menu` (NEW GAME flow)
- `SaveAutosave()` menggunakan slot ini untuk routing
- `CaptureSnapshot()` menyimpan `slotIndex = g_ActiveSaveSlot` ke dalam snapshot
- `DeleteSlot()` mereset `g_ActiveSaveSlot` ke -1 jika slot yang dihapus adalah slot aktif

---

## 5. UUID Entity Identity

Setiap enemy dan item punya UUID (string unik) yang di-generate saat spawn:

```cpp
enemy->SetUUID(GenerateUUID());
item.uuid = GenerateUUID();
```

### Enemy Matching Order (di ApplyPostSpawn)

1. **UUID match** — cocokkan snapshot enemy dengan live enemy by UUID
2. **MapObjectID + Name fallback** — jika UUID tidak cocok, fallback ke kombinasi `mapObjectID` + `enemyName`

### Item Replacement (Snapshot Source of Truth)

Untuk manual load, `ApplyPostSpawn()` melakukan **full replacement**:

```cpp
itemData.activeItems.clear();
for (const auto& saved : snap.items)
{
    ItemSpawn fresh;
    fresh.position = saved.position;
    fresh.isPickedUp = saved.isPickedUp;
    fresh.definitionId = saved.definitionId;
    fresh.amount = saved.amount;
    fresh.uuid = saved.uuid;
    itemData.activeItems.push_back(fresh);
}
```

Snapshot adalah source of truth untuk items — seluruh `activeItems` diganti dengan data dari snapshot. Ini menghindari masalah non-deterministic spawn yang sebelumnya terjadi.

Untuk checkpoint load, `ApplyCheckpointData()` melakukan partial restore: mencocokkan item by UUID dulu, lalu fallback ke index-based.

### Dead Entity Filtering

`Entities::IsAlreadyDead(entityId)` dicek di `SpawnEnemiesFromMap()` — jika entity ID ada di dead set, enemy tidak di-spawn. Dead entities di-restore via `ApplyPreSpawn()` sebelum spawn.

---

## 6. Loading Flows

### 6.1 HandleFastPath — assets sudah di-load

Dipanggil saat loading dari main menu (save file ada, assets cached).

```
1. UnloadMap()
2. LoadMap(mapPath)
3. LoadWorldgenForSave():
   - Load meta dari worldgen slot
   - ExtractStageFromPath → stage index
   - RunWorldgen(seed) → stamp rooms, spawn items
4. InitAll():
   - Entities::Clear()
   - Player::Init()
   - InitEnemy()
   - InitItems() → SpawnAll (spawn items dari map)
   - SpawnObject() → spawn chests, bombs, crates
   - SpawnEnemiesFromMap() → spawn enemies (skip dead)
   - SaveInitial()
5. RestoreGameState():
   - LoadManual(g_ActiveSaveSlot)
   - ApplyPostSpawn(snap)
```

### 6.2 HandleInitialLoad — boot pertama

3-stage FSM:
- **Case 0**: InitTextures
- **Case 1**: LoadMap + LoadWorldgenForSave
- **Default**: InitAll + RestoreGameState

### 6.3 HandleMapSwitch — door/prev stage transition

Menggunakan checkpoint system:

```
1. SaveCheckpoint(currentMap) → cache state sebelum pindah
2. LoadMap(mapBaru)
3. LoadCheckpoint(mapBaru) → ApplyPreSpawn
4. InitAll()
5. LoadCheckpoint(mapBaru) → ApplyCheckpointData (partial restore)
```

---

## 7. Cross-Slot Contamination Bug (Fixed)

### Timeline (saat bug aktif)

1. **Save slot 0** (stage 1, enemies hidup) — `g_ActiveSaveSlot = 0`
2. Lanjut main, bunuh 2 enemy di stage 1, masuk stage 2
3. **Save slot 1** (stage 2, fresh) — `g_ActiveSaveSlot = 1`
4. Player prev stage / exit → `SaveRuntimeState(oldStage)` dipanggil dengan **`g_ActiveSaveSlot = 1`** (sudah berubah!)
5. Data worldseed_stage_0.json TERTIMPA ke slot 1, padahal harusnya slot 0

### Root Cause

`WorldgenIO::SaveRuntimeState(stageIndex)` menggunakan `g_ActiveSaveSlot` untuk path routing — saat player ganti slot, state runtime stage sebelumnya salah routing.

### Fix: Snapshot Source of Truth

- **Dihapus**: `SaveRuntimeState/LoadRuntimeState`, worldseed_stage_N.json, `SetWorldgenPending/IsWorldgenPending`
- **Disederhanakan**: `ApplyPostSpawn` items → full replacement dari `snap.items`
- **Diseragamkan**: Semua state game disimpan dalam satu file (`snapshot.json`) per slot
- **Efek**: Loading dari main menu sekarang hanya bergantung pada manual snapshot — tidak ada lagi file runtime terpisah yang bisa terkontaminasi

---

## 8. Konvensi & Catatan Penting

### `g_ActiveSaveSlot`
- Variabel global yang menandai slot mana yang sedang aktif (`-1` = tidak aktif, `0-4` = slot manual)
- Di-set di:
  - `saveLoadScreen.cpp` — saat LOAD atau SAVE selesai
  - `mainMenu.cpp` — saat NEW GAME
- Semua operasi SaveManager (terutama autosave) menggunakan ini untuk routing.
- `SetActiveSlot(-1)` dipanggil saat "Return to Menu" untuk menonaktifkan slot.

### Old Format Backward Compat
`game_state_saver.cpp` masih menyimpan `RestoreGameState()` dual-path:
1. **New path**: `LoadManual(g_ActiveSaveSlot)` + `ApplyPostSpawn` — digunakan jika snapshot.json ditemukan dan version match
2. **Old path**: `ReadSaveFile(old manual.json)` + restore dari global state — fallback jika snapshot.json tidak ada

### Checkpoint vs Snapshot
- **Checkpoint**: Per-map cache, hanya dipakai di `HandleMapSwitch` (door/prev stage). Partial restore — tidak restore player/camera/mapHistory.
- **Manual Snapshot**: Source of truth untuk full save/load dari main menu. Full restore semua state.

### Save Format Version
- **SAVE_VERSION = 3** — untuk old format manual.json (backward compat)
- **SNAPSHOT_VERSION = 1** — untuk format baru via SaveManager/GameSnapshot
- Kedua version dicek secara independen di masing-masing code path.

### Ordering Apply Methods
Urutan pemanggilan kritis untuk correctness:
1. `ApplyPreSpawn(snap)` — HARUS sebelum `InitAll()` / `SpawnEnemiesFromMap()` / `SpawnObject()`
2. `InitAll()` — spawn enemies, items, props
3. `ApplyPostSpawn(snap)` — HARUS setelah semua spawn selesai
4. `ApplyCheckpointData(snap)` — untuk map switch, dipanggil setelah `InitAll()` (menggantikan `ApplyPostSpawn`)

---

## 9. Riwayat Perubahan (Changelog)

> Berikut adalah riwayat perubahan sistem save, cache, dan restart dari sesi pengembangan sebelumnya.
> Mencakup pipeline yang diubah, bug yang diperbaiki, dan catatan untuk developer selanjutnya.

---

### Pipeline Restart

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

**Perbedaan Worldgen vs Non-Worldgen**

| Aspek | Worldgen | Non-worldgen |
|---|---|---|
| **Sumber spawn enemy** | RNG dari seed → bisa beda tiap spawn | JSON statis map → selalu sama |
| **Urgensi cache** | Wajib — biar restart deterministik | Opsional — spawn dari JSON selalu sama |
| **Map layout** | Tetap (worldseed hasil RunWorldgen) | Tetap (loaded dari Tiled JSON) |
| **Fallback kalo .cache gak ada** | Musuh/item bisa beda tiap restart | Musuh/item selalu sama |

---

### Pipeline Save/Load (Legacy)

**Save (Pause → Save / Return to Menu):**
```
SaveGameState()
  ├─ Baca player state → savedPlayerState
  ├─ Baca enemy registry → savedEnemyStates
  ├─ Baca active items → savedItemStates
  ├─ Baca map state → savedMapState
  │   (path, deadEntities, chestsOpened, dll)
  ├─ Worldgen: WorldgenIO::SaveRuntimeState(currentStage)
  │   → simpan chests, crates, bombs, deadEnemies,
  │     itemDrops, barrier ke worldseed/save_N/runtime.json
  └─ WriteSaveFile("saves/manual/slot0.json")
```

**Load (Main Menu → Load Game) — legacy fast path:**
```
LoadMap(savedMapState.mapPath)
  ├─ Worldgen? → RunWorldgen(seed, isBoss) + LoadRuntimeState(stageIdx)
  ├─ SetWorldgenPending()
  ├─ RestoreDeadEntities() — skip kalo worldgen
  ├─ InitAll()
  ├─ RestoreGameState()
  ├─ PruneDeadEntities()
  └─ PLAY
```

---

### Bugs Fixed (Legacy)

**Bug #1 — ClearCache() Hapus Semua File**
| Item | Detail |
|---|---|
| **Lokasi** | `src/map/worldgenio.cpp:110-122` |
| **Gejala** | `ClearCache()` di `InitRun()` hapus SEMUA file di `saves/enemies/` dan `saves/items/` |
| **Akibat** | Save state enemy/item per-map ilang |
| **Fix** | Filter dengan `.cache` extension |

**Bug #2 — Layout Prefab Hilang Pas Load Game Worldgen**
| Item | Detail |
|---|---|
| **Lokasi** | `src/core/loading_screen.cpp` fast path |
| **Gejala** | Load game mid-worldgen lewat fast path → layout prefab ilang |
| **Fix** | Tambah `RunWorldgen()` + `LoadRuntimeState()` di fast path |

**Bug #3 — Crash Worldgen Run Ke-2**
| Item | Detail |
|---|---|
| **Lokasi** | `src/systems/interaction.cpp:102` + `src/core/game_state_saver.cpp:838` |
| **Gejala** | New Game → worldgen run 1 sukses → main menu → New Game crash |
| **Akar** | `ClearSavedState()` hapus worldseed folder tapi `SeedManager::isRunActive` masih true |
| **Fix** | `g_SeedManager.ResetRun()` di `ClearSavedState()` |

**Bug #4 — Cache Basi Setelah Load Game / Restart**
| Item | Detail |
|---|---|
| **Lokasi** | `src/core/loading_screen.cpp` fast path, `src/ui/pauseMenu.cpp` restart flow |
| **Gejala** | Setelah load game atau restart, file `.cache` masih pake snapshot dari sesi sebelumnya |
| **Fix** | Re-capture `.cache` di akhir fast path dan akhir restart flow |

**Bug #5 — WinMain Infinite Loop (Unit Test)**
| Item | Detail |
|---|---|
| **Lokasi** | `tests/constants_test.cpp` |
| **Gejala** | Stack overflow pas jalan `test_constants.exe` |
| **Akar** | MinGW-UCRT CRT wrapper panggil `WinMain` → panggil `main` lagi → infinite loop |
| **Fix** | Panggil `doctest::Context::run()` langsung dari `WinMain` |

---

### Concern / Catatan untuk Developer

**1. Cancel Load Hapus Worldseed (mainMenu.cpp:176):** `ClearSavedState()` dipanggil saat user cancel popup Load Game — otomatis hapus worldseed. Rekomendasi: pisah `ResetMemoryState()` dan `ResetWorldseed()`.

**2. Cache vs Save Separation:** File `.cache` dan `.json` ada di folder yang sama (`saves/enemies/`, `saves/items/`). Hati-hati fungsi cleanup jangan salah sasaran.

**3. Single Save Slot (Legacy):** Manual save dulu cuma `saves/manual/slot0.json`. Multi-slot sudah diimplementasi di Wave 5 (SaveLoadScreen UI).

**4. Worldseed Multiple Slot Isolation:** `ClearSavedState()` dulu hapus SEMUA worldseed. Sekarang sudah slot-specific via `worldgenSlot` field.

**5. WinMain Infinite Loop (MinGW-UCRT):** Jangan panggil `main()` dari `WinMain()` — panggil `doctest::Context::run()` langsung.

**6. Restart Flow Notes:** Restart tidak manggil `ClearSavedState()` atau `ClearCache()`. Cache di-re-capture di akhir restart. Kalo `.cache` gak ada, fallback ke `SpawnItemWave()`.

---

### Wave 1 — Data Safety Fixes (2026-06-05)

Commit `9617d40`

| Perubahan | Detail |
|---|---|
| Split `ClearSavedState()` → `ResetPlayer()` + `ResetCamera()` + `ResetMap()` | Setiap fungsi hanya reset satu aspek |
| Pindah inisialisasi camera cache ke `screen_handler.cpp` | Tersedia sebelum restart/load flow |
| Default `healthRegenTimer = 0.0f` | Cegah undefined behavior |

### Wave 2 — Save Format v3 + Utilities (2026-06-05)

Commits `8e9586d`, `3def89e`, `1b01b2e`

- **SAVE_VERSION** dinaikkan 2 → 3
- Field baru: `slotIndex`, `saveType`, `playTime`, `mapDisplayName`, `worldgenSlot`
- Fungsi baru: `GetActiveSlot()`, `SetActiveSlot()`, `IsSlotActive()`, `GetSlotPath()`, `GetMapDisplayName()`
- Variabel global: `g_ActiveSaveSlot`, `g_SaveSlotActive`

### Wave 3 — Per-Slot Directory Routing (2026-06-05)

Commits `a15840b`, `601f360`

- Setiap slot 0-4 punya direktori terisolasi: `saves/slot_N/{manual,autosave,enemies,items}/`
- Fungsi baru: `EnsureSlotDirectory()`
- Autosave per-slot dengan rotating 5 file, timestamp-based
- Isolasi penuh: path routing via `GetSlotPath()`, worldgen mapping via `worldgenSlot`

### Wave 4 — v2→v3 Migration Pipeline (2026-06-05)

Commit `87768b0`

- Fungsi: `NeedsMigration()`, `RunMigration()`, `MarkMigrationComplete()`
- Sentinel: `saves/.migration_completed_v3`
- Pipeline 4 langkah: copy slot0.json → rename enemies/ → rename items/ → hapus old + tulis sentinel
- Atomic: jika langkah 1 gagal, pipeline berhenti, save lama tetap utuh

### Wave 5 — SaveLoadScreen UI (2026-06-05)

Commits `959d1e6`, `694234f`, `d88611c`, `b8d182f`, `fd7e2c7`

- File baru: `include/ui/saveLoadScreen.h`, `src/ui/saveLoadScreen.cpp`
- 5 manual slot + 5 autosave slot, layout 3+2 grid
- Mode: SAVE_MODE (simpan) / LOAD_MODE (muat)
- Wiring: Pause Menu → Save/Load, Main Menu → Load

### Wave 6 — Option C SaveManager + Worldseed Stage Removal (2026-06-08)

- File baru: `include/core/savemanager.h` (403 baris), `src/core/savemanager.cpp` (1190 baris)
- GameSnapshot sebagai single source of truth
- Hapus `SaveRuntimeState/LoadRuntimeState` — tuntas cross-slot contamination
- Struktur direktori baru: `saves/slot_N/{manual,autosave,checkpoints}/`
- Items: full replacement dari snapshot (bukan partial match)

### Wave 7 — Legacy Cleanup (2026-06-08)

- Hapus `HasSaveFile()`, `DeleteSaveSlot()`, `RestoreDeadEntities()` dari `game_state_saver`
- Hapus subdir `enemies/` dan `items/` dari `EnsureSlotDirectory()`
- Hapus Migration Tasks 15-17 dari `RunMigration()`
- Testing values dikembalikan ke production
- Hapus `docs/save-refactor-plan.md`
- Filesystem: hapus `saves/slot_0/`, `saves/slot_1/`, worldseed lama, sentinel

### Wave 8 — SaveManager Alignment + Main Merge (2026-06-08)

Commit `83c23e3` — Finalisasi wiring SaveManager ke semua module.
Commit `18de54e` — Update player attributes + SAVE_VERSION kembali ke **3** (Wave 7 sebelumnya menurunkan ke 2).
Merge `9307fff` — 53 commit origin/main, fast-forward, zero conflict.

Perubahan terakhir pada dokumentasi:
| File | Perubahan |
|---|---|
| `docs/save-system.md` | Restrukturasi: SNAPSHOT_VERSION=1, koreksi API, hapus percakapan informal |
| `.agent/save-system-context.md` | Baru — konteks AI agent berbahasa Inggris |
| `docs/save-system-changelog.md` | Digabung ke sini (Section 9) |
