# Restart Pipeline — slot_-1 Runtime Workspace

> **Last updated:** 2026-06-16
> **Related issues:** #09 Restart Logic, #23 Worldseed Overwrite

---

## Overview

Restart system di Dungeon menggunakan **slot_-1 sebagai runtime workspace**. Konsepnya sederhana: setiap kali game state berubah (save, ganti map, start/load game), slot_-1 di-overwrite dengan snapshot terbaru. Restart tinggal ambil dari slot_-1 — selalu fresh, gak perlu fallback chain.

```
Semua event → update slot_-1 → restart pure HasInitial(-1)
```

---

## slot_-1 Runtime Workspace

### Konsep

slot_-1 adalah slot virtual yang **tidak pernah ditampilkan di save/load screen**. Dia bekerja di belakang layar sebagai:

- **Snapshot paling mutakhir** dari game state
- **Single source of truth** untuk restart
- **Tidak bisa di-save manual** — `SaveManual(slot<0)` return false

### Lokasi file

```
saves/slot_-1/manual/snapshot_initial.json
```

### Kapan slot_-1 di-update

| Event                    | Calling function                              | Flow                                              |
| ------------------------ | --------------------------------------------- | ------------------------------------------------- |
| Start Game               | `loading_screen.cpp` → `HandleFastPath`       | `SaveInitial(slot_N)` → `CaptureInitialSnapshot(-1)` |
| Load Game (fast path)    | `loading_screen.cpp` → `HandleFastPath`       | `SaveInitial(slot_N)` → `CaptureInitialSnapshot(-1)` |
| Load Game (initial load) | `loading_screen.cpp` → `HandleInitialLoad`    | `SaveInitial(slot_N)` → `CaptureInitialSnapshot(-1)` |
| Door transition          | `loading_screen.cpp` → `HandleMapSwitch`      | Old-map flag detect → `CaptureInitialSnapshot(-1)` |
| Manual save              | `savemanager.cpp` → `SaveManual`              | `WriteSnapshot(slot_N)` → `CaptureInitialSnapshot(-1)` |
| Autosave                 | `savemanager.cpp` → `SaveAutosave`            | Autosave write → `CaptureInitialSnapshot(-1)` |
| Checkpoint               | `savemanager.cpp` → `SaveCheckpoint`          | `WriteSnapshot(slot_N)` → `CaptureInitialSnapshot(-1)` |

### Guard

`SaveManual(slot<0)` langsung return false — mencegah manual save ke slot_-1 secara eksplisit.

---

## Restart Flow (pauseMenu.cpp)

### Priority

```
HasInitial(-1) → pure runtime workspace
```

Tidak ada fallback. HasInitial(-1) **pasti ada** karena semua save event update slot_-1.

### Pipeline

```cpp
// 1. Load snapshot dari slot_-1
GameSnapshot snap = SaveManager::LoadInitial(-1);

// 2. ApplyPreSpawn — restore dead entities + consumed props (SEBELUM spawn)
SaveManager::ApplyPreSpawn(snap);

// 3. Spawn — spawn enemies + items pake deterministic UUID
SpawnEnemiesFromMap();
SpawnItemWave();

// 4. ApplyPostSpawn — FULL restore player, enemies, items, kamera, history
SaveManager::ApplyPostSpawn(snap);

// 5. Rebuild collision
SpawnObject();
RebuildObstacleCache();
globalFlowField.Invalidate();

// 6. Back to gameplay
state->currentScreen = PLAY;
```

### Kenapa gak perlu reload map?

Karena slot_-1 selalu berisi snapshot dari **map yang lagi dimainin**. Setiap ganti map → `HandleMapSwitch` → `CaptureInitialSnapshot(-1)` di akhir stage 2. Jadi restart gak perlu reload map — snapshot sudah sesuai map yang aktif.

---

## Loading Screen Dispatch

Tiga mode loading screen, dipilih berdasarkan state:

```
UpdateLoadingScreen()
├── isSwitchingMap || isGoingBack → HandleMapSwitch()
├── assetsLoaded (true)           → HandleFastPath()
└── else                          → HandleInitialLoad()
```

