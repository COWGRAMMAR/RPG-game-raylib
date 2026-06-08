# Save System -- AI Agent Context

Single source of truth for any AI agent working with the game's save/load system.
All API signatures match the actual codebase at `include/core/savemanager.h` (line numbers referenced).
Version: SNAPSHOT_VERSION = 1 (savemanager.h:50), SAVE_VERSION = 3 (game_state_saver.h:29).

---

## Section 1: API Reference -- SaveManager

**File:** `include/core/savemanager.h` (lines 117-363)
All methods are static. No instance state.

### Capture (read live state into GameSnapshot)

```cpp
// Capture entire current runtime state into a GameSnapshot.
// Reads: PlayerInstance, Entities::GetEnemyRegistry(), itemData.activeItems,
//        chestManager, bombManager, crateManager, barrierManager,
//        Entities::GetDeadEntities(), camera, mapHistoryStack, gState.
static GameSnapshot CaptureSnapshot();

// Capture initial snapshot after first spawn and write to disk.
// Used as restart cache replacement.
// File: saves/slot_N/manual/snapshot_initial.json
static bool CaptureInitialSnapshot(int slot);
```

### Apply (write GameSnapshot into live state)

```cpp
// MUST be called BEFORE InitAll / SpawnEnemiesFromMap / SpawnObject.
// Restores: deadEntities, chest/bomb/crate consumed positions, barrier state.
static void ApplyPreSpawn(const GameSnapshot& snap);

// MUST be called AFTER all spawns complete.
// Full restore: player stats/inventory/position/animation/combat,
// enemies (UUID then MapObjectID+Name), items (full replacement),
// consumed props, barrier, camera, mapHistory.
static void ApplyPostSpawn(const GameSnapshot& snap);

// Partial restore for checkpoint/cache load (map transitions).
// Restores: enemies, items, props (chest/bomb/crate consumed, barrier).
// Does NOT restore: player, camera, mapHistory.
// ApplyPreSpawn() MUST be called BEFORE this function.
static void ApplyCheckpointData(const GameSnapshot& snap);
```

### Serialization

```cpp
static nlohmann::json Serialize(const GameSnapshot& snap);
static GameSnapshot Deserialize(const nlohmann::json& root);
```

### Low-level File I/O (used internally, available publicly)

```cpp
// Atomic write: write to .tmp then rename.
static bool WriteSnapshot(const GameSnapshot& snap, const std::string& path);

// Read and validate. Returns empty GameSnapshot on parse error or version mismatch.
static GameSnapshot ReadSnapshot(const std::string& path);

// Returns true if file exists and non-empty.
static bool HasSnapshot(const std::string& path);
```

### Manual Save/Load (source of truth for full save/load)

```cpp
static bool SaveManual(const GameSnapshot& snap, int slot);    // saves/slot_N/manual/snapshot.json
static GameSnapshot LoadManual(int slot);
static bool HasManual(int slot);
static bool HasAnySave(int slot);    // true if manual OR initial snapshot exists
static bool DeleteSlot(int slotIndex);    // range: 0-11, removes entire slot dir + cleans orphans
```

### Autosave

```cpp
// Captures internally (no snap param). Generates timestamped filename.
// Prunes to max 5 files per slot.
static bool SaveAutosave(int slot);
```

### Checkpoint (per-map cache for door transitions)

```cpp
static bool SaveCheckpoint(const GameSnapshot& snap, const std::string& mapPath, int slot);
static GameSnapshot LoadCheckpoint(const std::string& mapPath, int slot);
static bool HasCheckpoint(const std::string& mapPath, int slot);
```

### Initial Snapshot (restart cache)

```cpp
static bool SaveInitial(const GameSnapshot& snap, int slot);   // saves/slot_N/manual/snapshot_initial.json
static GameSnapshot LoadInitial(int slot);
static bool HasInitial(int slot);
```

### Path Helpers

```cpp
static std::string GetSlotDir(int slot);              // saves/slot_N
static std::string GetManualPath(int slot);           // saves/slot_N/manual/snapshot.json
static std::string GetAutosaveDir(int slot);          // saves/slot_N/autosave
static std::string GetInitialPath(int slot);          // saves/slot_N/manual/snapshot_initial.json
static std::string GetCheckpointPath(const std::string& mapPath, int slot);  // saves/slot_N/checkpoints/{sanitized}.json
```

