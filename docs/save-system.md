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
