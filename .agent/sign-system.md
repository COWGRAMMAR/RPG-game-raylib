# Sign System — Technical Documentation (AI/Developer)

## Architecture Overview

Sign system allows players to interact with placed signs in the Tiled map, displaying dialog text. The system follows the same pattern as other props (ChestManager, CrateManager, etc.).

### Data Flow

```
Tiled Object (type="sign", property "dialog")
  ↓ LoadMap() → SpawnObject()
  ↓ signManager.SpawnSigns()
  ↓ Parse "dialog" → SplitDialog() → store in SignData::lines
  ↓ Push bounds to DynamicObstacles + cachedObstacleList
  ↓
Player raycast → CheckProps() → detects SIGN_TYPE_OBJECT_NAME
  ↓ InputInstance.IsInteract()
  ↓ signManager.Interact(hitPos)
  ↓ FindSign() → set isDialogActive + activeDialogLines
  ↓ main.cpp detects IsDialogActive → skip UpdateLogicAll()
  ↓ DrawUIOverlay() → DrawSignDialog() → render dim + box + lines
  ↓
Player left-click → main.cpp dismiss check → DismissDialog()
```

## Files

| File | Role |
|------|------|
| `include/map/propsbehavior.h` | `SignManager` class declaration |
| `src/map/propsbehavior.cpp` | `SignManager` implementation |
| `include/rendering/hud.h` | `DrawSignDialog()` declaration |
| `src/rendering/hud.cpp` | `DrawSignDialog()` rendering implementation |
| `src/systems/interaction.cpp` | Sign detection in `CheckProps()` |
| `src/core/main.cpp` | Dialog freeze + dismiss logic |
| `src/entities/entities.cpp` | `signManager.Render()` + `Clear()` integration |
| `src/entities/enemies/enemy_ai.cpp` | Sign in `cachedObstacleList` for enemy pathfinding |
| `include/map/map.h` | `SIGN_TYPE_OBJECT_NAME = "sign"` constant |

## SignManager Class

### Public Methods

| Method | Description |
|--------|-------------|
| `SpawnSigns(vector<MapObject*>&)` | Parse Tiled objects, read `properties["dialog"]`, split into lines, push to `DynamicObstacles` |
| `Interact(Vector2 hitPos)` | Find sign via `FindSign()`, set `isDialogActive` + `activeDialogLines` |
| `Render(Rectangle viewRect)` | Draw placeholder DARKGREEN rect per visible sign |
| `Clear()` | Clear signs list + dismiss dialog |
| `IsDialogActive()` | Returns `true` if a dialog is currently showing |
| `GetActiveDialogLines()` | Returns `const vector<string>&` of pre-split dialog lines |
| `DismissDialog()` | Resets `isDialogActive` + clears `activeDialogLines` |

### Private

- `SignData` struct: `TileObject tile` + `vector<string> lines`
- `isDialogActive` (bool)
- `activeDialogLines` (vector\<string\>)
- `SplitDialog(text)` — static, splits by `|` first (fallback `\n`), trims whitespace
- `FindSign(hitPos, threshold)` — nearest hit-test with distance falloff

## Dialog Delimiter

Two delimiters supported, checked in order:

1. `|` (pipe) — single-line in Tiled editor, explicit split
2. `\n` (newline) — multiline in Tiled editor, natural split

Whitespace is trimmed from each line. Empty lines are discarded.

## Freeze Behavior

When `signManager.IsDialogActive() == true`:

- `UpdateLogicAll()` is skipped in the fixed timestep loop (same as pause menu)
- All runtime pauses: player, enemies, spikes, bombs, combat, items
- Input polling continues (`PollInput()` runs every frame)
- Rendering continues (`DrawRenderTexture()` runs every frame)
- Dismiss check runs every frame in main loop (not inside fixed timestep)
- Pause menu takes priority: if both active, dismiss is blocked

## Integration Points

### DynamicObstacles
In `SpawnSigns()`, each sign's bounds is pushed to `DynamicObstacles` (global `vector<Rectangle>` checked by `IsPositionSafe()`).

### cachedObstacleList (enemy AI)
In `BuildObstacleList()` → `appendType(SIGN_TYPE_OBJECT_NAME)` adds sign objects to the raycast obstacle cache.

### Interaction Flow
In `CheckProps()` at `interaction.cpp`, the type guard was changed from `type != CHEST_TYPE_OBJECT_NAME` to `type != CHEST_TYPE_OBJECT_NAME && type != SIGN_TYPE_OBJECT_NAME`, allowing both chests and signs to be interactable via raycast.

## Tiled Setup

| Property | Value |
|----------|-------|
| Layer | `"object"` (OBJECT_LAYER_NAME) |
| Object type | `"sign"` (SIGN_TYPE_OBJECT_NAME) |
| Custom property | `dialog` (string) — dialog text, use `\|` for newlines |

## Rendering Pipeline

```
DrawRenderTexture()
  → RenderMap()           (layer 1: tiles)
  → BeginMode2D()
    → RenderTileProps()   (layer 2: world-space props)
      → signManager.Render(viewRect)  ← DARKGREEN placeholder
  → EndMode2D()
  → DrawUIOverlay()       (layer 3: screen-space UI)
    → DrawSignDialog()    ← dim + box + lines + hint
```