### Slot Management

```cpp
static bool EnsureDirs(int slot);          // creates manual/ + autosave/ + checkpoints/ subdirs
static void CleanupTmpFiles();             // removes all .tmp files under saves/ recursively
```

### Private Helpers

```cpp
static std::string SanitizePath(const std::string& mapPath);  // / \ : . space -> _
static bool AtomicWrite(const std::string& path, const json& data);  // .tmp + rename
static bool EnsureDir(const std::string& dir);  // create_directories wrapper
```

---

## Section 2: GameSnapshot Schema

**File:** `include/core/savemanager.h` (lines 49-101)

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `SNAPSHOT_VERSION` | `static constexpr int` | `1` | Current snapshot format version |
| `playerPosition` | `Vector2` | `{0,0}` | Player world position |
| `playerHealth` | `float` | `100.0f` | Current HP |
| `playerMana` | `float` | `100.0f` | Current mana |
| `playerMaxHealth` | `float` | `100.0f` | Maximum HP |
| `playerMaxMana` | `float` | `100.0f` | Maximum mana |
| `hotbar[4]` | `InventoryItem[4]` | `{}` | Hotbar inventory slots |
| `bag[12]` | `InventoryItem[12]` | `{}` | Bag inventory slots |
| `animState.state` | `int` | `0` | Player animation state (State enum) |
| `animState.direction` | `int` | `0` | Player direction (Direction enum) |
| `animState.isDead` | `bool` | `false` | Whether player is dead |
| `animState.activeSlot` | `int` | `0` | Active hotbar slot (ItemSlot enum) |
| `dashCooldown` | `float` | `0.0f` | Remaining dash cooldown timer |
| `manaRegenTimer` | `float` | `0.0f` | Delay before mana regen begins |
| `swingAttack` | `nlohmann::json` | `null` | Serialized attack state |
| `enemies` | `std::vector<SavedEnemyState>` | empty | Per-enemy state snapshot |
| `items` | `std::vector<SavedItemState>` | empty | Per-item state snapshot |
| `chestConsumed` | `std::unordered_set<std::string>` | empty | Chest positions already opened |
| `bombConsumed` | `std::unordered_set<std::string>` | empty | Bomb positions already destroyed |
| `crateConsumed` | `std::unordered_set<std::string>` | empty | Crate positions already destroyed |
| `barrierCleared` | `bool` | `false` | BarrierManager cleared flag |
| `barrierHasReLocked` | `bool` | `false` | BarrierManager re-lock flag |
| `deadEntities` | `std::set<std::string>` | empty | Entity identifiers that died |
| `mapPath` | `std::string` | `""` | Current map file path |
| `cameraTarget` | `Vector2` | `{0,0}` | Camera target position |
| `cameraZoom` | `float` | `1.0f` | Camera zoom level |
| `mapHistory` | `std::vector<MapSystem::MapHistoryEntry>` | empty | Map traversal history stack |
| `mapDisplayName` | `std::string` | `""` | Human-readable map name |
| `version` | `int` | `SNAPSHOT_VERSION` | Serialization version for validation |
| `timestamp` | `std::string` | `""` | ISO 8601 capture timestamp |
| `slotIndex` | `int` | `-1` | Save slot index (-1 = unassigned) |
| `worldgenSlot` | `int` | `-1` | Worldgen seed slot (-1 = not worldgen) |
| `stageIndex` | `int` | `-1` | Stage index (-1 = full snapshot, >=0 = worldgen per-stage) |
| `showFPS` | `bool` | `false` | FPS display toggle state |

### SavedEnemyState (enemy.h)

| Field | Type | Description |
|-------|------|-------------|
| `position` | `Vector2` | Enemy world position |
| `enemyName` | `std::string` | Enemy type name (Slime/Skeleton/Wolf) |
| `currentHP` | `int` | Current HP |
| `isAlive` | `bool` | Alive status |
| `maxHealth` | `float` | Maximum HP |
| `aiState` | `int` | AI state enum (IDLE/PATROL/CHASE/ATTACK/RETURN) |
| `patrolTargetX` | `float` | Patrol target X |
| `patrolTargetY` | `float` | Patrol target Y |
| `patrolTimer` | `float` | Patrol wait timer |
| `mapObjectID` | `int` | MapObjectID for matching on restore |
| `spawnPoint` | `nlohmann::json` | Spawn position ({x, y}) |
| `healthRegenTimer` | `float` | Countdown before health regen (default 2.0f) |
| `attackCooldownTimer` | `float` | Remaining cooldown between attacks |
| `uuid` | `std::string` | UUID for cross-save matching |

