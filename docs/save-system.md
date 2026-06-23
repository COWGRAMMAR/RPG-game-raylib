# Save System — Dokumentasi Arsitektur

Dokumen ini menggabungkan deskripsi arsitektur, API reference, flow pipeline, dan riwayat perubahan sistem save/load/restart.

---

## 1. Ikhtisar

Sistem save menggunakan **SaveManager** (static class) + **GameSnapshot** (struct) sebagai single source of truth untuk semua data persistensi. Semua state runtime (player, enemies, items, props, barrier, map history) ditangkap dalam satu snapshot dan disimpan/dibaca lewat API terpusat.

**Konsep utama: slot_-1 workspace.**
- `slot_-1` = **runtime workspace** — semua operasi save (manual, autosave, checkpoint) selalu ditulis ke -1 dulu
- `slot_N` (0–4) = **persisted save** — isinya adalah copy dari -1 saat `SaveManual()` dipanggil
- **Golden rule**: segala sesuatu ditulis ke -1 dulu, baru di-copy ke slot_N

**File utama:**
| File | Fungsi |
|---|---|
| `include/core/savemanager.h` | Deklarasi `GameSnapshot` + `SaveManager` |
| `src/core/savemanager.cpp` | Implementasi |
| `include/core/game_state_saver.h` | Backward compat + `RestoreGameState()` |
| `src/core/game_state_saver.cpp` | Dual-path restore (new/old format) |

---

## 2. Arsitektur Slot

### Dua Jenis Slot

| Slot | Fungsi | Isi |
|---|---|---|
| `slot_-1` | **Workspace runtime** — semua data selama bermain | checkpoints/, manual/, autosave/ |
| `slot_N` (N=0..MAX) | **Persisted save** — disimpan permanen | Copy dari -1 |

### Directory Structure

```
saves/
├── slot_-1/
│   ├── checkpoints/          # Snapshot per-map (dibikin pas ganti map)
│   │   ├── assets_maps_stage_1_json.json
│   │   ├── assets_maps_stage_2_json.json
│   │   └── assets_maps_main_hub_json.json
│   ├── manual/
│   │   ├── snapshot.json           # Data save manual (in-game)
│   │   └── snapshot_initial.json   # Data restart (in-game)
│   └── autosave/                   # Temporal snapshots tiap beberapa detik
│       └── snapshot_DD-MM-YYYY-HH-MM-SS.json
└── slot_N/  (N = g_ActiveSaveSlot, N=0..4)
    ├── checkpoints/
    ├── manual/
    └── autosave/
```

### Atomic Writes

Semua write menggunakan file sementara (`.tmp`) + rename:
1. Write ke `snapshot.json.tmp`
2. Flush + close
3. Rename `snapshot.json.tmp` → `snapshot.json`

Ini mencegah file korup jika crash di tengah write. `CleanupTmpFiles()` tersedia untuk membersihkan file `.tmp` orphan.

---

## 3. GameSnapshot

Struct `GameSnapshot` merepresentasikan **seluruh state runtime game** pada satu titik waktu.

### Field

