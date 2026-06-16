# Restart Logic — #5 #6 #2 Enemy gak respawn, Hotbar/Bag resets, Ghost item

## Masalah
Restart game menyebabkan 3 bug terpisah yang se-root:

| #   | Tipe    | Detail                                                       |
| --- | ------- | ------------------------------------------------------------ |
| 5   | Bug     | Enemy gak ke respawn pas restart — map snapshot dikotorin     |
| 6   | Bug     | Hotbar & bag isinya gak sesuai snapshot awal — jadi kosong/reset |
| 2   | Bug     | Ghost item — item visible tapi gak bisa di-pickup             |

## Root Cause

### Restart Flow (pauseMenu.cpp)
```
Entities::Clear()
  → ApplyPreSpawn(initialSnap)        // set props dari snapshot (deadEntities: [])
  → SpawnEnemiesFromMap()             // spawn semua enemies (worldgen pake RNG)
  → ApplyCheckpointData(initialSnap)  // apply enemy states dari snapshot
  → PlayerInstance.ResetForNewGame()
  → PlayerInstance.Init()
  → CaptureSnapshot()                 // simpan state baru sebagai initial
```

### Bug #5 — Enemy gak respawn
- `ApplyCheckpointData` pake **UUID matching** buat restore enemy state (hidup/mati)
- Worldgen spawn enemies pake **RNG** — beda seed/call count pas restart → UUID beda
- Fallback: MapObjectID + Name matching — kalo gak cocok, enemy dianggap "mati" (state dari snapshot)
- Karena RNG worldgen gak deterministic sama persis pas restart → spawn campur aduk → checkpoint data gak cocok → enemy gak ke-respawn

### Bug #6 — Hotbar/bag resets
- `ApplyCheckpointData` **hanya restore** enemy states, barrier states, props — **TIDAK restore player inventory**
- `PlayerInstance.ResetForNewGame()` + `PlayerInstance.Init()` dijalanin → inventory di-reset ke default
- `CaptureSnapshot()` nyimpen inventory yang udah di-reset sebagai initial baru
- **Akibat**: restart selalu mulai dengan inventory fresh, isi hotbar/bag lenyap

### Bug #2 — Ghost item
- Sama root: restart flow corruption
- Item di-ground yang di-spawn pas map inisialisasi bisa gak matching sama state checkpoint
- Item visible (render dari map data) tapi secara state gak terdaftar di runtime → gak bisa di-pickup

## Fix Approach (dari user)
> **Restart initial snapshot harusnya menang dan bakal nge-overwrite semua sistem save.**

Implementasi:
1. Pas restart, `initialSnap` harus jadi **sumber kebenaran utama**
2. `ApplyCheckpointData` perlu di-expand buat restore **semua** state — termasuk player inventory (hotbar + bag)
3. Atau reverse: `ResetForNewGame` + `Init` jangan dijalanin kalo ada initial snapshot — langsung restore dari snapshot
4. `CaptureSnapshot()` di akhir restart harus nyimpen state yang **benar** (inventory sesuai snapshot awal, bukan hasil reset)

## File yang terlibat
- `src/ui/pauseMenu.cpp` — restart handler
- `src/systems/checkpoint.cpp` — ApplyCheckpointData, CaptureSnapshot, ApplyPreSpawn
- `include/systems/checkpoint.h` — snapshot data structures
- `src/entities/player/player.cpp` — ResetForNewGame, Init
- `src/map/mapTransition.cpp` — HandleMapSwitch (autosave trigger)

## Status — RESOLVED  (2026-06-16)

### Final Implementation: slot_-1 Runtime Workspace

**Masalah lapisan 1** — Stale manual save di restart:
- `HasManual(0)` tetap true dari sesi lama → restart pake stale manual, bukan fresh initial snapshot
- Fix: slot_-1 sebagai runtime workspace yang selalu di-overwrite

**Masalah lapisan 2** — Initial snapshot keburu di-capture di tutorial:
- `CaptureInitialSnapshot(-1)` dipanggil di loading_screen (sebelum player masuk map beneran)
- Fix: old-map flag approach — cek source map untuk `initial_snapshot` object di Tiled, capture pas HandleMapSwitch stage 2

**Masalah lapisan 3** — Restart gak reload map:
- Restart handler set currentScreen = PLAY langsung, tanpa loading screen
- Fix: gak perlu reload map — slot_-1 selalu isi snapshot map terakhir yang dimasukin player

### Flow sekarang:
| Event | slot_-1 |
|---|---|
| Start Game (HandleFastPath) | CaptureInitialSnapshot(-1) |
| Load Game (HandleFastPath) | CaptureInitialSnapshot(-1) |
| Load Game (HandleInitialLoad) | CaptureInitialSnapshot(-1) |
| Door transition (HandleMapSwitch) | CaptureInitialSnapshot(-1) |
| SaveManual | CaptureInitialSnapshot(-1) setelah write |
| SaveAutosave | CaptureInitialSnapshot(-1) setelah write |
| SaveCheckpoint | CaptureInitialSnapshot(-1) setelah write |

### Restart priority:
```
HasInitial(-1)  // pure runtime workspace — no fallback chain
```

### Files changed:
- `src/core/loading_screen.cpp` — HandleFastPath + HandleInitialLoad + HandleMapSwitch (old-map flag trigger)
- `src/core/savemanager.cpp` — SaveManual + SaveAutosave + SaveCheckpoint tambah CaptureInitialSnapshot(-1)
- `src/ui/pauseMenu.cpp` — restart priority simplified ke pure HasInitial(-1)
- `src/core/map.cpp` — InitMap: tutorial.json → main_hub.json
- `src/core/game_state_saver.cpp` — fallback defaults: tutorial → main_hub
- `assets/maps/main_hub.json` — tambah initial_snapshot objects (type-based)

### Related plan files (deleted)
- `plans/restart-overhaul.md`
- `plans/09-restart-overhaul.md`
