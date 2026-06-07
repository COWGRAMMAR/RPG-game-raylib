---
title: "Save System Changelog — AI Version"
purpose: "Machine-parsable reference for AI agents"
applies_to: ["restart system", "cache system", "save/load system", "worldgen persistence"]
session: "Death Drop + Revive + Restart + Unit Test + CI/CD + Bug Fixes"
---

# AGENT REF: Save System / Cache / Restart

> Format: structured sections with machine-parseable tables.
> Prefix: `##` = system, `###` = component/function, `####` = property.

---

## 1. SYSTEMS OVERVIEW

### 1.1 Restart System

```
id: restart-system
status: implemented
location: src/ui/pauseMenu.cpp:561-626
trigger: pause menu → restart confirm popup
dependencies: [cache-system, spawn-system]
```

| Property | Value |
|---|---|
| Clears runtime | `Entities::Clear()`, `itemData.activeItems.clear()`, `ClearTileProps()`, `Entities::ClearDeadEntities()`, all managers reset |
| Spawn enemies | `SpawnEnemiesFromMap()` — RNG-based for worldgen, JSON-static for non-worldgen |
| Cache overlay | `LoadEnemiesForMap(cachePath)` + `LoadItemsForMapDir(cachePath)` |
| Cache missing fallback | `SpawnItemWave()` |
| Player reset | `ResetForNewGame()` + `Init(state, SPAWN_OBJECT_NAME)` |
| Cache re-capture | At end of restart: `SaveEnemiesForMap(cachePath)` + `SaveItemsForMapDir(cachePath)` |
| Worldseed affected | NO — does not call ClearSavedState() |

### 1.2 Cache System

```
id: cache-system
status: implemented
storage: saves/enemies/<hash>.cache, saves/items/<hash>.cache
suffix: ".cache"
purpose: deterministic restart (prevent RNG exploitation)
```

| Capture point | File | Event |
|---|---|---|
| First load | `src/core/screen_handler.cpp:119-122` | `InitAll()` — initial map load |
| Map switch | `src/core/loading_screen.cpp:174-177` | After loading per-map state (case 2) |
| Fast path load | `src/core/loading_screen.cpp:276-285` | After `PruneDeadEntities()` |
| Restart | `src/ui/pauseMenu.cpp:610-618` | After `RebuildObstacleCache()` |

#### ClearCache()

```
function: WorldgenIO::ClearCache()
file: src/map/worldgenio.cpp:110-122
behavior: deletes ONLY files with ".cache" extension
scope: saves/enemies/ and saves/items/
```

**Known issue (FIXED):** Previously deleted ALL files (both .json and .cache). Now filters by `entry.path().extension() == ".cache"`.

### 1.3 Save/Load System

```
id: save-load-system
status: pre-existing
manual_slot: saves/manual/slot0.json (single slot only)
autosave: saves/autosave/autosave_*.json (max 5 files, timestamp rotation)
per_map_enemies: saves/enemies/<sanitized_path>.json
per_map_items: saves/items/<sanitized_path>.json
worldseed: assets/maps/World_generation/worldseed/save_N/
```

#### ClearSavedState()

```
function: ClearSavedState()
file: src/core/game_state_saver.cpp:838-882
actions:
  - clear in-memory saved state (hasSavedState, player, enemies, items, map)
  - clear worldgenPending flag
  - clear DeadEntities, chest/bomb/crate consumed
  - delete ALL worldseed/save_N/ folders
  - [NEW] g_SeedManager.ResetRun() — reset isRunActive, currentStage, prevStage
callers:
  - mainMenu.cpp:146 — New Game confirmation
  - mainMenu.cpp:176 — Load Game popup cancel  [CONCERN: accidental deletion]
```

### 1.4 Load Game Pipeline (Fast Path)

```
id: load-game-fast-path
file: src/core/loading_screen.cpp:226-297
condition: state->assetsLoaded == true
```

Order:
```
1. UnloadMap()
2. LoadMap(savedMapState.mapPath)
3. BuildMapObjectIndex()
4. [IF worldseed path] RunWorldgen(seed, isBoss) + LoadRuntimeState(stageIdx)  [NEW]
5. SetWorldgenPending(true) [IF worldseed] — skip RestoreDeadEntities
6. RestoreDeadEntities() [IF NOT worldgen]
7. InitAll() — spawn enemies, items, capture cache
8. RestoreGameState(state) — overwrite position/HP/inventory
9. PruneDeadEntities()
10. Re-capture .cache   [NEW]
11. InitMainMenu()
12. PLAY
```