| Field | Tipe | Deskripsi |
|---|---|---|
| **Version** | | |
| `version` | int | `SNAPSHOT_VERSION = 1` |
| **Player** | | |
| `playerPosition` | Vector2 | Posisi player di world |
| `playerHealth` | float | HP saat ini |
| `playerMana` | float | Mana saat ini |
| `playerMaxHealth` | float | Max HP |
| `playerMaxMana` | float | Max Mana |
| `hotbar[HOTBAR_SLOTS]` | InventoryItem[] | Hotbar inventory |
| `bag[BAG_SLOTS]` | InventoryItem[] | Full bag inventory |
| `animState.state` | int | Animation state enum |
| `animState.direction` | int | Direction enum |
| `animState.isDead` | bool | Player dead? |
| `animState.activeSlot` | int | Active hotbar slot |
| `dashCooldown` | float | Remaining dash cooldown |
| `manaRegenTimer` | float | Timer delay before mana regen |
| `swingAttack` | json | Serialized attack state (active, timer, duration, angle) |
| **Enemies & Items** | | |
| `enemies` | vector\<SavedEnemyState\> | Posisi, HP, UUID, AI state, timers |
| `items` | vector\<SavedItemState\> | Posisi, definitionId, amount, UUID |
| **Props** | | |
| `chestConsumed` | unordered_set\<string\> | Posisi chest yang sudah dibuka (key `"x_y"`) |
| `bombConsumed` | unordered_set\<string\> | Posisi bomb yang sudah hancur (key `"x_y"`) |
| `crateConsumed` | unordered_set\<string\> | Posisi crate yang sudah hancur (key `"x_y"`) |
| `barrierMap` | unordered_map\<string, bool\> | `barrierMap[mapPath] = cleared?` |
| **Dead Entities** | | |
| `deadEntities` | set\<string\> | Entity UUID yang mati permanen |
| **Map** | | |
| `mapPath` | string | Path ke file map saat ini |
| `cameraTarget` | Vector2 | Posisi target kamera |
| `cameraZoom` | float | Zoom kamera |
| `mapHistory` | vector\<MapHistoryEntry\> | Stack riwayat map |
| `mapDisplayName` | string | Nama map human-readable |
| **Meta** | | |
| `slotIndex` | int | -1 = unassigned |
| `worldgenSlot` | int | -1 = not worldgen |
| `stageIndex` | int | -1 = full snapshot, >=0 = worldgen per-stage |
| `showFPS` | bool | Status toggle FPS |
| `timestamp` | string | ISO 8601 timestamp saat capture |

### Snapshot Versioning

- **`SNAPSHOT_VERSION = 1`** — format saat ini
- Validasi ketat: file dengan version mismatch langsung return `GameSnapshot()` kosong

---

## 4. SaveManager — API

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
```

### Snapshot Apply Methods

| Method | Use Case | Yang di-restore |
|---|---|---|
| `ApplyPreSpawn(snap)` | Sebelum `InitAll` / `SpawnEnemiesFromMap` / `SpawnObject` | chest/bomb/crate consumed positions, barrier state |
| `ApplyPostSpawn(snap)` | Setelah `InitAll` (full state restore) | Player stats/inventory/position/animation/combat, enemies (by UUID → MapObjectID+Name), items (full replacement), consumed props, barrier, camera, mapHistory |
| `ApplyCheckpointData(snap)` | Map transition (partial restore) | Enemies (by UUID → MapObjectID+Name), items (full replacement), consumed props, barrier |

### Workspace Management

| Method | Fungsi |
|---|---|
| `MirrorToWorkspace(sourceSlot)` | Copy checkpoints/manual/autosave dari slot sumber ke -1 (clear + copy) |
| `CopyWorkspaceTo(slot)` | Copy checkpoints/manual/autosave dari -1 ke slot tujuan |
| `ClearWorkspaceManual()` | Hapus -1/manual/ (snapshot.json + snapshot_initial.json) |
| `ClearWorkspaceAutosave()` | Hapus -1/autosave/ |
| `ClearWorkspaceCheckpoints()` | Hapus -1/checkpoints/ |

### Serialization

```cpp
static nlohmann::json Serialize(const GameSnapshot& snap);
static GameSnapshot Deserialize(const nlohmann::json& root);
```

### Path Helpers

| Method | Hasil |
|---|---|
| `GetSlotDir(slot)` | `saves/slot_N/` |
| `GetManualPath(slot)` | `saves/slot_N/manual/snapshot.json` |
| `GetInitialPath(slot)` | `saves/slot_N/manual/snapshot_initial.json` |
| `GetAutosaveDir(slot)` | `saves/slot_N/autosave/` |
| `GetCheckpointPath(mapPath, slot)` | `saves/slot_N/checkpoints/{sanitized}.json` |

### Utility

```cpp
static void CleanupTmpFiles();  // Hapus semua file .tmp di saves/
static bool EnsureDirs(int slot);
static bool DeleteSlot(int slotIndex);
```

---

## 5. Flow Diagrams

### 5.1 Save Manual (In-Game Save)

```
Player klik Save
  → ClearWorkspaceManual()          # Hapus -1/manual/ (termasuk snapshot_initial.json)
  → CaptureInitialSnapshot(-1)      # Generate ulang snapshot_initial.json
  → WriteSnapshot(snap, -1)         # Generate ulang snapshot.json
  → SaveCheckpoint(snap, map, -1)   # Update checkpoint map saat ini
  → CopyWorkspaceTo(g_ActiveSaveSlot)  # Copy -1/ → slot_N/