### HandleFastPath

Digunakan ketika **assets game sudah pernah di-load** (Start Game setelah main menu, Load Game setelah setidaknya sekali main).

1. UnloadMap
2. Load map dari savedMapState
3. ApplyPreSpawn kalo ada manual save
4. InitAll + RestoreGameState
5. `CaptureSnapshot` → `SaveInitial(slot_N)` → `CaptureInitialSnapshot(-1)`

### HandleInitialLoad

Digunakan **first time loading** (fresh start program).

1. Stage 0: InitTextures
2. Stage 1: Load map
3. Stage 2: Finalize
4. Default: ApplyPreSpawn → InitAll → RestoreGameState
5. `CaptureSnapshot` → `SaveInitial(slot_N)` → `CaptureInitialSnapshot(-1)`

### HandleMapSwitch

Digunakan untuk **transisi antar map** (door, elevator, dll).

Stages:
- **Stage 0**: SEBELUM UnloadMap — cek old map untuk `initial_snapshot` objects → set `s_OldMapHasInitialSnapshot` flag
- **Stage 1**: Load map baru + loading text
- **Stage 2**: InitPlayer → SpawnEnemies → ApplyPostSpawn → kalo flag true, `CaptureInitialSnapshot(-1)`

Trigger mechanism:
```
Stage 0: TilesonGetObjectsByType(tilesonMap, "initial_snapshot") → set flag
Stage 2: if (s_OldMapHasInitialSnapshot) → CaptureInitialSnapshot(-1) → reset flag
```

Object Tiled harus punya `type = "initial_snapshot"` (bukan name). Ditempatkan di layer terpisah sebagai rectangle objects (visible=false).

---

## Old-Map Flag Approach

### Masalah

CaptureInitialSnapshot(-1) gak boleh dipanggil pas map transisi masih loading — nanti snapshotnya nyangkut di state **sebelum** player beneran masuk map baru.

### Solusi

Deteksi object Tiled di **source map** (yang mau ditinggal), bukan destination map.

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

### Kenapa unconditional (tanpa HasInitial guard)?

Karena kita mau **selalu overwrite** pas transisi dari map yang punya initial_snapshot objects. Di main_hub.json ada 3 object initial_snapshot (di setiap door). Jadi setiap door transition dari main_hub → map lain, slot_-1 di-update.

---

## Deterministic UUID

Restart pipeline menggunakan **deterministic UUID** untuk entity matching:

```cpp
GenerateDeterministicUUID(seed, mapObjectID, name, instanceIndex)
```

- `seed` = worldseed (frozen dari worldgen)
- `mapObjectID` = object ID dari Tiled
- `name` = class name entity
- `instanceIndex` = index dalam cluster

Ini memastikan ApplyPreSpawn/ApplyPostSpawn bisa match entity dengan benar antar restart.

---

## SaveSystem Interaction

```
SaveSystem (slot_N)
    │
    ├── manual / snapshot.json         ← SaveManual (eksplisit)
    ├── autosaves / snapshot_*.json    ← SaveAutosave (otomatis per map)
    ├── checkpoints / *.json           ← SaveCheckpoint (per map entry)
    └── snapshot_initial.json          ← SaveInitial (restart baseline)
    
Runtime Workspace (slot_-1)
    └── manual / snapshot_initial.json ← CaptureInitialSnapshot(-1) (overwrite terus)
```

slot_-1 **independen** dari slot_N — isinya snapshot terkini yang cuma dipake buat restart.

---

## File References

| File | Peran |
|---|---|
| `src/core/loading_screen.cpp` | Dispatching + CaptureInitialSnapshot(-1) di semua entry point |
| `src/core/savemanager.cpp` | SaveManual/SaveAutosave/SaveCheckpoint update slot_-1 |
| `src/ui/pauseMenu.cpp` | Restart handler — pure HasInitial(-1) |
| `src/core/map.cpp` | InitMap default ke main_hub.json |
| `src/core/game_state_saver.cpp` | Fallback snapshot defaults |
| `assets/maps/main_hub.json` | initial_snapshot objects sebagai trigger |
| `include/core/savemanager.h` | API declarations |