### 1.5 Load Game Pipeline (Normal Map Switch)

```
id: load-game-normal-switch
file: src/core/loading_screen.cpp:93-207
```

Stages:
```
Stage 1: LoadMap + [IF worldseed] RunWorldgen + LoadRuntimeState → BuildMapObjectIndex → SpawnObject
Stage 2: Init player + LoadEnemiesForMap(path.json) / SpawnEnemiesFromMap + LoadItemsForMapDir(path.json) / SpawnItemWave → capture .cache
Stage 3: WriteAutosave + PLAY
```

---

## 2. FILES MODIFIED

### 2.1 Code Changes

| File | Lines | Type | Description |
|---|---|---|---|
| `src/map/worldgenio.cpp` | 110-122 | BUGFIX | `ClearCache()` filter `.cache` only |
| `src/core/loading_screen.cpp` | 234-241 | BUGFIX | Add `RunWorldgen()` + `LoadRuntimeState()` in fast path |
| `src/core/loading_screen.cpp` | 276-285 | IMPROVE | Re-capture `.cache` after fast path load |
| `src/ui/pauseMenu.cpp` | 610-618 | IMPROVE | Re-capture `.cache` at end of restart |
| `src/core/game_state_saver.cpp` | 861 | BUGFIX | Add `g_SeedManager.ResetRun()` in `ClearSavedState()` |
| `include/map/propsbehavior.h` | multiple | IMPROVE | Prop constants changed from `private:` to `public:` for testability |
| `CMakeLists.txt` | 104-124 | ADD | New `test_constants` target |

### 2.2 New Files

| File | Size | Purpose |
|---|---|---|
| `lib/doctest/doctest.h` | ~1.5 MB | Unit test framework v2.4.11 |
| `tests/constants_test.cpp` | ~13 KB | 39 test cases, 135 assertions |
| `.github/workflows/build.yml` | Modified | Added build+test steps for Windows & Linux |

### 2.3 Docs Files

| File | Purpose |
|---|---|
| `docs/item_spawn_rng_issue.md` | Pre-existing: RNG issue documentation |
| `docs/save-system-changelog.md` | Human-readable changelog |
| `docs/save-system-changelog-ai.md` | AI/machine-parseable reference |

---

## 3. BUGS FIXED (Summary Table)

| ID | Name | Root Cause | Fix | File |
|---|---|---|---|---|
| B1 | ClearCache deletes everything | No filename filter | `.extension() == ".cache"` check | `worldgenio.cpp` |
| B2 | Worldgen layout missing on load | Fast path skips RunWorldgen() | Added RunWorldgen + LoadRuntimeState | `loading_screen.cpp` |
| B3 | Crash on 2nd worldgen run | SeedManager::isRunActive not reset | `g_SeedManager.ResetRun()` | `game_state_saver.cpp` |
| B4 | Stale cache after load/restart | Cache not re-captured | Re-capture at end of both paths | `loading_screen.cpp`, `pauseMenu.cpp` |
| B5 | WinMain infinite loop (test) | main→WinMain→main chain | Call doctest::Context::run() directly | `constants_test.cpp` |

---

## 4. CONCERNS FOR FUTURE DEV

### C1 — Cancel Load Destroys Worldseed

```
file: src/ui/mainMenu.cpp:176
code: ClearSavedState() called on Load popup cancel
impact: Accidental worldseed folder deletion
fix: Split ClearSavedState() → ResetMemoryState() + ResetWorldseed()
```

### C2 — Cache & Save Share Directories

```
paths: saves/enemies/ , saves/items/
risk: Cleanup functions targeting wrong files
mitigation: Cache uses ".cache" extension, filtered in ClearCache()
```

### C3 — Single Save Slot

```
current: saves/manual/slot0.json only
future: Need multi-slot UI + per-slot worldseed isolation
blockers: WriteSaveFile()/ReadSaveFile() need path param, UI widgets needed
```

### C4 — Worldgen Slot Cleanup Too Aggressive

```
current: ClearSavedState() deletes ALL worldseed/save_N/*
future: Should only delete the specific slot being overwritten
```

### C5 — WinMain Loop (MinGW-UCRT)