```

**Hasil**: slot_N sekarang punya checkpoint semua map yang pernah dikunjungi, manual save, sama initial snapshot.

### 5.2 Map Switch (Pindah Map)

```
Player masuk pintu ke map lain
  → SaveCheckpoint(snap, currentMap, -1)   # Simpen state map SAAT INI
  → HandleMapSwitch()

     Stage 1 — Load map baru:
       LoadMap(targetMap)
       if worldgen → RunWorldgen()
       ApplyPreSpawn(snap)            # Restore consumed chest/bomb/crate
       SpawnObject()                  # Spawn semua props (skip consumed)
       RebuildObstacleCache()

     Stage 2 — Init player & entities:
       ApplyPreSpawn(snap)            # Double guard
       SpawnEnemiesFromMap()
       SpawnItemWave()
       ApplyCheckpointData(snap)      # Restore enemy HP, item posisi, barrier

     Stage 3 — Finalize:
       SaveAutosave(-1)
```

### 5.3 Load Save (Main Menu → Continue)

```
Player klik Continue
  → LoadManual(g_ActiveSaveSlot)         # Baca slot_N/manual/snapshot.json
  → MirrorToWorkspace(g_ActiveSaveSlot)  # Copy checkpoints/manual/autosave ke -1
  → HandleFastPath:
       ApplyPreSpawn(snap)             # Restore consumed state
       InitAll()                       # Load map + spawn everything
       RestoreGameState(state)         # Restore player HP, inventory, dll
```

**MirrorToWorkspace = 3 langkah:**
1. `ClearWorkspaceManual()`, `ClearWorkspaceAutosave()`, `ClearWorkspaceCheckpoints()`
2. Copy `checkpoints/`, `manual/`, `autosave/` dari slot_N ke -1

### 5.4 New Game

```
Player klik New Game
  → ClearWorkspaceManual()
  → ClearWorkspaceAutosave()
  → ClearWorkspaceCheckpoints()
  → InitMap()
  → SaveAutosave(-1)
  → InitAll()                       # Spawn everything fresh
```

### 5.5 Restart (In-Game)

Restart menggunakan **slot_-1 sebagai runtime workspace**. Selama bermain, setiap event (save, ganti map, start/load game) meng-update slot_-1. Restart tinggal ambil dari slot_-1 — selalu fresh, tanpa fallback chain.

```
Semua event → update slot_-1 → restart pure HasInitial(-1)
```

#### 5.5.1 Pipeline

```
Player klik Restart
  → ClearWorkspaceCheckpoints()       # Hapus checkpoints lama
  → LoadInitial(-1)                   # Baca snapshot_initial.json
  → ApplyPreSpawn(snap)
  → SpawnObject()
  → SpawnEnemiesFromMap()
  → ApplyPostSpawn(snap)
```

**Kenapa gak perlu reload map?** Karena slot_-1 selalu berisi snapshot dari **map yang lagi dimainin**. Setiap ganti map → `HandleMapSwitch` → `CaptureInitialSnapshot(-1)` di akhir stage 2. Jadi restart gak perlu reload map — snapshot sudah sesuai map yang aktif.

**Priority**: `HasInitial(-1) → pure runtime workspace`. Tidak ada fallback. HasInitial(-1) **pasti ada** karena semua save event update slot_-1.

#### 5.5.2 Old-Map Flag Approach

CaptureInitialSnapshot(-1) gak boleh dipanggil pas map transisi masih loading — nanti snapshotnya nyangkut di state **sebelum** player beneran masuk map baru. Solusinya: deteksi object Tiled di **source map** (yang mau ditinggal), bukan destination map.

```cpp
static bool s_OldMapHasInitialSnapshot = false;