### SavedItemState (item.h)

| Field | Type | Description |
|-------|------|-------------|
| `position` | `Vector2` | Item world position |
| `isPickedUp` | `bool` | Collected status |
| `definitionId` | `int` | Reference to ItemDefinition |
| `amount` | `int` | Stack count (default 1) |
| `uuid` | `std::string` | UUID for matching |

### swingAttack JSON Schema

Fields serialized from `PlayerInstance.attack`:

| Key | Type | Description |
|-----|------|-------------|
| `active` | `bool` | Whether attack is active |
| `timer` | `float` | Attack timer |
| `duration` | `float` | Attack duration (default 0.9f) |
| `raycastAngle` | `float` | Attack raycast angle |
| `center` | `[float, float]` | Attack center position |
| `pressHeld` | `bool` | Whether attack button is held |

---

## Section 3: Serialization Format

**File:** `src/core/savemanager.cpp` (lines 155-446)

### JSON Structure (produced by Serialize, consumed by Deserialize)

```json
{
    "version": 1,
    "timestamp": "2026-06-08T15:27:19",
    "slotIndex": 0,
    "worldgenSlot": -1,
    "stageIndex": -1,

    "player": {
        "position": [100.0, 200.0],
        "health": 80.0,
        "maxHealth": 100.0,
        "mana": 50.0,
        "maxMana": 100.0,
        "hotbar": [
            {"definitionId": 1, "amount": 1},
            {"definitionId": -1, "amount": 0},
            {"definitionId": -1, "amount": 0},
            {"definitionId": -1, "amount": 0}
        ],
        "bag": [
            {"definitionId": -1, "amount": 0},
            ... 12 items total
        ],
        "animState": {
            "state": 0,
            "direction": 2,
            "isDead": false,
            "activeSlot": 0
        },
        "showFPS": false,
        "dashCooldown": 0.0,
        "manaRegenTimer": 0.0,
        "swingAttack": {
            "active": false,
            "timer": 0.0,
            "duration": 0.9,
            "raycastAngle": 0.0,
            "center": [0.0, 0.0],
            "pressHeld": false
        }
    },

    "enemies": [
        {
            "position": [300.0, 400.0],
            "enemyName": "Slime",
            "currentHP": 30,
            "isAlive": true,
            "maxHealth": 30.0,
            "aiState": 0,
            "patrolTargetX": 0.0,
            "patrolTargetY": 0.0,
            "patrolTimer": 0.0,
            "mapObjectID": 5,
            "spawnPoint": {"x": 300.0, "y": 400.0},
            "healthRegenTimer": 2.0,
            "attackCooldownTimer": 0.0,
            "uuid": "a1b2c3d4-..."
        }
    ],

    "items": [
        {
            "position": [150.0, 250.0],
            "isPickedUp": false,
            "definitionId": 42,
            "amount": 1,
            "uuid": "e5f6g7h8-..."
        }
    ],

    "chestsOpened": ["100_200", "300_400"],
    "bombConsumedPositions": ["500_600"],
    "crateConsumedPositions": ["700_800"],

    "barrier": {
        "cleared": true,
        "hasReLocked": false
    },

    "deadEntities": ["Slime_5", "Skeleton_3"],

    "map": {
        "mapPath": "assets/maps/forest.json",
        "cameraTarget": [100.0, 200.0],
        "cameraZoom": 1.0,
        "mapDisplayName": "Forest of Echoes",
        "mapHistory": [
            {"mapPath": "assets/maps/tutorial.json", "doorName": "door_south"},
            {"mapPath": "assets/maps/forest.json", "doorName": ""}
        ]
    }
}
```

### Version Check Behavior

In `ReadSnapshot()` (savemanager.cpp:459-504):