```
platform: Windows + MinGW-UCRT CRT
cause: main() wrapper always calls WinMain()
rule: Never call main() from WinMain(); use Context::run() directly
applies_to: tests/constants_test.cpp
```

---

## 5. PIPELINES

### 5.1 Restart Pipeline

```
RESTART:
  ├── CLEAR: Entities, items, tile props, dead entities, all managers
  ├── SPAWN: SpawnEnemiesFromMap()
  │           ├── worldgen:  RNG + seed (non-deterministic without cache)
  │           └── static:    JSON map data (always deterministic)
  ├── RESTORE: LoadEnemiesForMap(.cache) + LoadItemsForMapDir(.cache)
  │             ├── has cache:  overlay exact state
  │             └── no cache:   fallback SpawnItemWave()
  ├── RESET: Player → startSpawnPos, ResetForNewGame, Init
  ├── INIT:  SpawnObject, RebuildObstacleCache, FlowField.Invalidate
  ├── CACHE: Re-capture .cache for next restart
  └── PLAY
```

### 5.2 Save Pipeline (Pause Menu)

```
SAVE:
  ├── SaveGameState() → fill global structs
  ├── [IF worldgen] WorldgenIO::SaveRuntimeState(currentStage)
  └── WriteSaveFile("saves/manual/slot0.json")
```

### 5.3 Load Pipeline (Fast Path)

```
LOAD (fast path, assetsLoaded=true):
  ├── UnloadMap()
  ├── LoadMap(savedMapState.mapPath)
  ├── BuildMapObjectIndex()
  ├── [IF worldseed path] RunWorldgen(seed, isBoss) + LoadRuntimeState(stageIdx)   [FIX B2]
  ├── SetWorldgenPending(true) [IF worldseed]  →  skip RestoreDeadEntities
  ├── RestoreDeadEntities() [IF NOT worldgen]
  ├── InitAll() → spawn enemies + items, capture .cache
  ├── RestoreGameState(state) → overwrite player position, HP, inventory
  ├── PruneDeadEntities()
  ├── Re-capture .cache   [FIX B4]
  └── PLAY
```

### 5.4 Load Pipeline (Normal Map Switch)

```
LOAD (map switch):
  ├── Stage 1:
  │     LoadMap(pendingMapPath)
  │     [IF worldseed] RunWorldgen(seed, isBoss) + WorldgenIO::LoadRuntimeState(stageIdx)
  │     BuildMapObjectIndex, SpawnObject, RebuildObstacleCache
  ├── Stage 2:
  │     Init player
  │     LoadEnemiesForMap(.json) [fallback: SpawnEnemiesFromMap]
  │     LoadItemsForMapDir(.json) [fallback: SpawnItemWave]
  │     Capture .cache
  └── Stage 3:
        WriteAutosave, PLAY
```

---

## 6. FILE DEPENDENCY MAP

```
pauseMenu.cpp → restart flow
  ├── Entities::Clear()
  ├── itemData.activeItems.clear()
  ├── ClearTileProps()
  ├── SpawnEnemiesFromMap()  [src/entities/enemy.cpp]
  ├── GetCurrentMapPath()    [src/core/map.cpp]
  ├── LoadEnemiesForMap()    [src/entities/enemy.cpp]
  ├── LoadItemsForMapDir()   [src/items/item.cpp]
  ├── SpawnItemWave()        [src/items/item.cpp]
  ├── PlayerInstance.ResetForNewGame()
  ├── SaveEnemiesForMap()    [src/entities/enemy.cpp]
  ├── SaveItemsForMapDir()   [src/items/item.cpp]
  └── SpawnObject() / RebuildObstacleCache()

loading_screen.cpp → load game fast path
  ├── LoadMap() / UnloadMap()
  ├── RunWorldgen()          [src/map/map.cpp]
  ├── WorldgenIO::LoadRuntimeState() [src/map/worldgenio.cpp]
  ├── BuildMapObjectIndex()
  ├── SetWorldgenPending()   [src/core/game_state_saver.cpp]
  ├── RestoreDeadEntities()  [src/core/game_state_saver.cpp]
  ├── InitAll()              [src/core/screen_handler.cpp]
  ├── RestoreGameState()     [src/core/game_state_saver.cpp]
  ├── SaveEnemiesForMap()    [src/entities/enemy.cpp]
  └── SaveItemsForMapDir()   [src/items/item.cpp]

game_state_saver.cpp → ClearSavedState
  ├── g_SeedManager.ResetRun()  [src/core/seedmanager.cpp]
  ├── Entities::ClearDeadEntities()
  ├── chestManager.ResetConsumed()
  ├── bombManager.ResetConsumed()
  ├── crateManager.ResetConsumed()
  └── std::filesystem::remove_all(worldseed/save_N)
```

