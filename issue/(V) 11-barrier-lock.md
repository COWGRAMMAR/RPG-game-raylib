# World Generation — #1 Barrier lock pas balik ke stage sebelumnya

## Masalah
Player masuk Stage 2 → balik ke Stage 1 → barrier di Stage 1 masih terkunci, padahal Stage 1 udah clear sebelumnya.

## Investigasi Flow

### Alur SwitchMap:
```
SwitchMap(stage2_path)         // Player sentuh CELL_FINISH
  → CaptureSnapshot()           // nyimpen state Stage 1 (barrierCleared=true kalo udah clear)
  → SaveCheckpoint(snap, "stage1_path", slot)
  → mapHistoryStack.Push("stage1_path")

GoBack()                        // Player pencet tombol balik
  → CaptureSnapshot()           // nyimpen state Stage 2
  → SaveCheckpoint(snap, "stage2_path", slot)
  → mapHistoryStack.Pop()       // dapet "stage1_path"
  → isGoingBack = true
```

### Alur HandleMapSwitch case 1 — Load map Stage 1:
```
LoadMap("stage1_path")
SetCurrentMapPath("stage1_path")

if (!isBack && path contains "worldseed/save_")   // isBack=true → else branch
else:
  BuildMapObjectIndex()
  if (HasCheckpoint("stage1_path", slot)):
    chkSnap = LoadCheckpoint(...)
    ApplyPreSpawn(chkSnap)      // barrierManager.SetCleared(snap.barrierCleared)
  SpawnObject()
    → SpawnBarriers()
      → if (cleared) skip spawn  // OK kalo barrierCleared=true
```

### HandleMapSwitch case 2 — Init entities:
```
Entities::Clear()               // gak sentuh barrier manager
ApplyPreSpawn(chkSnap)          // barrierManager.SetCleared() lagi — OK
SpawnEnemiesFromMap()
ApplyCheckpointData(chkSnap)    // barrierManager.SetCleared() lagi — OK
```

## Analisis — Kenapa masih bisa lock?

Secara flow kodenya harusnya **beres** — `ApplyPreSpawn` di case 1 set `barrierManager.cleared = true` SEBELUM `SpawnBarriers()` ngecek `if (cleared)`.

### Kemungkinan penyebab:

1. **Worldgen map path mismatch** — Stage 1 pake worldgen → map path `"worldseed/save_stage_X"`. Pas `SwitchMap("stage2")` dipanggil, `currentMapPath` buat Stage 1 mungkin masih path lama (bukan format yang dipake `HasCheckpoint`). Jadi checkpoint gak ketemu → `ApplyPreSpawn` gak dijalanin → `cleared=false` → barrier spawn.

2. **Checkpoint gak sempet ke-save** — Ada kemungkinan `barrierCleared` berubah jadi `true` TAPI checkpoint belum di-update. Cek di mana `SaveCheckpoint` dipanggil setelah barrier clear:
   - `map.cpp:473` — di `SwitchMap()` — hanya pas pindah map
   - `map.cpp:510` — di `GoBack()` — hanya pas balik
   - **Tidak ada auto-save periodik** yang nyimpen checkpoint pas barrier clear di tengah gameplay
   - Kalo player clear Stage 1 → main bentar di Stage 1 → baru pindah ke Stage 2 → `SwitchMap()` capture snapshot pas itu. Tapi kalo antara clear barrier dan pindah map ada state change lain... seharusnya tetap `barrierCleared=true`.

3. **Boss room re-lock** — Kalo Stage 1 punya boss room dan player pernah masuk → `ReLockBarriers()` set `cleared=false`. Pas balik, checkpoint nyimpen `cleared=false` → barrier spawn lagi.

4. **Snapshot version mismatch** — `ApplyPreSpawn` ngecek `snap.version != SNAPSHOT_VERSION`. Kalo format snapshot berubah antar sesi save, `ApplyPreSpawn` return duluan.

## Fix — barrierMap + always-spawn (2026-06-19)

### Root Cause
Barrier state sebelumnya cuma pake `barrierCleared + hasReLocked` di snapshot — relatif terhadap map path. Pas balik ke stage, state barrier gak konsisten karena snapshot disimpen per-stage pake format worldgen path.

### Solusi
- `barrierMap`: `std::unordered_map<string,bool>` di snapshot — kebenaran persisten per map path
  - `SerializeBarrierMap()` / `DeserializeBarrierMap()` di save/load
  - `CaptureSnapshot()` → dump barrierMap ke snap
  - `ApplyPreSpawn()` → restore barrierMap ke BarrierManager
  - `ApplyPostSpawn()` → sync barrierMap lagi setelah spawn
- `SpawnBarriers()` **always spawn** — hapus `if (cleared) return`
  - Barrier manager jadi runtime state machine; barrierMap = persistent truth
- Forward worldgen reset: `SetCleared(false)` sebelum `SpawnObject()` biar barrier selalu di-worldgen
- Checkpoint refactor: `SaveCheckpoint`/`SaveInitial` pake runtime workspace `-1` aja
  - `MirrorToWorkspace()`, `ClearWorkspaceCheckpoints()` untuk cleanup
- Cross-stage: map switch cukup tulis ke `-1`, checkpoint dual-write dihapus

### File yang diubah
| File | Perubahan |
|------|-----------|
| `include/core/savemanager.h` | Tambah barrierMap field, Serialize/DeserializeBarrierMap |
| `src/core/savemanager.cpp` | Implementasi barrierMap di CaptureSnapshot/ApplyPreSpawn/PostSpawn |
| `include/map/propsbehavior.h` | BarrierManager: barrierMap + SetCleared per map path |
| `src/map/propsbehavior.cpp` | SpawnBarriers always spawn, SetCleared forward to barrierMap |
| `src/core/loading_screen.cpp` | ApplyPreSpawn/PostSpawn sync barrierMap |
| `src/entities/entities.cpp` | ClearTileProps reset barrier state |
| `src/map/map.cpp` | SwitchMap, GoBack, worldgen reset |

## Status
- Root cause:   Worldgen map path mismatch + barrier state gak persisten antar stage
- Fix approach:  barrierMap (persistent per-map-path) + always-spawn runtime BarrierManager
- Dibantu oleh story: `#11 Barrier lock — fixed`