```
1. If file not found -> return empty GameSnapshot (version = SNAPSHOT_VERSION default)
2. Parse JSON
3. If missing "version" key -> return empty GameSnapshot (version = SNAPSHOT_VERSION default)
4. If version != SNAPSHOT_VERSION (1) -> log warning, return empty GameSnapshot
5. Otherwise -> Deserialize and return populated snapshot
```

The empty GameSnapshot returned on failure has `version = SNAPSHOT_VERSION` (1) because of the default member initializer. The `Deserialize()` function overwrites it from the JSON root via `snap.version = root.value("version", -1)` -- but only if JSON parsing succeeded and version matched. When version mismatches, no deserialization occurs.

**Correction note:** Looking at the actual code, when version mismatches, `ReadSnapshot` returns a default `GameSnapshot()` which has `version = 1` (the C++ default member initializer), NOT `version = -1`. Callers must check `snap.version != GameSnapshot::SNAPSHOT_VERSION` to detect failure.

### Atomic Write Pattern

In `AtomicWrite()` (savemanager.cpp:113-134):

```
1. Delete existing .tmp file if present
2. Write JSON (4-space indent) to path + ".tmp"
3. Flush and close file
4. Rename .tmp to final path
5. Verify final file exists
```

This guarantees the file is never partially written. A crash during write corrupts only the .tmp file. Orphan .tmp files are cleaned by `CleanupTmpFiles()`.

---

## Section 4: Path Conventions

**File:** `src/core/savemanager.cpp` (lines 30-75), `game_state_saver.cpp` (lines 106-122)

### SaveManager Path Helpers

| Helper | Returns | Notes |
|--------|---------|-------|
| `GetSlotDir(slot)` | `saves/slot_N` | Base directory for slot |
| `GetManualPath(slot)` | `saves/slot_N/manual/snapshot.json` | Source of truth for full save |
| `GetAutosaveDir(slot)` | `saves/slot_N/autosave` | Directory containing rotating autosave files |
| `GetInitialPath(slot)` | `saves/slot_N/manual/snapshot_initial.json` | Restart cache replacement |
| `GetCheckpointPath(mapPath, slot)` | `saves/slot_N/checkpoints/{sanitized}.json` | Per-map cache |

### SanitizePath Rules

Replaces: `/` `\` `:` `.` ` ` (space) -> `_`

Example: `assets/maps/forest.json` -> `assets_maps_forest_json`

### Directory Structure

```
saves/
├── slot_0/
│   ├── manual/
│   │   ├── snapshot.json              # Manual save (SaveManager format, v1)
│   │   └── snapshot_initial.json      # Initial snapshot (restart cache)
│   ├── autosave/
│   │   ├── snapshot_DD-MM-YYYY-HH-MM-SS.json  # Autosave (rotating, max 5)
│   │   └── ...
│   └── checkpoints/
│       ├── {sanitized_mapPath}.json   # Per-map state cache
│       └── ...
├── slot_1/ ... (same structure)
├── slot_2/ ...
├── slot_3/ ...
└── slot_4/ ...
```

### Old Format (Backward Compat) Paths

From `game_state_saver.cpp` -- `GetSlotPath(slot, type)`:

| Call | Returns | Notes |
|------|---------|-------|
| `GetSlotPath(0, "manual")` | `saves/slot_0/manual/manual.json` | Old format v3 manual file |
| `GetSlotPath(0, "autosave")` | `saves/slot_0/autosave` | Old format autosave directory |
| `GetSlotPath(0, "checkpoints")` | `saves/slot_0/checkpoints` | Generic fallback pattern |

---

## Section 5: Loading Flows

### 5.1 HandleFastPath (assets already loaded, resume from main menu)

Called when returning to a saved game with map assets still cached.

```
1. UnloadMap()
2. LoadMap(snap.mapPath)
3. LoadWorldgenForSave():
   a. Load meta from worldgen slot
   b. ExtractStageFromPath(snap.mapPath) -> stageIndex
   c. RunWorldgen(seed) -> stamp rooms, spawn items
4. InitAll():
   a. Entities::Clear()
   b. Player::Init()
   c. InitEnemy()
   d. InitItems() -> SpawnAllItems (spawn items from map JSON)
   e. SpawnObject() -> spawn chests, bombs, crates
   f. SpawnEnemiesFromMap() -> spawn enemies (skips if dead entity is in deadEntities set)
   g. SaveInitial() -> writes initial snapshot
