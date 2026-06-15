# Enemy Persistence Bug — Investigation & Fix Plan

**Date**: 2026-06-09
**Branch**: nisko/feat/save-load
**Status**: IMPLEMENTED (commit `fc58754` on branch `nisko/feat/save-load`)

---

## Bug Description

When loading a saved game, enemies disappear. Props (blocks, chests, bombs, crates) are restored correctly.

---

## Root Cause Analysis

### Primary Cause: Rectangle Spawn Dead-Entity Poisoning

In `SpawnInRect()` (`src/entities/enemies/enemy.cpp:934`), ALL enemies spawned from the same rectangle share the **same MapObjectID** (= `obj->id`, the Tiled spawn object's ID):

```cpp
enemy->Init(spawnPos, enemyName.c_str(), obj->id, def);
```

When **any** enemy from a rectangle dies, `Enemy::Update()` calls `Entities::RegisterDeath(GetCurrentMapPath(), MapObjectID)` (line 227), which stores `"mapPath_obj-id"` in the global dead set. At load time:

1. `ApplyPreSpawn(snap)` restores the dead set from the snapshot
2. `SpawnEnemiesFromMap()` checks `IsAlreadyDead(GetCurrentMapPath(), obj->id)` — returns **TRUE** for the rectangle's ID
3. The **entire rectangle spawn is skipped**, so ALL enemies from that spawn point (including ones that were alive at save time) never appear
4. `ApplyPostSpawn()` cannot restore them because they were never spawned

**Net effect**: If ANY enemy from a rectangle spawn was killed before saving, ALL enemies from that spawn point vanish on reload.

### Why blocks/chests survive

Props use per-tile position-based consumed sets (e.g., `"grid_x_y"` strings). Partial consumption leaves other tiles intact. The spawn mechanism (`SpawnObject()`) is independent per position, not batched per spawn-point ID.

### Secondary: `RestoreGameState` called twice

The save/load screen (`src/ui/saveLoadScreen.cpp:173`) calls `RestoreGameState(state)` **before** the loading screen rebuilds the world. The loading screen (`HandleFastPath`/`HandleInitialLoad`) then calls it again. The first call operates on the stale/previous map — harmless but wasteful. The second call is the one that matters.

---

## Loading Sequence (relevant code paths)

### HandleFastPath (`src/core/loading_screen.cpp:240`)

```
1. UnloadMap()           → ClearEnemies() → Entities::Clear() (clears registries, NOT dead set)
2. LoadMap(...)          → loads saved map
3. SetCurrentMapPath(...)
4. BuildMapObjectIndex()
5. ApplyPreSpawn(snap)   → Entities::SetDeadEntities(snap.deadEntities) — FULL REPLACE
6. InitAll():
   a. Entities::Clear()
   b. SpawnEnemiesFromMap()  → checks IsAlreadyDead() — DEAD ENEMIES SKIPPED
   c. SpawnObject()          → props restored
7. RestoreGameState(state) → new path: ApplyPostSpawn(snap)
8. PruneDeadEntities()      → deactivates any registry enemy in dead set
```

### ApplyPostSpawn (`src/core/savemanager.cpp:761`)

Enemy matching logic:
1. UUID match (fails — new enemies get fresh UUIDs)
2. Fallback: MapObjectID + Name match
3. Unmatched saved enemies → silently dropped
4. Unmatched spawned enemies → stay with default state (still visible)

### PruneDeadEntities (`src/entities/entities.cpp:178`)

```
for each enemy in registry:
    if IsAlreadyDead(currentMap, enemy.MapObjectID):
        enemy.IsActive = false
        enemy.Health = 0
```

---

## Fix Options

### Option A (Recommended): Per-Enemy Death Tracking

**Problem**: `MapObjectID` is shared among rectangle-spawned enemies but used as unique death key.

**Fix**: Replace `RegisterDeath(GetCurrentMapPath(), MapObjectID)` with a per-enemy unique identifier. Simplest approach: use the enemy's UUID combined with the map path as the death key instead of `mapPath + MapObjectID`.

**Changes**:

1. **`src/entities/entities.cpp`** — `RegisterDeath()`/`IsAlreadyDead()`:
   - Keep the current map+objectID API for compatibility
   - Add a new function `RegisterDeathByUUID(const std::string& mapPath, const std::string& uuid)` that stores `mapPath + "uuid_" + uuid`
   - Add matching `IsDeadByUUID(const std::string& mapPath, const std::string& uuid)`

2. **`src/entities/enemies/enemy.cpp`** — `Enemy::Update()` (line 227):
   - Change `Entities::RegisterDeath(GetCurrentMapPath(), MapObjectID)` to `Entities::RegisterDeathByUUID(GetCurrentMapPath(), GetUUID())`

3. **`src/entities/enemies/enemy.cpp`** — `SpawnEnemiesFromMap()` (line 993):
   - Remove the `IsAlreadyDead` skip for spawn points
   - Instead, after spawning each enemy, mark it inactive if its UUID is in the dead set

4. **`src/core/savemanager.cpp`** — `ApplyPostSpawn()` and `ApplyCheckpointData()`:
   - For `!saved.isAlive` enemies: call `RegisterDeathByUUID` instead of `RegisterDeath`
   - Remove the spawn-point-level dead-entity skip in the restoring loops

### Option B (Simpler): Keep spawn but skip dead individuals

**Changes**:
1. **`SpawnEnemiesFromMap()`**: Remove the `IsAlreadyDead` check for spawn points. Always spawn enemies from ALL rectangles.
2. After spawning, immediately deactivate enemies whose UUID is in `snap.deadEntities` (via a UUID-based lookup).
3. **`ApplyPostSpawn()`**: Keep existing restore logic — matched enemies get restored state, unmatched dead ones are handled by `PruneDeadEntities`.

**Downside**: Wastes spawn overhead for dead enemies.

### Option C (Minimal): Clear dead set per-map

**Problem**: Dead entities accumulate across maps in the global set.

**Fix**: During loading, scope the dead set to the current map only. Before `SpawnEnemiesFromMap()`, call `ClearDeadEntities()` and only re-register deaths for the current map from the snapshot.

**Downside**: This is already how it works (dead set is replaced by `ApplyPreSpawn` with snapshot data). The core issue is the shared MapObjectID, not map-scoping.

---

## Recommendation

**Option A** — change death tracking from spawn-point ID to per-enemy UUID. This cleanly separates the concept of "this enemy instance is dead" from "this spawn point should be disabled."

---

## Files Touched

| File | Change |
|---|---|---|
| `include/entities/entities.h` | Add `RegisterDeathByUUID()` / `IsDeadByUUID()` |
| `include/core/savemanager.h` | Update `ApplyPreSpawn` docstring (no deadEntities restore) |
| `src/entities/entities.cpp` | Implement per-UUID death tracking; `PruneDeadEntities` uses `IsDeadByUUID`; `ClearDeadEntities` clears both sets |
| `src/entities/enemies/enemy.cpp` | `Enemy::Update()` uses `RegisterDeathByUUID`; `SpawnEnemiesFromMap()` removed `IsAlreadyDead` skip |
| `src/core/savemanager.cpp` | `ApplyPreSpawn` — no `SetDeadEntities`; `ApplyPostSpawn`/`ApplyCheckpointData` — deactivate dead enemies directly, not `RegisterDeath` |
| `src/core/loading_screen.cpp` | (No change needed) |

---

## Verification

1. Save game with mixed dead/alive enemies from the same rectangle spawn
2. Load the save — all previously alive enemies should be present with correct state
3. Load a save where all rectangle enemies were dead — none should appear
4. Verify pin-spawned (single) enemies still work correctly
5. Verify map-switch checkpoint restore still works
6. **Build PASS** (commit `fc58754` — confirmed clean build)