---

## 7. CONSTANTS REFERENCE (TESTED)

### Player Constants

```
Player::MaxHealth = 100.0f      (player.h:202)
Player::MaxMana   = 100.0f      (player.h:203)
```

### Combat Constants

```
THRUST_DISTANCE      = 16.0f    (combat.cpp:140)
PIERCE_DISTANCE      = 24.0f    (combat.cpp:141)
SLAM_THRUST          = 8.0f     (combat.cpp:142)
Arrow::MaxLifeTime   = 2.0f     (combat.cpp:479)
arrow speed          = 300.0f   (combat.cpp:126)
BOSS_HP_MULTIPLIER   = 2.5f     (enemy_ai.h:34)
BOSS_SPEED_MULTIPLIER = 1.2f    (enemy_ai.h:35)
DEFAULT_ATTACK_COOLDOWN = 1.5f  (enemy_ai.h:110)
BOW_COOLDOWN         = 2.5f     (enemy_ai.h:112)
BOW_PROJECTILE_SPEED = 3.0f     (enemy_ai.h:113)
SPEAR_COOLDOWN       = 2.0f     (enemy_ai.h:116)
SPEAR_RANGE          = 4.0f     (enemy_ai.h:117)
FLAME_COOLDOWN       = 3.0f     (enemy_ai.h:122)
FLAME_AOE_RADIUS     = 3.5f     (enemy_ai.h:124)
FLAME_AOE_DAMAGE     = 12.0f    (enemy_ai.h:125)
FLAME_AOE_DELAY      = 0.5f     (enemy_ai.h:126)
};

HomingProjectile::HOMING_DURATION   = 2.0f   (enemy_ai.h:132)
HomingProjectile::HOMING_SPEED      = 350.0f (enemy_ai.h:134)
HomingProjectile::TRACKING_STRENGTH = 3.0f   (enemy_ai.h:136)
```

### Enemy Base Stats

```
Skeleton  : HP=40,  SPD=90,  DMG=10,  Detection=200, Attack=30
Skeleton2 : HP=60,  SPD=100, DMG=15,  Detection=200, Attack=30
Skeleton3 : HP=80,  SPD=110, DMG=20,  Detection=200, Attack=30
SkeletonBoss: HP=2000, SPD=120, DMG=25, Detection=300, Attack=50
Slime     : HP=30,  SPD=80,  DMG=8,   Detection=150, Attack=25
Slime2    : HP=50,  SPD=90,  DMG=12,  Detection=150, Attack=25
Slime3    : HP=70,  SPD=100, DMG=16,  Detection=150, Attack=25
SlimeBoss : HP=500, SPD=150, DMG=20,  Detection=200, Attack=35
Bat       : HP=15,  SPD=120, DMG=5,   Detection=180, Attack=20
Bat2      : HP=25,  SPD=130, DMG=8,   Detection=180, Attack=20
Bat3      : HP=35,  SPD=140, DMG=12,  Detection=180, Attack=20
Golem     : HP=120, SPD=60,  DMG=20,  Detection=100, Attack=40
Golem2    : HP=160, SPD=70,  DMG=25,  Detection=100, Attack=40
Golem3    : HP=200, SPD=80,  DMG=30,  Detection=100, Attack=40
GolemBoss : HP=500, SPD=200, DMG=35,  Detection=200, Attack=50
```

### Weapon Definitions