5. RestoreGameState():
   a. LoadManual(g_ActiveSaveSlot) -> GameSnapshot
   b. ApplyPostSpawn(snap) -> full restore
```

### 5.2 HandleInitialLoad (first boot, 3-stage FSM)

Used on the very first load into gameplay (from main menu). Implements a 3-state finite state machine:

- **Case 0:** `InitTextures()` -- load all textures, transition to Case 1.
- **Case 1:** `LoadMap(mapPath)` + `LoadWorldgenForSave()`, transition to Default.
- **Default:** `InitAll()` + `RestoreGameState()`, transition to gameplay.

### 5.3 HandleMapSwitch (door/prev stage transition)

Uses checkpoint system for partial state recovery:

```
1. SaveCheckpoint(currentMap) -> cache enemies+items state before leaving
2. LoadMap(newMap)
3. LoadCheckpoint(newMap) -> ApplyPreSpawn(snap)
4. InitAll():
   a. Entities::Clear()
   b. Player::Init()
   c. InitEnemy()
   d. InitItems() -> SpawnAllItems
   e. SpawnObject() -> spawn props
   f. SpawnEnemiesFromMap() -> spawn enemies (skip dead)
5. LoadCheckpoint(newMap) -> ApplyCheckpointData(snap)  // partial restore
```

### 5.4 PRE-SPAWN -> INITALL -> POST-SPAWN Ordering Constraint

This ordering is **critical** for correct state restoration:

```
ApplyPreSpawn(snap)        // 1. Set dead entities + consumed props BEFORE spawn
    |
InitAll():                      // 2. Spawn entities (enemies, items, props)
  - Entities::Clear()
  - Player::Init()
  - InitEnemy()
  - InitItems() -> SpawnAllItems
  - SpawnObject()               // checks consumed positions, skips already-consumed
  - SpawnEnemiesFromMap()       // checks deadEntities set, skips dead
    |
ApplyPostSpawn(snap)       // 3. Override spawned state with snapshot data
```

For map transitions, step 3 uses `ApplyCheckpointData` instead (partial restore, no player/camera/mapHistory).

---

## Section 6: Entity Identity and Matching

### UUID Generation

UUIDs are generated at spawn time for enemies and items:
```
enemy->SetUUID(GenerateUUID());
item.uuid = GenerateUUID();
```

### Enemy Matching (ApplyPostSpawn / ApplyCheckpointData)

Two-pass matching against `Entities::GetEnemyRegistry()`:

```
Pass 1 -- UUID match:
  for each saved enemy:
    for each live enemy:
      if saved.uuid non-empty AND matches live enemy UUID:
        restore position, health, maxHealth, AIState, patrol, spawn point,
        healthRegenTimer, attackCooldownTimer, IsActive=true
        mark as matched

Pass 2 -- MapObjectID + Name fallback:
  for each unmatched saved enemy:
    for each unmatched live enemy:
      if mapObjectID matches AND enemyName matches:
        restore (same fields as Pass 1)
        mark as matched

Dead enemies (isAlive == false):
  RegisterDeath(GetCurrentMapPath(), saved.mapObjectID)
  skip restore
```

### Enemy AIState Clamping

```
enemy->AIState = (EnemyAIState)(saved.aiState < 0 || saved.aiState > 4 ? 0 : saved.aiState);
```
Clamps to valid range 0-4 (IDLE/PATROL/CHASE/ATTACK/RETURN).

### HealthRegenTimer Grace

```
if (saved.healthRegenTimer <= 0.0f && enemy->Health >= enemy->MaxHealth)
    enemy->HealthRegenTimer = 2.0f;
```
Prevents instant regen after load when timer was 0 and enemy is at full health.

### Item Replacement

**ApplyPostSpawn (full restore):** Complete replacement. Snapshot is the source of truth:
```
itemData.activeItems.clear()
for each saved item:
    push fresh ItemSpawn with saved.position, isPickedUp, definitionId, amount, uuid
