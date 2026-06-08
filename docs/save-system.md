# Save System — Dokumentasi Arsitektur

## 1. Ikhtisar

Save system menggunakan **SaveManager** (static class) + **GameSnapshot** (struct) sebagai source of truth tunggal untuk semua data persistensi. Semua state game (player, enemies, items, props, map history, barrier) ditangkap dalam satu snapshot dan disimpan/load lewat API terpusat.

**File utama:**
- `include/core/savemanager.h` — deklarasi GameSnapshot + SaveManager
- `src/core/savemanager.cpp` — implementasi
- `include/core/game_state_saver.h` — old format backward compat + RestoreGameState
- `src/core/game_state_saver.cpp` — old format + dual-path RestoreGameState

---

## 2. GameSnapshot

Struct yang merepresentasikan **seluruh state game** pada satu titik waktu.

### Field utama

| Field | Tipe | Deskripsi |
|-------|------|-----------|
| version | uint32_t | Versi format snapshot (SNAPSHOT_VERSION=2) |
| timestamp | int64_t | Unix timestamp saat capture |
| player | SavedPlayerState | Posisi, stats, inventory, dash, cooldown |
| enemies | vector<SavedEnemyState> | Posisi, HP, UUID, aggro, MapObjectID, Name |
| items | vector<SavedItemState> | Posisi, isPickedUp, definitionId, UUID |
| chestConsumed | vector<SavedPosData> | Posisi chest yang sudah dibuka |
| bombConsumed | vector<string> | Posisi bomb yang sudah hancur |
| crateConsumed | vector<string> | Posisi crate yang sudah hancur |
| deadEntities | vector<string> | Entity ID yang mati (tidak di-spawn ulang) |
| barrierCleared | bool | BarrierManager cleared state |
| barrierHasReLocked | bool | BarrierManager re-lock state |
| camera | SavedCameraState | Posisi & target kamera |
| mapHistory | vector<SavedMapHistory> | Stack riwayat map (untuk prev stage/door) |

### Snapshot Versioning

- **SNAPSHOT_VERSION = 2** — format saat ini (per June 2026)
- Version 1: legacy format (old path — masih didukung backward compat)

---

## 3. SaveManager — API

### Core Methods

```cpp
// Capture
static GameSnapshot CaptureSnapshot();

// Manual save/load (source of truth)
static bool SaveManual(int slot, const GameSnapshot& snap);
static GameSnapshot LoadManual(int slot);
static bool HasManual(int slot);
static void DeleteSlot(int slot);

// Autosave (rotating 5 files)
static bool SaveAutosave(int slot, const GameSnapshot& snap);
static GameSnapshot LoadAutosave(int slot, int index);

// Checkpoint (per-map cache untuk door transition)
static bool SaveCheckpoint(int slot, const std::string& mapPath, const GameSnapshot& snap);
static GameSnapshot LoadCheckpoint(int slot, const std::string& mapPath);
static bool HasCheckpoint(int slot, const std::string& mapPath);

// Initial snapshot (first frame after InitAll)
static bool SaveInitial(int slot, const GameSnapshot& snap);
static GameSnapshot LoadInitial(int slot);
```

### Snapshot Apply Methods

| Method | Use Case | Yang di-restore |
|--------|----------|-----------------|
| `ApplyPreSpawn(snap)` | Sebelum InitAll (checkpoint restore) | deadEntities, consumedPositions, barrier state |
| `ApplyPostSpawn(snap)` | Setelah InitAll (full state restore) | Player, enemies (by UUID), items (full replacement), consumed, barrier, camera, mapHistory |
| `ApplyCheckpointData(snap)` | Map transition (partial restore) | Enemies (by UUID+MapObjectID), items (by UUID+index), consumed props |

### Path Structure

```
saves/
├── settings/                      # Global settings
│   └── settings.json
├── slot_0/
│   ├── manual/
│   │   ├── snapshot.json          # Manual save (source of truth)
│   │   └── snapshot_initial.json  # Snapshot pertama setelah InitAll
│   ├── autosave/
│   │   ├── snapshot_0.json        # Autosave (rotating, max 5)
│   │   ├── snapshot_1.json
│   │   ├── snapshot_2.json
│   │   ├── snapshot_3.json
│   │   └── snapshot_4.json
│   └── checkpoints/
│       ├── {sanitized_mapPath}.json  # Per-map cache
│       └── {other maps...}.json
├── slot_1/
│   └── ...
└── slot_2/
    └── ...
```

### Atomic Writes

Semua write menggunakan file sementara (`.tmp`) + rename:

1. Write ke `snapshot.json.tmp`
2. Flush + close
3. Rename `snapshot.json.tmp` → `snapshot.json`

Ini mencegah file korup jika crash di tengah write.

---

## 4. Per-slot Isolation

Setiap save slot punya direktori sendiri (`saves/slot_N/`). Snapshot, autosave, dan checkpoint disimpan per-slot secara terpisah.

