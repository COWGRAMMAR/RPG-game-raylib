# Dungeon — RPG Raylib

> 2D dungeon crawler RPG berbasis C++, Raylib, dan Tiled (.tmj) — dengan tile DDA obstacle system,
> state-machine screen management, inventory crafting, enemy AI berbasis FSM, dan boss encounters.

> **Catatan:** File ini adalah referensi permanent arsitektur & konvensi. Session history & rules AI ada di `AGENTS.md` (root) yang di-`.gitignore`.

---

## Build & Run

```powershell
.\setup.ps1                          # auto-download raylib/tileson/nlohmann-json ke lib/
cmake --preset ninja                  # configure (re-run setelah nambah .cpp)
cmake --build --preset ninja          # build
.\build\bin\main.exe                  # run dari project root (jangan klik exe langsung)
```

**Presets:**
| Preset | Build Type | Deskripsi |
|---|---|---|
| `ninja` | Release | Optimized, fast build |
| `ninja-debug` | Debug | Full debug symbols |
| `ninja-sanitize` | Debug + ASan | AddressSanitizer + UBSan |

**Dependencies:** Raylib 5.5+, Tileson 2.x, nlohmann-json 3.x — semua auto-download oleh `setup.ps1` ke `lib/`.

**Unity Build:** Semua `.cpp` dalam satu translation unit. Nambah file `.cpp` → `cmake --preset ninja` ulang (CMakeLists.txt udah glob).

**Compiler:** Prefer clang++, fallback ke default. ccache otomatis dipake kalo ada.

---

## Direktori Struktur

```
Dungeon/
├── src/                    # Source files (.cpp)
│   ├── core/               # Entrypoint, screen handler, game loop
│   ├── entities/           # Player, enemy, NPC
│   ├── items/              # Item definitions, inventory, crafting, loot
│   ├── map/                # Map loading, collision, rendering
│   ├── systems/            # Combat, movement, AI, dialogue, video player
│   ├── rendering/          # Rendering pipeline, shaders, lighting
│   ├── ui/                 # HUD, menus, buttons, fonts
│   └── debug/              # Debug overlay, logging
├── include/                # Header files (.h) — mirror src/ structure
│   ├── core/
│   ├── entities/
│   ├── items/
│   ├── map/
│   ├── systems/
│   ├── rendering/
│   ├── ui/
│   └── debug/
├── assets/                 # Game assets (auto-copied ke build output)
│   ├── textures/
│   ├── fonts/
│   ├── audio/
│   ├── maps/               # Tiled .tmj files
│   └── video/
├── lib/                    # Third-party (auto-download, gitignored)
├── build/                  # Build output (gitignored)
├── issue/                  # Issue tracking (.md files per issue)
├── docs/                   # Design docs, GDD, PRD
├── plans/                  # Implementation plans
├── .omo/                   # OpenCode agent workspace
├── CMakeLists.txt          # Root build config
├── CMakePresets.json       # Build presets
└── AGENTS.md               # Session-specific (gitignored)
```

---

## Arsitektur

### Entry Point & Game Loop

```
main.cpp → InitScreen() → ScreenHandler → Update(frame) + Draw(frame) per Screen
```

- **`src/core/main.cpp`**: Inisialisasi window (virtual 640x360, `FLAG_WINDOW_RESIZABLE`), SetConfigFlags, InitAudioDevice, InitScreen, game loop.
- **`src/core/screen_handler.cpp`**: State machine management. Screen stack dengan Push/Pop/Replace.
- **`Screen` base class**: Setiap screen punya `Update(f64)`, `Draw(f64)`, `UpdateDraw(f64)` (untuk render texture), `OnEnter`/`OnExit`.

### Screen State Machine

Screen didefinisikan sebagai enum + inheritance:

```
ScreenType::Menu
ScreenType::GamePlay    — main game
ScreenType::Pause       — pause overlay
ScreenType::Inventory   — inventory + crafting
ScreenType::Settings    — settings (tab: Controls, Audio, Video)
ScreenType::BossDefeated
ScreenType::GameOver
ScreenType::Victory
```

`ScreenHandler` maintain vector of `unique_ptr<Screen>`. Implementasi:
- `PushScreen(type)` — push ke stack
- `PopScreen()` — pop dari stack
- `ReplaceScreen(type)` — replace top screen
- `PeekScreen()` / `PeekScreenRaw()` — akses current screen