```

**ApplyCheckpointData (map transition):** Partial restore, preserves existing items:
```
Pass 1: Match by UUID -> restore position, isPickedUp, definitionId, amount
Pass 2 (fallback): Index-based positional match for unmatched items
```

### Dead Entity Filtering

`SpawnEnemiesFromMap()` checks `Entities::IsAlreadyDead(entityId)` before spawning.
If the entity ID is in the dead set, the enemy is not spawned.
Dead entities are restored via `ApplyPreSpawn(snap)` which calls `Entities::SetDeadEntities(snap.deadEntities)`.

---

## Section 7: Cross-Module Dependencies

Every system that SaveManager touches:

### Player (PlayerInstance)

- `PlayerInstance.GetPosition()` / `SetPosition()` -- Vector2 world position
- `PlayerInstance.GetHealth()` / `SetHealth()` -- float current HP
- `PlayerInstance.GetMana()` / `SetMana()` -- float current mana
- `PlayerInstance.MaxHealth` / `MaxMana` -- float max stats
- `PlayerInstance.GetHotbarItem(i)` / `SetHotbarItem(i, item)` -- hotbar slots (4)
- `PlayerInstance.GetBagItem(i)` -- bag slots (12)
- `PlayerInstance.Anim.state` / `direction` / `isDead` -- animation state
- `PlayerInstance.DashCooldown` -- float
- `PlayerInstance.ManaRegenTimer` -- float
- `PlayerInstance.attack.active`, `timer`, `duration`, `raycastAngle`, `center`, `pressHeld` -- attack state

### Input

- `InputInstance.GetActiveSlot()` / `SetActiveSlot(ItemSlot)` -- active hotbar slot (int cast to ItemSlot)

### Entities

- `Entities::GetEnemyRegistry()` -- `std::vector<Enemy*>` reference for capture and restore
- `Entities::GetDeadEntities()` -- `const std::set<std::string>&` current dead set
- `Entities::SetDeadEntities(std::set<std::string>)` -- restore dead set in ApplyPreSpawn
- `Entities::RegisterDeath(mapPath, mapObjectID)` -- mark enemy as dead during restore
- `Entities::ClearDeadEntities()` -- called in `ResetMemoryState()`

### Items

- `itemData.activeItems` -- `std::vector<ItemSpawn>` global item list

### Props

- `chestManager.GetConsumedPositions()` / `SetConsumedPositions(unordered_set)` -- `std::unordered_set<std::string>`
- `bombManager.GetConsumedPositions()` / `SetConsumedPositions(unordered_set)`
- `crateManager.GetConsumedPositions()` / `SetConsumedPositions(unordered_set)`

### Barrier

- `barrierManager.IsCleared()` / `SetCleared(bool)`
- `barrierManager.HasReLocked()` / `SetHasReLocked(bool)`

### Camera (raylib Camera2D)

- `camera.target` -- `Vector2`
- `camera.zoom` -- `float`

### Map History

- `mapHistoryStack.GetAllEntries()` -- returns `std::vector<MapSystem::MapHistoryEntry>`
- `mapHistoryStack.FromVector(vector)` -- restores history from vector

### Map

- `GetCurrentMapPath()` -- `const char*`, current map path (fallback to `assets/maps/tutorial.json` if null or empty)
- `GetMapDisplayName(mapPath)` -- human-readable map name

### Global State

- `gState->showFPS` -- FPS toggle bool

### Seed Manager

- `g_SeedManager.IsRunActive()` -- bool, whether worldgen run is active
- `g_SeedManager.GetCurrentSlot()` -- int, current worldgen slot number
- `g_SeedManager.ResetRun()` -- called in `ResetMemoryState()`

### WorldgenIO

- `WorldgenIO::CleanupOrphanedSlots()` -- called in `DeleteSlot()` to clean up worldseed directories with no referencing save files

---

## Section 8: g_ActiveSaveSlot Contract

**Declared in:** `game_state_saver.h:141`, defined in `game_state_saver.cpp:62`

### Declaration

```cpp
extern int g_ActiveSaveSlot;       // -1 = inactive, 0-4 = valid slot
extern bool g_SaveSlotActive;      // true when slot >=0 && <=4
void SetActiveSlot(int slot);      // sets both g_ActiveSaveSlot and g_SaveSlotActive
int GetActiveSlot(void);
bool IsSlotActive(void);
```

### Who Sets It

| Context | Flow | Slot Value |
|---------|------|------------|
| `SaveLoadScreen` LOAD flow | After `LoadManual(slot)` succeeds | `g_ActiveSaveSlot = slot` (0-4) |
| `SaveLoadScreen` SAVE flow | After `SaveManual(snap, slot)` succeeds | `g_ActiveSaveSlot = slot` (0-4) |
| `mainMenu` NEW GAME | On starting a new game | `g_ActiveSaveSlot = slot` (slot from `GetNextAvailableSlot` or selected) |
| `SaveLoadScreen` / `mainMenu` Return to Menu | On exit to main menu | `SetActiveSlot(-1)` |

### Who Reads It

| Location | Usage |
|----------|-------|
| `SaveManager::CaptureSnapshot()` | Sets `snap.slotIndex = g_ActiveSaveSlot` |
| `SaveManager::SaveAutosave(slot)` | Called with explicit slot (caller passes the value, but it also depends on slot check `if (slot < 0) return false`) |
| `SaveManager::DeleteSlot()` | Resets `g_ActiveSaveSlot` to -1 if the deleted slot matches (`if (g_ActiveSaveSlot == slotIndex)`) |
| `RestoreGameState()` | Uses `g_ActiveSaveSlot` to call `SaveManager::LoadManual(g_ActiveSaveSlot)` |
| `WriteAutosave()` (old path) | Uses `g_ActiveSaveSlot` directly for routing to autosave directory |
| `SaveGameState()` (old path) | Checks `g_ActiveSaveSlot >= 0` before ensuring slot directory |

### Contract Rules

1. **Range:** 0-4 = valid manual slots, -1 = no active slot (inactive).
2. **Return to Menu:** MUST call `SetActiveSlot(-1)` to deactivate.
3. **Autosave:** Writes to the current active slot. If no slot is active, autosave fails.
4. **DeleteSlot:** If the deleted slot is the active one, resets to -1 automatically.
5. **Initial state:** -1 at application start (before any save slot interaction).

---

## Section 9: Known Issues and Edge Cases

### 1. ApplyPreSpawn MUST Be Called Before InitAll

If `ApplyPreSpawn` is not called before `SpawnEnemiesFromMap`, dead enemies will respawn because the dead entities set is empty. Similarly, consumed chests/bombs/crates will spawn again because the consumed positions are not set.

### 2. ApplyPostSpawn MUST Be Called After All Spawns Complete

If `ApplyPostSpawn` is called before spawns finish, spawned entities may overwrite restored state, or enemies may not be found in the registry yet (empty registry at time of match).

### 3. ApplyCheckpointData Replaces ApplyPostSpawn for Map Transitions

Checkpoint loads use partial restore. Player state, camera, and mapHistory are intentionally NOT restored. `ApplyPreSpawn` must still be called before spawns.

### 4. Old Format Backward Compat (game_state_saver.cpp dual-path)

`RestoreGameState()` (game_state_saver.cpp:710) has a dual-path design:

```
Path 1 (NEW -- SaveManager):
  if g_ActiveSaveSlot >= 0:
    snap = SaveManager::LoadManual(g_ActiveSaveSlot)
    if snap.version == SNAPSHOT_VERSION:
      SaveManager::ApplyPostSpawn(snap)
      return  // early exit