- `LoadManual(N)` → baca `saves/slot_N/manual/snapshot.json`
- `LoadCheckpoint(N, mapPath)` → baca `saves/slot_N/checkpoints/{sanitized_path}.json`
- `GetManualPath(N)`, `GetCheckpointPath(N, mapPath)`, dll — semua menggunakan `slot_N` dalam path.

---

## 5. UUID Entity Identity

Setiap enemy dan item punya UUID (string unik) yang di-generate saat spawn:

```cpp
enemy.SetUUID(GenerateUUID());
item.uuid = GenerateUUID();
```

Di snapshot, UUID disimpan di `SavedEnemyState.uuid` dan `SavedItemState.uuid`.
Saat restore, ApplyPostSpawn mencocokkan snapshot entities dengan yang baru di-spawn berdasarkan UUID.

### Enemy Matching Order

1. **UUID match** — cocok snapshot enemy dengan live enemy by UUID
2. **MapObjectID + Name fallback** — jika UUID tidak cocok, fallback ke kombinasi MapObjectID + Name (nitpick: perlu diperbaiki — Issue #minor)

### Item Replacement (Snapshot Source of Truth)

```cpp
// ApplyPostSpawn — items section
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

Snapshot adalah source of truth untuk items — seluruh `activeItems` diganti dengan data dari snapshot. Ini menghindari masalah non-deterministic spawn yang sebelumnya terjadi karena `ItemSpawnManager::SpawnAll()` menggunakan random seed.

### Dead Entity Filtering

`Entities::IsAlreadyDead(entityId)` dicek di `SpawnEnemiesFromMap()` — jika entity ID ada di dead set, enemy tidak di-spawn. Dead entities di-restore via ApplyPreSpawn (checkpoint flow) atau ApplyPostSpawn (manual load).

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

### Timeline (Run 1 — saat bug aktif)

1. **Save slot 0** (stage 1, enemies hidup) — `g_ActiveSaveSlot = 0`
2. Lanjut main, bunuh 2 enemy di stage 1, masuk stage 2
3. **Save slot 1** (stage 2, fresh) — `g_ActiveSaveSlot = 1`
4. Player prev stage / exit → `SaveRuntimeState(oldStage)` dipanggil dengan **`g_ActiveSaveSlot = 1`** (sudah berubah!)
5. Data worldseed_stage_0.json TERTIMPA ke slot 1, padahal harusnya slot 0

### Root Cause

`WorldgenIO::SaveRuntimeState(stageIndex)` menggunakan `g_ActiveSaveSlot` untuk path:

```cpp
bool SaveRuntimeState(int stageIndex)
{
    int uiSlot = g_ActiveSaveSlot >= 0 ? g_ActiveSaveSlot : 0;
    return SaveManager::SaveWorldgenStage(snap, uiSlot, stageIndex);
    //                                              ^^^^ SALAH saat g_ActiveSaveSlot berubah
}
```

Ketika player save ke slot 1, `g_ActiveSaveSlot` berubah jadi 1. Tapi `SaveRuntimeState` masih dipanggil dengan stage index 0 (karena player kembali ke stage 1 via prev stage/exit). Akibatnya data stage 1 (dengan dead entities dari playthrough lanjutan) tertulis ke slot 1, bukan slot 0.

### Efek pas load slot 0

```
LoadManual(0) → snapshot.json: enemies HIDUP (benar)
LoadRuntimeState(0) → worldseed_stage_0.json (Terkontaminasi!):
  deadEntities berisi enemy dari playthrough lanjutan
InitAll → SpawnEnemiesFromMap skip enemy yang dianggap mati
ApplyPostSpawn → cari UUID enemy → GAK ADA (gak di-spawn)
Enemy HILANG walau snapshot bilang hidup
```

### Fix: Snapshot Source of Truth

- **Dihapus**: `SaveRuntimeState/LoadRuntimeState`, worldseed_stage_N.json, `SetWorldgenPending/IsWorldgenPending`
- **Disederhanakan**: `ApplyPostSpawn` items → full replacement dari `snap.items`
- **Diseragamkan**: Semua state game disimpan dalam satu file (`snapshot.json`) per slot
- **Efek**: Loading dari main menu sekarang hanya bergantung pada manual snapshot — tidak ada lagi file runtime terpisah yang bisa terkontaminasi

---

## 8. Konvensi & Catatan Penting

### `g_ActiveSaveSlot`
- Variabel global yang menandai slot mana yang sedang aktif.
- Di-set di `saveLoadScreen.cpp` (LOAD flow) dan `mainMenu.cpp` (NEW GAME flow).
- Semua operasi SaveManager menggunakan ini.

### Old Format Backward Compat
`game_state_saver.cpp` masih menyimpan `RestoreGameState()` dual-path:
1. **New path**: `LoadManual(g_ActiveSaveSlot)` + `ApplyPostSpawn` — digunakan jika snapshot.json ditemukan dan version match
2. **Old path**: `ReadSaveFile(old manual.json)` + restore dari global state — fallback jika snapshot.json tidak ada

### Checkpoint vs Snapshot
- **Checkpoint**: Per-map cache, hanya dipakai di HandleMapSwitch (door/prev stage)
- **Manual Snapshot**: Source of truth untuk full save/load dari main menu