```
SWORD        : id=1,  dmg=20, range=40, speed=0.3f,  cooldown=0.5f
SWORD_SILVER : id=2,  dmg=30, range=45, speed=0.25f, cooldown=0.45f
SWORD_GOLD   : id=3,  dmg=40, range=50, speed=0.2f,  cooldown=0.4f
SWORD_BOSS   : id=4,  dmg=60, range=60, speed=0.3f,  cooldown=0.6f
BOW_WOOD     : id=5,  dmg=10, range=200, speed=0.5f, cooldown=1.0f
BOW_SILVER   : id=6,  dmg=15, range=220, speed=0.4f, cooldown=0.9f
BOW_GOLD     : id=7,  dmg=20, range=250, speed=0.35f, cooldown=0.8f
BOW_BOSS     : id=8,  dmg=30, range=300, speed=0.3f,  cooldown=1.2f
SPEAR        : id=9,  dmg=15, range=60, speed=0.5f,  cooldown=0.8f
SPEAR_SILVER : id=10, dmg=22, range=65, speed=0.45f, cooldown=0.7f
SPEAR_GOLD   : id=11, dmg=30, range=70, speed=0.4f,  cooldown=0.6f
STAFF_WOOD   : id=12, dmg=10, range=250, speed=0.6f, cooldown=1.5f
STAFF_SILVER : id=13, dmg=18, range=275, speed=0.5f, cooldown=1.3f
STAFF_GOLD   : id=14, dmg=25, range=300, speed=0.4f, cooldown=1.2f
STAFF_BOSS   : id=15, dmg=30, range=350, speed=0.5f, cooldown=1.5f
SHIELD_WOOD  : id=16, dmg=0,  range=0,  speed=0.0f,  cooldown=0.5f
SHIELD_GOLD  : id=17, dmg=0,  range=0,  speed=0.0f,  cooldown=0.5f
```

### Potion Definitions

```
HEALTH_POTION_SMALL  : id=101, heal=20,  cooldown=0 (consumable)
HEALTH_POTION_MEDIUM : id=102, heal=35,  cooldown=0 (consumable)
HEALTH_POTION_LARGE  : id=103, heal=50,  cooldown=0 (consumable)
MANA_POTION_SMALL    : id=104, mana=15,  cooldown=0 (consumable)
MANA_POTION_MEDIUM   : id=105, mana=30,  cooldown=0 (consumable)
MANA_POTION_LARGE    : id=106, mana=50,  cooldown=0 (consumable)
```

### Prop Constants

```
SpikeManager:
  SPIKE_ACTIVE_MAX      = 6.0f
  SPIKE_ACTIVE_MIN      = 3.0f
  SPIKE_INACTIVE_MAX    = 7.0f
  SPIKE_INACTIVE_MIN    = 4.0f
  SPIKE_DAMAGE          = 10.0f
  SPIKE_DAMAGE_COOLDOWN = 1.0f

BombManager:
  BOMB_EXPLOSION_RADIUS    = 80.0f
  BOMB_DAMAGE              = 25.0f
  BOMB_EXPLOSION_DURATION  = 0.6f

CrateManager:
  CRATE_LOOT_CHANCE = 0.1f

BarrierManager:
  KILL_THRESHOLD = 0.9f
```

### Map Constants

```
FRAME_SIZE            = 32
TOTAL_LAYER           = 9
COLLISION_LAYER_NAME  = "Collision"
COLLISION_LABEL_NAME  = "collision"
SPAWN_OBJECT_NAME     = "spawn"
TILESON_OBJECT_NAME   = "Object Layer 1"
CELL_FINISH           = "finish"
GRID_MAX_X            = 9
GRID_MAX_Y            = 9
SLOT_GRID_SIZE        = 288.0f    (9 * 32)
TOTAL_ROOM_TILE_X     = 31
TOTAL_ROOM_TILE_Y     = 31
ROOM_PIXEL_W          = 992.0f    (31 * 32)
ROOM_PIXEL_H          = 992.0f    (31 * 32)
ROOM_CELL_W           = 3
ROOM_CELL_H           = 3
CENTER_X              = 1
CENTER_Y              = 1
```

### Flow Field / AI Constants

```
FLOW_FIELD_TILE_SIZE        = 32
FLOW_FIELD_CENTER_OFFSET    = 16.0f
FLOW_FIELD_REBUILD_COOLDOWN = 0.3f
FLOW_FIELD_PLAYER_RADIUS    = 10
FLOW_FIELD_RETURN_RADIUS    = 18
STEERING_GRID_RADIUS        = 2
SEPARATION_RADIUS           = 28.0f
SEPARATION_STRENGTH         = 25.0f
MAX_SEPARATION_FORCE        = 30.0f
CELL_SIZE                   = 57    (int, 32 * 1.8f truncates)
```

### Seed / Save Constants

```
SEED_COUNT      = 5    (seedmanager.h:22)
SAVE_VERSION    = 2    (game_state_saver.h:29)
MAX_AUTOSAVE_SLOTS = 5 (game_state_saver.cpp:216)
```