### Module Dependencies

```
core/ (main, screen_handler)
  ├── map/ (tilemap, collision)
  │     └── entities/ (player, enemy, NPC)
  │           ├── items/ (inventory, crafting, loot)
  │           ├── systems/ (combat, movement, AI, dialogue)
  │           └── ui/ (HUD, menus)
  ├── rendering/ (render pipeline, shaders)
  └── debug/ (debug overlay)
```

### Virtual Resolution

- Logical: 640×360 (didefinisikan di `game_constants.h`: `LOGICAL_WIDTH` / `LOGICAL_HEIGHT`)
- Render ke `RenderTexture2D`, scale dengan `GetScreenToWorldMatrix` untuk mouse/input
- Window resizable, aspect ratio dijaga

---

## Build System Detail

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.21)
project(Dungeon)

# Unity build — all .cpp in one TU
set(CMAKE_UNITY_BUILD ON)

# Auto-glob semua .cpp
file(GLOB_RECURSE SOURCES src/*.cpp)

# Library auto-find via lib/cmake/
# raylib, tileson, nlohmann_json
# C++17 required
```

### Library Management

`setup.ps1` otomatis:
1. Check `lib/` — skip kalo sudah ada
2. Download raylib 5.5+ (Windows: prebuilt .zip, Linux/macOS: build from source)
3. Download tileson 2.x (header-only via CMake FetchContent atau download)
4. Download nlohmann-json 3.x (header-only single include)

**Windows-specific:** `raylib.dll` otomatis di-copy ke `build/bin/` post-build. Assets auto-copy juga.

---

## Coding Conventions (Wajib)

### Naming

| Entitas | Style | Contoh |
|---|---|---|
| Class / Struct | `PascalCase` | `Player`, `BombManager` |
| Function | `PascalCase` | `InitScreen()`, `CheckExplosionCircle()` |
| Variable lokal | `camelCase` | `bombCount`, `playerHealth` |
| Member variable | `PascalCase` | `this->Health`, `this->Position` |
| Enum value | `UPPER_SNAKE_CASE` | `SCREEN_MENU`, `STATE_IDLE` |
| Namespace | `PascalCase` | `ExplosionUtils`, `ItemDB` |
| Constant | `UPPER_SNAKE_CASE` | `BOMB_DAMAGE`, `TILE_SIZE` |
| Parameter | `camelCase` | `targetPos`, `deltaTime` |
| Getter | `GetX()` | `GetPosition()`, `GetHealth()` |
| Setter | `SetX(val)` | `SetPosition(val)` |
| Boolean getter | `IsX()` / `HasX()` | `IsAlive()`, `HasKey()` |

### Struktur File

```
/*===== [SECTION NAME] =====*/

// Baris kosong antar function
void FunctionA() { ... }

void FunctionB() { ... }

/*===== [ANOTHER SECTION] =====*/
```

- **Brace `{`**: Baris yang sama untuk `if/for/while`; baris baru untuk function/class definition
- **Single-line `if`**: Boleh tanpa brace kalo body pendek dan jelas

### Komentar

- **Bahasa: Indonesia** — semua komentar pake Bahasa Indonesia
- **Docstring `/** */`**: Hanya untuk function publik yang non-obvious
- **Inline `//`**: Untuk logika yang butuh penjelasan, bukan yang self-explanatory

### Praktik Lain

- **Early return** diutamakan daripada nested `if`
- **Cast eksplisit**: `(int)`, `(float)` — BUKAN `static_cast`
- **Null pointer**: `nullptr` — BUKAN `NULL`
- **Inisialisasi member**: Di declaration kalo bisa (`bool x = false`)
- **Shader reload**: Tombol R reload shader otomatis
- **FPS toggle**: F4 toggle FPS display
- **No tests, no linter, no typechecker** — pure C++ with raylib

---

## Sistem-Sistem Utama

### 1. Screen Management (`core/screen_handler.h`)

Screen stack dengan lifecycle events. Setiap screen inherit dari `Screen`:

```cpp
class Screen {
    virtual void Update(f64 deltaTime);
    virtual void Draw(f64 deltaTime);
    virtual void UpdateDraw(f64 deltaTime); // untuk render texture
    virtual void OnEnter();                 // pas screen masuk stack
    virtual void OnExit();                  // pas screen keluar stack
    virtual ~Screen() = default;
};
```

Pattern: `ReplaceScreen` untuk navigasi utama (menu→gameplay), `PushScreen` untuk overlay (pause, inventory), `PopScreen` untuk kembali.

### 2. Collision System (`systems/combat.h/cpp`)

- **`gCollisionCache`**: Global collision cache. Struct `CollisionCache` dengan `std::vector<Rectangle> rects` — di-populate dari Tiled object layer.
- **`DynamicObstacles`**: Runtime obstacles (barriers, crates, boss walls).
- **Fungsi bantuan**: `IsOnScreen()`, `IsWalkable()`, `IsBlocked()`, `IsPositionInsideWall()`, `GetDynamicObstacles()`, `CombineObstacles()`.

### 3. Explosion / Bomb System (`map/`, `items/bomb_manager.h`)

Explosion flow via `BombManager::Explode()` — 6 phase:
1. Collect all bombs in chain
2. Cluster by distance
3. Crate destruction (radius + shadow occlusion via tile DDA)
4. Entity damage per bomb using `CheckExplosionCircle`
5. Accumulate hits per entity per cluster
6. Apply `TakeDamage(count × BOMB_DAMAGE)` once

**`ExplosionUtils` namespace:**
- `IsLineBlockedByObstacles(startTile, endTile, obstacles)` — Bresenham tile DDA, skip start/end tile
- `CheckExplosionCircle(bombCenter, radius, solidObstacles)` — cek 4 kuadran dengan ray
- `ClusterByDistance(positions, threshold)` — grouping bombs by proximity

### 4. Melee Combat (`systems/combat.h/cpp`)

- Slash attack dengan per-entity LOS (Bresenham tile DDA, shared dengan bomb system)
- Sword slash animation (arc visual)
- Damage berdasarkan `PlayerAttack` stats
- `GetMeleeHitEntities()` — filter by range + LOS

### 5. Inventory & Crafting (`items/inventory.h/cpp`)

- Slot-based inventory (hotbar 1-4 + backpack)
- Item stacking
- Crafting system dengan recipe
- Drag & drop (planned)
- Item database via `ItemDB` namespace

### 6. Enemy AI (`systems/enemy_ai.h/cpp`)

FSM-based enemy behavior:
- `STATE_IDLE` — patrol / wait
- `STATE_CHASE` — pursue player
- `STATE_ATTACK` — melee / ranged
- `STATE_HURT` — knockback stun
- `STATE_DIE` — death animation + loot drop

Per-enemy AI parameter: detection range, attack range, speed, patrol path.

**Boss AI:** Multi-phase, barrier management, unique attack patterns.

### 7. HUD System (`ui/hud.h/cpp`)

`DrawPlayerHUD()` — refactored into 5 sub-functions:
- `DrawPauseKeycap`, `DrawInteractKeycap`, `DrawInvDropKeycaps`
- `DrawKillCount` (via `EnemyRegistry`)
- Slot numbers 1-4 below hotbar, stack counter (text only, no black bg)
- FPS di (190, 10)
- Font: `FontId::HUD_PLAYER` (Poppins-Bold, RES_256)
- 3 static Textures (bagIcon, settingsIcon, killCount) ready for asset load

### 8. UI Elements (`ui/`)

- **Button**: Clickable, hover, disabled states. `DrawButtonRounded()`, `DrawButtonOutline()`.
- **Font system**: `ScreenFonts` loader. `GetScaledFont()` untuk dynamic sizing.
- **Menu**: Main menu, pause menu, settings tabs.

### 9. Map System (`map/`)

- Tiled .tmj format via Tileson
- Layer parsing: tile layers, object layers (collision, spawn points, triggers)
- Tile DDA utility untuk obstacle checks (shared dengan combat)
- Per-map properties untuk game logic triggers

### 10. Dialogue System (`systems/dialogue.h/cpp`)

- NPC dialogue dengan branching
- JSON-driven dialogue data
- Typewriter text effect
- Response options

### 11. Loot System (`items/loot.h/cpp`)

- Enemy death → loot table roll
- Drop items via `LootDrop` component
- Auto-pickup atau manual pickup

---

## Key Constants (`include/core/game_constants.h`)

| Constant | Value | Deskripsi |
|---|---|---|
| `LOGICAL_WIDTH` | 640 | Virtual width |
| `LOGICAL_HEIGHT` | 360 | Virtual height |
| `TILE_SIZE` | 16 | Pixels per tile |
| `BOMB_DAMAGE` | 100 | Base bomb damage |
| `PLAYER_SPEED` | 200 | Movement speed (px/s) |
| `FIRE_RATE` | 0.25 | Seconds between shots |

---

## .gitignore Rules

```
build/                  # Build output
lib/                    # Third-party libs
.vs/                    # Visual Studio
.vscode/                # VS Code (kecuali settings)
*.user                 # VS user files
CMakeUserPresets.json   # User-specific CMake presets
*.db                   # Database files
*.svn                  # Subversion

# AI agent workspace
.omo/

# Issue tracking
issue/

# Project docs
docs/
plans/

# Ignored: AGENTS.md (agent-specific tracking)
AGENTS.md
```

**Catatan:** `AGENTS.md` ada di `.gitignore` karena isinya session-specific dan berubah tiap sesi. Jangan commit file ini. Dokumentasi yang di-track ada di `docs/` dan `issue/`.

---

## Issue Tracking Convention

Issue ditulis sebagai file `.md` di `issue/` dengan format:

- `(V) <nomor> <judul>` — Selesai
- `(!) <nomor> <judul>` — Belum dikerjakan / blocking
- `(TESTING) <nomor> <judul>` — Perlu testing

Referensi antar-issue: `#<nomor>`. Setiap issue mencakup deskripsi, acceptance criteria, dan technical notes.

File rencana implementasi di `plans/` dengan format `.omo/plans/<name>.md`.

---

## Troubleshooting

| Problem | Solution |
|---|---|
| `Could not open` file | Jalankan dari project root (`Dungeon/`) — path relatif ke assets |
| `cmake --preset` error | Cek `CMakePresets.json`, pastiin `lib/` sudah terisi |
| `raylib.dll` missing | Post-build step gagal, manual copy `lib/raylib/bin/raylib.dll` ke `build/bin/` |
| Link error undefined | Unity build — cek semua `.cpp` ke-glob. Pastiin file baru ada di `src/` |
| Shader error | Tekan R reload. Cek path shader relatif ke `build/bin/` |
| Crash di bomb explosion | Cek obstacle list — `gCollisionCache.rects` mungkin belum populate |
| Audio gak bunyi | Cek `InitAudioDevice()` sukses, format file didukung |

---

## Dev Notes

- **Window**: `FLAG_WINDOW_RESIZABLE` + virtual 640x360 dengan render texture scaling
- **Shader hot-reload**: Tombol R reload shader runtime (fragment + vertex)
- **Debug FPS**: F4 toggle, render via `DrawFPS()`
- **Audio**: Raylib audio native, support .ogg/.wav/.mp3
- **Tileson**: `tson::Tileson` untuk parse .tmj — map layers, object layers, properties
- **Font system**: `ScreenFonts` menggunakan raylib font loader, `GetScaledFont(size)` untuk responsive font sizing
- **C++17**: `std::optional`, `std::variant`, structured bindings, `if constexpr` tersedia
- **Global collision cache**: Collision data dari Tiled object layer di-cache di `gCollisionCache` untuk performance

---

## RULES FOR AI

- selalu prioritaskan mode plan sebelum mulai esekusi
- jangan langsung build tanpa konfirmasi dari gw
- sangat dianjurkan untuk bertanya jika ada ambigu atau tidak jelas
- jika bisa dikerjakan secara pararel kerjakan saja
- jika saat mode pararel agent dalam kurun waktu 5 menit agent tidak melakukan apa apa, segeran cancel dan kerjakan manual. jika agent pararel benar benar mengerjakan tugasnya jangan di cancel. biarkan sampai kelar task nya
- jika gw ngomong compact atau sejenisnya langsung refer file ini dan tulis session log nya TEPAT di bawah baris rules ini
- jika session log sebelumnya dirasa sudah tidak relevan diperbolehkan overwrite
- jika aku bilang bahwa terdapat pengerjaan pararel di sesi lain kasih tau gw apakah file yang akan lu ubah bakal bentrok dengan file yang lagi dikerjain di sesi lain
- jika gw bilang cek issue dan sejenisnya langsung refer file ini. jika sudah ketemu root cause nya atau diskusikan lagi tulis kedalam bentuk file.md di folder issue
- jika gw sedang nge planning fitur baru atau cara membenarkan bug dari folder issue yang dirasa cukup berat designya langsung tulis plan tersebut di folder plans jika sudah di acc ama gw