Path 2 (OLD -- fallback):
  restore from global variables (savedPlayerState, savedEnemyStates, etc.)
  Player::SetHealth/Mana/Position, hotbar/bag, animState, etc.
```

The old path still runs when `SnapshotVersion` does not match (file missing, old format).

### 5. Independent Version Constants

| Constant | File | Value | Purpose |
|----------|------|-------|---------|
| `SNAPSHOT_VERSION` | savemanager.h:50 | `1` | New format (snapshot.json) via GameSnapshot |
| `SAVE_VERSION` | game_state_saver.h:29 | `3` | Old format (manual.json) via WriteSaveFile/ReadSaveFile |

Each format has its own version check. They are independent. The old format v2 is explicitly unsupported (ReadSaveFile returns false for v2 with a warning).

### 6. Atomic Write (Disk Full Susceptibility)

The atomic write pattern uses `.tmp` + `rename`. This protects against partial writes from crashes, but a disk-full condition during the write phase will still fail (the `ofstream` will not write, file won't exist, and `AtomicWrite` returns false). The rename itself is atomic on most filesystems but disk-full rename failures are rare.

### 7. Empty Slot Handling

`LoadManual(slot)` returns a default `GameSnapshot()` when the file does not exist or version mismatches. The caller must check `snap.version != GameSnapshot::SNAPSHOT_VERSION` to detect failure. The default-constructed `GameSnapshot` has `version = 1` (member initializer), so a missing file is indistinguishable from a valid file with `version=1` -- the caller should check additional fields or use `HasManual(slot)` first.

### 8. Cross-Slot Contamination (Fixed)

The old worldgen system had a bug where `WorldgenIO::SaveRuntimeState(stageIndex)` used `g_ActiveSaveSlot` for path routing. When the player saved to a new slot while in a worldgen run, the previous stage's runtime state was written to the wrong slot. This was fixed by:
- Removing `SaveRuntimeState/LoadRuntimeState` entirely
- Using GameSnapshot as the single source of truth
- Full item replacement (clearing `itemData.activeItems` and rebuilding from snapshot)

### 9. Dead Entity Set Persistence

The dead entities set (`std::set<std::string>`) is restored in `ApplyPreSpawn` and checked in `SpawnEnemiesFromMap`. The entity identifier format is `"EnemyName_MapObjectID"` (e.g., `"Slime_5"`). If the naming convention changes, existing saves may respawn previously dead enemies.

### 10. healthRegenTimer Edge Case

The grace rule (`if timer <= 0.0f && health >= maxHealth -> set to 2.0f`) prevents instant health regen after loading. However, if an enemy genuinely had a timer at 0.0f and was below max health (actively regenerating), this rule does not trigger -- the timer correctly remains at 0.0f and regen continues naturally.

### 11. Default SNAPSHOT_VERSION in Empty Snapshot

A default-constructed `GameSnapshot` has `version = 1` from the member initializer `int version = SNAPSHOT_VERSION`. This means checking `snap.version == SNAPSHOT_VERSION` is not sufficient to verify a load succeeded. Callers must use additional checks like file existence (`HasManual()`) or verify that key fields are populated (e.g., `snap.mapPath` is not empty).

### 12. DeleteSlot Range Check

`DeleteSlot(int slotIndex)` checks `slotIndex < 0 || slotIndex > 11`. This allows slots 0-11, but g_ActiveSaveSlot only supports 0-4. The extra range (5-11) is reserved for future use (e.g., additional manual slots or internal staging slots).

### 13. Autosave Timestamp Format

`SaveAutosave` uses `std::strftime(buf, sizeof(buf), "snapshot_%d-%m-%Y-%H-%M-%S.json", ...)` which produces European date format (DD-MM-YYYY). The autosave pruning matches filenames starting with `"snapshot_"` and having `".json"` extension.

### 14. Map Fallback on Missing File

In `CaptureSnapshot()` (savemanager.cpp:710-711):
```
snap.mapPath = (mapPath && mapPath[0] != '\0') ? std::string(mapPath) : "assets/maps/tutorial.json";
```
If `GetCurrentMapPath()` returns null or empty string, the snapshot defaults to `assets/maps/tutorial.json`.

### 15. SwingAttack JSON Null Handling

If `snap.swingAttack.is_null()` is true, `ApplyPostSpawn` skips the attack state restoration entirely (savemanager.cpp:800). This can happen when loading a snapshot created before the player attacked, or from the default-constructed snapshot.

### 16. Enemy HealthRegenTimer Serialization Default

In `Deserialize`, `healthRegenTimer` defaults to `2.0f` (savemanager.cpp:367) to match the enemy's initial regen delay. In the old format `ReadSaveFile`, the same default applies (game_state_saver.cpp:456). This ensures enemies don't regen immediately after loading if the field was serialized as 0.

### 17. Old Format WriteSaveFile Path

The old `WriteSaveFile()` writes to the path given as parameter. It does NOT route through `GetSlotPath`. Callers (like `WriteAutosave` in game_state_saver.cpp) must construct the full path manually. This is in contrast to `SaveManager` which always routes through path helpers.