// Stage 0 — sebelum UnloadMap
{
    auto oldTileson = tilesonMap; // snapshot before unload
    auto objects = TilesonGetObjectsByType(oldTileson, "initial_snapshot");
    s_OldMapHasInitialSnapshot = !objects.empty();
}
UnloadMap();

// Stage 2 — setelah player init + entities
if (s_OldMapHasInitialSnapshot)
{
    SaveManager::CaptureInitialSnapshot(-1);
    s_OldMapHasInitialSnapshot = false;
}
```

Object Tiled harus punya `type = "initial_snapshot"` (bukan name). Ditempatkan di layer terpisah sebagai rectangle objects (visible=false). Di `main_hub.json` ada 3 object initial_snapshot (di setiap door).

**Kenapa unconditional** (tanpa HasInitial guard)? Karena kita mau **selalu overwrite** pas transisi dari map yang punya initial_snapshot objects.

#### 5.5.3 Loading Screen Dispatch

Tiga mode loading screen, dipilih berdasarkan state:

```
UpdateLoadingScreen()
├── isSwitchingMap || isGoingBack → HandleMapSwitch()
├── assetsLoaded (true)           → HandleFastPath()
└── else                          → HandleInitialLoad()
```

Mode-mode ini memastikan `CaptureInitialSnapshot(-1)` selalu di-trigger di timing yang tepat — tidak pernah di tengah-tengah loading yang belum selesai.

#### 5.5.4 Deterministic UUID

Entity matching di restart menggunakan **deterministic UUID** agar konsisten antar restart (terutama di worldgen map):

```
GenerateDeterministicUUID(seed, mapObjectID, name, instanceIndex)
```

- `seed` = worldseed (frozen dari worldgen)
- `mapObjectID` = object ID dari Tiled
- `name` = class name entity
- `instanceIndex` = index dalam cluster

Ini memastikan ApplyPreSpawn/ApplyPostSpawn bisa match entity dengan benar antar restart — karena UUID dihasilkan dari kombinasi seed+mapObjectID yang deterministic, bukan random.

### 5.6 Coverage Matrix

| Kasus | -1/manual | -1/autosave | -1/checkpoints |
|---|---|---|---|
| New game (initial load) | Clear | Clear→fresh | Clear |
| New game (fast path) | Clear | Clear→fresh | Clear |
| Manual save | Clear→fresh | Keep (copied) | Keep (copied) |
| Map switch (keluar) | N/A | N/A | Write checkpoint |
| Map switch (masuk) | Not touched | Append | ApplyCheckpointData |
| Load save (MirrorToWorkspace) | Clear→copy | Clear→copy | Clear→copy |
| Restart | Not touched | Not touched | Clear→LoadInitial |
| Timer autosave | Not touched | Append | Not touched |

---

## 6. Apply Methods Detail

| Method | When | What it restores |
|---|---|---|
| `ApplyPreSpawn` | BEFORE SpawnObject | `deadEntities`, `chestConsumed`, `bombConsumed`, `crateConsumed`, `barrierMap` |
| `ApplyCheckpointData` | AFTER SpawnEnemiesFromMap + SpawnItemWave | Enemy HP/AIState, world items (full replacement), chest/bomb/crate consumed, barrier |
| `ApplyPostSpawn` | AFTER SpawnObject (restart path) | Same as ApplyCheckpointData + player stats + camera + mapHistory |

**Important**: `consumedPositions` untuk crates/bombs/chests di-set di BOTH ApplyPreSpawn dan ApplyCheckpointData — ApplyPreSpawn ensures SpawnObject skips them, ApplyCheckpointData adalah double guard.

### Ordering (kritis untuk correctness)
1. `ApplyPreSpawn(snap)` — HARUS sebelum `InitAll()` / `SpawnEnemiesFromMap()` / `SpawnObject()`
2. `InitAll()` — spawn enemies, items, props
3. `ApplyPostSpawn(snap)` — HARUS setelah semua spawn selesai (full restore)
4. `ApplyCheckpointData(snap)` — untuk map switch, dipanggil setelah `InitAll()` (menggantikan `ApplyPostSpawn`)

---

## 7. Per-slot Isolation

Setiap save slot (0-4) punya direktori sendiri (`saves/slot_N/`). Semua operasi save menggunakan `g_ActiveSaveSlot` untuk routing:

- `SaveManual(snap, g_ActiveSaveSlot)` → tulis ke -1 dulu, copy ke `saves/slot_N/`
- `LoadCheckpoint(map, 1)` → baca `saves/slot_1/checkpoints/...`
- `DeleteSlot(2)` → hapus `saves/slot_2/` + cleanup worldseed orphan

### Active Slot Tracking

`g_ActiveSaveSlot` (global, di `game_state_saver.h`):
- Di-set di `SaveLoadScreen` (LOAD flow) dan `main menu` (NEW GAME flow)
- `SaveAutosave()` menggunakan `g_ActiveSaveSlot` untuk routing
- `CaptureSnapshot()` menyimpan `slotIndex = g_ActiveSaveSlot`
- `DeleteSlot()` mereset `g_ActiveSaveSlot` ke -1 jika slot yang dihapus adalah slot aktif
- `SetActiveSlot(-1)` dipanggil saat "Return to Menu"

### Checkpoint vs Snapshot

- **Checkpoint**: Per-map cache, hanya dipakai di `HandleMapSwitch` (door/prev stage). Partial restore — tidak restore player/camera/mapHistory.
- **Manual Snapshot**: Source of truth untuk full save/load dari main menu. Full restore semua state.

---

## 8. UUID Entity Identity

Setiap enemy dan item punya UUID (string unik) yang di-generate saat spawn:

```cpp
enemy->SetUUID(GenerateUUID());
item.uuid = GenerateUUID();
```

### Enemy Matching Order (di ApplyPostSpawn / ApplyCheckpointData)

1. **UUID match** — cocokkan snapshot enemy dengan live enemy by UUID
2. **MapObjectID + Name fallback** — jika UUID tidak cocok, fallback ke kombinasi `mapObjectID` + `enemyName`

### Dead Enemy Handling

Enemy dengan `!isAlive` tidak memanggil `RegisterDeath(MapObjectID)` (menghindari spawn point poisoning). Langsung cari spawned enemy dengan `MapObjectID + Name` yang cocok, lalu **deactivate** (IsActive=false, Health=0). Jika tidak ada yang cocok (misal spawn count berbeda), enemy baru tetap hidup.

### Item Replacement (Full Replacement)

Snapshot adalah **source of truth** untuk items. Seluruh `itemData.activeItems` diganti dengan data dari snapshot:

```cpp
itemData.activeItems.clear();
for (const auto& saved : snap.items) {
    ItemSpawn fresh;
    fresh.position = saved.position;
    fresh.isPickedUp = saved.isPickedUp;
    fresh.definitionId = saved.definitionId;
    fresh.amount = saved.amount;
    fresh.uuid = saved.uuid;
    itemData.activeItems.push_back(fresh);
}
```

Ini berlaku untuk manual load (`ApplyPostSpawn`) dan checkpoint load (`ApplyCheckpointData`). Bug #24 sebelumnya menggunakan partial UUID matching yang gagal — sekarang full replacement.

### Dead Entity Filtering (Per-Instance UUID)

Sebelum bugfix (commit `fc58754`): `Entities::IsAlreadyDead(mapPath, objectId)` dicek di `SpawnEnemiesFromMap()` — jika MapObjectID ada di dead set, **seluruh spawn point** dilewati. Rectangle-spawned enemy (banyak enemy dengan MapObjectID sama) jadi ikut hilang walau hanya satu yang mati.

**Sekarang**: `Enemy::Update()` menggunakan `Entities::RegisterDeathByUUID(mapPath, uuid)` — setiap enemy dicatat secara individual via UUID unik. `SpawnEnemiesFromMap()` selalu spawn dari semua spawn point. Kematian per-instance ditangani oleh `ApplyPostSpawn()` / `ApplyCheckpointData()`.

Safety net `PruneDeadEntities()` menggunakan `IsDeadByUUID()` (UUID-based) bukan `IsAlreadyDead()` (MapObjectID-based).

### Props Persistence (Crate, Bomb, Chest)

Masing-masing prop manager menggunakan `consumedPositions` (`std::unordered_set<std::string>`) keyed by `EncodePos(pos)` = `"x_y"`. Saat spawn, posisi yang ada di consumed set akan di-skip.

Snapshot capture (savemanager.cpp):
```cpp
snap.bombConsumed = bombManager.GetConsumedPositions();
snap.crateConsumed = crateManager.GetConsumedPositions();
```

Restore: `ApplyPreSpawn` memanggil `SetConsumedPositions()` pada masing-masing manager.

---

## 9. Key Functions

| Fungsi | File:Line | Apa yang Dilakukan |
|---|---|---|
| `SaveManual()` | savemanager.cpp | Pipeline: cleanup -1 → write → copy ke slot |
| `SaveCheckpoint(snap, map, -1)` | savemanager.cpp | Simpen state map saat switch |
| `SaveAutosave(-1)` | savemanager.cpp | Auto-save ke -1 (timer/map switch) |
| `ApplyPreSpawn(snap)` | savemanager.cpp | Restore consumed state SEBELUM spawn |
| `ApplyCheckpointData(snap)` | savemanager.cpp | Restore enemy/item/props SETELAH spawn |
| `ApplyPostSpawn(snap)` | savemanager.cpp | Restore full state dari initial snapshot |
| `MirrorToWorkspace(slot)` | savemanager.cpp | Copy semua data dari slot ke -1 |
| `CopyWorkspaceTo(slot)` | savemanager.cpp | Copy semua data dari -1 ke slot |
| `CaptureSnapshot()` | savemanager.cpp | Bikin GameSnapshot dari state saat ini |
| `LoadManual(slot)` | savemanager.cpp | Baca snapshot.json dari slot |
| `LoadInitial(slot)` | savemanager.cpp | Baca snapshot_initial.json dari slot |

### ClearWorkspace Call Sites

| Location | Function | When |
|---|---|---|
| savemanager.cpp:SaveManual() | ClearWorkspaceManual() | Sebelum regenerate snapshot |
| savemanager.cpp:MirrorToWorkspace() | ClearWorkspaceManual+Autosave+Checkpoints | Sebelum copy dari slot |
| loading_screen.cpp:HandleFastPath else | ClearWorkspaceManual+Autosave+Checkpoints | New game (fast path) |
| loading_screen.cpp:HandleInitialLoad else | ClearWorkspaceManual+Autosave+Checkpoints | New game (initial load) |
| pauseMenu.cpp:Restart | ClearWorkspaceCheckpoints() | Sebelum LoadInitial |

### Autosave Call Sites

| Location | When |
|---|---|
| main.cpp:301 | Timer autosave (berkala) |
| loading_screen.cpp:246 | Map switch selesai (Stage 3) |
| loading_screen.cpp:321 | HandleFastPath new game |
| loading_screen.cpp:434 | HandleInitialLoad new game |

---

## 10. Bug History (Save System)

| Bug | Root Cause | Fix |
|---|---|---|
| **#24** World items gak persist | ApplyCheckpointData pake partial UUID matching | Full replacement dari snap.items |
| **#25** Checkpoints ilang di save manual | SaveManual cuma save 1 checkpoint, gak copy semua | slot_-1 workspace + CopyWorkspaceTo + autosave ke -1 |
| **MirrorToWorkspace** manual/autosave kebawa lama | Clear cuma checkpoints | Clear manual+autosave juga, baru copy dari source slot |
| **Crate/bomb respawn** di worldgen | HandleMapSwitch worldgen skip ApplyPreSpawn | Tambah ApplyPreSpawn sebelum SpawnObject |
| **Cross-slot contamination** | WorldgenIO::SaveRuntimeState pake g_ActiveSaveSlot yg sudah berubah | Snapshot source of truth — hapus runtime.json terpisah |
| **Enemy persistence (rectangle spawn)** | RegisterDeath(MapObjectID) bunuh seluruh spawn point | Per-Instance UUID death tracking |
| **Cache basi setelah load/restart** | .cache tidak di-re-capture | Re-capture di akhir setiap flow |
| **Worldgen crash run ke-2** | SeedManager::isRunActive masih true | ResetRun() di ClearSavedState() |

---

## 11. Riwayat Perubahan (Changelog)

### Wave 1 — Data Safety Fixes (2026-06-05)
Commit `9617d40`
- Split `ClearSavedState()` → `ResetPlayer()` + `ResetCamera()` + `ResetMap()`
- Pindah inisialisasi camera cache ke `screen_handler.cpp`

### Wave 2 — Save Format v3 + Utilities (2026-06-05)
Commits `8e9586d`, `3def89e`, `1b01b2e`
- SAVE_VERSION 2 → 3; field baru: slotIndex, saveType, mapDisplayName, worldgenSlot
- Fungsi: `GetActiveSlot()`, `SetActiveSlot()`, `GetSlotPath()`
- Global: `g_ActiveSaveSlot`, `g_SaveSlotActive`

### Wave 3 — Per-Slot Directory Routing (2026-06-05)
Commits `a15840b`, `601f360`
- Setiap slot 0-4 punya direktori terisolasi: `saves/slot_N/{manual,autosave,enemies,items}/`
- Autosave per-slot, rotating 5 file, timestamp-based

### Wave 4 — v2→v3 Migration Pipeline (2026-06-05)
Commit `87768b0`
- `NeedsMigration()`, `RunMigration()`, `MarkMigrationComplete()`
- Sentinel: `saves/.migration_completed_v3`

### Wave 5 — SaveLoadScreen UI (2026-06-05)
Commits `959d1e6`, `694234f`, `88611c`, `b8d182f`, `fd7e2c7`
- File: `include/ui/saveLoadScreen.h`, `src/ui/saveLoadScreen.cpp`
- 5 manual slot + 5 autosave slot, layout 3+2 grid

### Wave 6 — Option C SaveManager + Worldseed Stage Removal (2026-06-08)
- File: `savemanager.h` (408 baris), `savemanager.cpp` (~1300 baris)
- GameSnapshot sebagai single source of truth
- Hapus `SaveRuntimeState/LoadRuntimeState`
- Struktur direktori: `saves/slot_N/{manual,autosave,checkpoints}/`

### Wave 7 — Legacy Cleanup (2026-06-08)
- Hapus `HasSaveFile()`, `DeleteSaveSlot()`, `RestoreDeadEntities()` dari game_state_saver
- Hapus subdir `enemies/` dan `items/` dari `EnsureSlotDirectory()`

### Wave 8 — SaveManager Alignment + Main Merge (2026-06-08)
Commit `83c23e3`, `18de54e`, Merge `9307fff`
- Finalisasi wiring SaveManager ke semua module
- Update player attributes + SAVE_VERSION kembali ke 3

### Bugfix — Enemy Persistence (2026-06-09)
Commit `fc58754`
- Per-Instance UUID death tracking
- `SpawnEnemiesFromMap()` selalu spawn dari semua spawn point
- `ApplyPostSpawn`/`ApplyCheckpointData` deactivate dead enemies langsung

### #24 World Item Persist (2026-06-20)
- ApplyCheckpointData items: UUID matching partial → full replacement dari snap.items
- Hitbox direkonstruksi dari item definitions, spawnTime di-reset

### #25 Save Pipeline Rework (2026-06-20)
- slot_-1 pure workspace: semua runtime state di -1
- SaveManual: cleanup -1 → generate → CopyWorkspaceTo(slot)
- Autosave → slot_-1 (4 call sites changed)
- Functions: `ClearWorkspaceManual()`, `ClearWorkspaceAutosave()`, `CopyWorkspaceTo()`
- MirrorToWorkspace: ClearWorkspaceManual + Autosave + Checkpoints → copy dari source
- Crate/bomb respawn fix: ApplyPreSpawn di worldgen branch
