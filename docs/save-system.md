# Sistem Save/Load

> **Save Format v2** (sejak Task 5 + UUID). File save sebelumnya dengan `version=1` langsung ditolak oleh `ReadSaveFile()` -- version field bertindak sebagai schema guard, memastikan file dengan format lawan tidak pernah masuk ke proses deserialisasi.

## Arsitektur: In-Memory Bridge

Sistem ini menggunakan **struct global C++** sebagai jembatan antara game world dan file di disk:

```txt
[Game World]  <-->  [Global Structs]  <-->  [File JSON di Disk]
                     (di memori)
```

| Fungsi | Apa yang dilakukan |
| --- | --- |
| `SaveGameState()` | Baca player/musuh/item/map dari game world -> tulis ke global structs |
| `WriteSaveFile(path)` | Ambil global structs saat ini -> serialisasi ke file JSON |
| `ReadSaveFile(path)` | Baca file JSON -> deserialisasi ke global structs |
| `RestoreGameState()` | Ambil global structs -> tulis balik ke game world |
| `WriteAutosave(name)` | `SaveGameState()` + `WriteSaveFile()` dalam satu panggilan |

## File Save di Disk

| File | Pemicu | Isi |
| --- | --- | --- |
| `saves/manual/slot0.json` | Pause "Save", Pause "Return to Menu" | Full state |
| `saves/autosave/autosave_DD-MM-YYYY-HH-MM-SS.json` | Map switch, periodic 60s, fresh game | Full state (via `WriteAutosave`). Format timestamp, tidak overwrite. Maksimal 5 file — terlama dihapus. |
| `saves/enemies/<sanitized_map_path>` | Map switch | Posisi/HP/status mati musuh per-map |
| `saves/items/<sanitized_map_path>` | Map switch | Status pickup item per-map |

## Apa yang Disimpan

Semua ada di `slot0.json` (satu file JSON, version=2):

- **Player**: position (x,y), health, maxHealth, mana, maxMana, hotbar[4] (definitionId, amount), bag[12] (definitionId, amount), animation state (state, direction, isDead), active hotbar slot, dashCooldown, manaRegenTimer, attack (serialized sebagai JSON object: active, timer, duration, raycastAngle, center, pressHeld)
- **Musuh**: position, enemyName, currentHP, maxHealth, isAlive, aiState, patrolTarget, patrolTimer, mapObjectID, spawnPoint (sebagai {x, y}), healthRegenTimer, attackCooldownTimer, uuid
- **Item**: position, definitionId, isPickedUp, amount, uuid
- **Map**: mapPath, cameraTarget (x,y), cameraZoom, deadEntities list, chestsOpened list, bombConsumedPositions, crateConsumedPositions, mapHistory stack
- **Settings**: showFPS

## Entity Identity (UUID)

Setiap entitas yang bisa dipersist (musuh, item, bomb, crate) mendapat identifier unik saat spawn. UUID adalah string 32 karakter hex yang dihasilkan oleh `GenerateUUID()` menggunakan `std::random_device` dan `std::mt19937` untuk entropy -- pendekatan yang sama digunakan di berbagai tempat yang membutuhkan stream random deterministik.

| Entitas | Di-set di | Disimpan di |
| --- | --- | --- |
| **Musuh (Enemy)** | `SpawnAtPoint`, `SpawnInRect`, `SpawnBoss` via `SetUUID(GenerateUUID())` | SaveGameState -> WriteSaveFile -> `enemies[].uuid` |
| **Item** | `ItemDataManager::CreateItem()` via `item.uuid = GenerateUUID()` | SaveGameState -> WriteSaveFile -> `items[].uuid` |
| **Bomb** | `BombManager::SpawnBombs()` via `data.tile.uuid = GenerateUUID()` | Per-map file (indirect via TileObject) |
| **Crate** | `CrateManager::SpawnCrates()` via `data.tile.uuid = GenerateUUID()` | Per-map file (indirect via TileObject) |

### Matching Saat Restore

Saat `RestoreGameState()` menjalankan restore entitas, ia menggunakan multi-pass matching:

1. **Pass 1 (UUID)**: Cocokkan UUID dari save dengan UUID entitas yang sudah di-spawn. Jika cocok, restore state langsung. Ini memberikan deterministik lookup -- setiap entitas punya identitas tetap yang tidak bergantung pada urutan spawn.
2. **Pass 2 (Fallback)**: Jika UUID tidak cocok atau kosong (misalnya save lawan / dev migration), gunakan `MapObjectID` + `enemyName` untuk musuh, atau urutan index untuk item.

Pendekatan dua-pass ini memastikan bahwa entitas tetap match ke state yang benar meskipun urutan spawn berubah akibat perubahan spawn logic. Item juga menggunakan pola fallback yang sama: UUID dicocokkan dulu, baru index-based matching sebagai cadangan.

### UUID di Per-Map Files

File per-map (`saves/enemies/<path>.json`, `saves/items/<path>.json`) juga menyertakan UUID. Saat `LoadEnemiesForMap()` merestore musuh, UUID di-set kembali via `SetUUID()`. Ini memastikan kontinuitas identitas entitas melintasi map switch dan full save/load cycles.

## Per-Map Item Persistence

Item map sekarang disimpan per-map ke file sistem filesystem, mengikuti pola yang sama seperti `SaveEnemiesForMap` / `LoadEnemiesForMap`:

| Lokasi | Format | Pemicu |
| --- | --- | --- |
| `saves/items/<sanitized_map_path>.json` | JSON array, setiap item: definitionId, positionX/Y, isPickedUp, amount, uuid | Map switch (via `SaveItemsForMapDir` / `LoadItemsForMapDir`) |

### Detail Implementasi

- **Save** (`SaveItemsForMapDir` di `item.cpp`): Serialisasi `itemData.activeItems` ke JSON array. Atomic write via `.tmp` + rename untuk mencegah korupsi jika proses crash di tengah write -- pola yang sama digunakan di semua penulisan file sistem.
- **Load** (`LoadItemsForMapDir` di `item.cpp`): Baca file per-map, parse JSON array, rekonstruksi setiap `ItemSpawn` termasuk hitbox dari `ItemDefinition`. Fallback ke 16x16 jika definisi tidak ditemukan.
- **Cleanup**: `mainMenu.cpp` menghapus `saves/items/` saat fresh game start via `std::filesystem::remove_all("saves/items")`.

Pendekatan filesystem terpisah ini (vs satu file besar) berarti state item per-map bisa dibaca dan ditulis secara independen tanpa perlu memuat ulang seluruh save. Ini mengikuti arsitektur yang sama seperti file musuh per-map.

## Restore Ordering Fix

### Masalah

Sebelum perbaikan, `InitAll()` di `loading_screen.cpp` memanggil `SpawnEnemiesFromMap()` yang mengecek `IsAlreadyDead()` untuk setiap musuh, tetapi static set `DeadEntities` kosong karena `RestoreGameState()` (yang memanggil `Entities::SetDeadEntities()` dari `savedMapState.deadEntities`) belum dijalankan. Akibatnya, musuh yang sudah mati akan respawn setelah Load Game.

### Perbaikan

1. **`RestoreDeadEntities()` diekstrak** dari `RestoreGameState()` menjadi fungsi mandiri yang dideklarasikan di `game_state_saver.h`. Fungsinya membaca `savedMapState.deadEntities` dan memanggil `Entities::SetDeadEntities()`.
2. **`RestoreDeadEntities()` dipanggil SEBELUM `InitAll()`** di kedua jalur loading_screen.cpp (First Load default case + Fast Path assetsLoaded). DeadEntities menggunakan `std::set` untuk lookup O(log n), memastikan membership checks deterministik tanpa bergantung pada urutan insertion.
3. **`Entities::PruneDeadEntities()` safety net**: Setelah semua spawn dan restore selesai, fungsi ini mengiterasi `EnemyRegistry`, mengecek `IsAlreadyDead(currentMap, MapObjectID)`, dan menonaktifkan survivor yang seharusnya mati.

### Urutan Final

```txt
ReadSaveFile() -> RestoreDeadEntities() -> InitAll() (spawn enemies,
cek DeadEntities) -> RestoreGameState() (restore HP/posisi/timer)
-> PruneDeadEntities() (safety net)
```

Urutan ini menjamin bahwa DeadEntities sudah terisi sebelum spawn logic berjalan, sementara PruneDeadEntities bertindak sebagai jaring pengaman untuk menangani kasus tepi.

## Semua Skenario

### 1. Fresh Game (Tanpa Save File)

Main Menu -> **Start Game** -> LOADING -> Tutorial Map -> PLAY

```txt
Start Game diklik
  +-- HasSaveFile()? TIDAK
  +-- Screen = LOADING
  +-- Loading stages (texture, map, musuh)
  +-- InitAll() -> WriteAutosave("spawn.json") -> PLAY
```

- Tidak ada popup konfirmasi (tidak ada yang di-overwrite)
- Autosave `spawn.json` dibuat di `saves/autosave/spawn.json`

### 2. Fresh Game (Dengan Save File yang Sudah Ada)

Main Menu -> **Start Game** -> popup "Overwrite existing save?"

```txt
Start Game diklik
  +-- HasSaveFile()? YA
  +-- Tampilkan popup dua-tombol: [Start New] [Cancel]
  +-- Klik [Start New]:
       DeleteSaveFile -> ClearSavedState -> LOADING
  +-- Klik [Cancel]:
       Popup disembunyikan, kembali ke main menu
```

- Save file dihapus saat konfirmasi
- Setelah itu berjalan persis seperti Skenario 1

### 3. Bermain: Manual Save

Tekan tombol grave -> Pause Menu -> **Save Game**

```txt
Save Game diklik
  +-- SaveGameState() -> mengisi global structs dari game world
  +-- WriteSaveFile("saves/manual/slot0.json") -> tulis ke disk
  +-- Popup "Game Saved!" muncul
```

- Menyimpan semua ke `saves/manual/slot0.json`

### 4. Bermain: Load Game dari Pause

Tekan tombol grave -> Pause Menu -> **Load Game**

```txt
Load Game diklik
  +-- HasSaveFile()?
       YA -> Tampilkan popup konfirmasi: [Load Save] [Cancel]
       +-- Klik [Load Save]:
            ReadSaveFile() -> screen = LOADING
            +-- Fast path: InitAll() -> RestoreGameState() -> PLAY
       +-- Klik [Cancel]:
            Kembali ke pause menu
       TIDAK -> Popup "No save file found"
```

- **PERINGATAN**: Progress saat ini digantikan oleh saved state
- Tidak ada auto-save sebelum loading (state saat ini hilang)

### 5. Bermain: Return to Main Menu

Tekan tombol grave -> Pause Menu -> **Return to Main Menu**

```txt
Return to Menu diklik
  +-- Popup konfirmasi: "Return to main menu?" + sub-message
  +-- Klik [Confirm]:
       SaveGameState() + WriteSaveFile("saves/manual/slot0.json")
       Screen = MAIN_MENU
```

- **Auto-save ke slot0.json**
- Jika kemudian klik **Load Game** dari main menu, akan load state yang auto-saved ini
- Tidak ada cara untuk "return to menu tanpa save" saat ini

### 6. Main Menu: Load Game

Main Menu -> **Load Game**

```txt
Load Game diklik
  +-- HasSaveFile()?
       TIDAK -> Popup "No save file", klik OK, kembali ke menu
       YA -> ReadSaveFile() -> tampilkan popup konfirmasi
              +-- Klik [Load Save]:
                   screen = LOADING
                   +-- Fast path: InitAll() -> RestoreGameState() -> PLAY
              +-- Klik [Cancel]:
                   ClearSavedState() -> kembali ke menu
```

### 7. Map Switching (Pintu ke Map Baru)

Masuk pintu di game world -> LOADING -> Map Baru -> PLAY

```txt
Masuk pintu di game world
  +-- SwitchMap() menyimpan musuh ke `saves/enemies/` dan item ke `saves/items/`
  +-- Loading screen:
       Stage 0: UnloadMap
       Stage 1: LoadMap(map baru), SetCurrentMapPath
       Stage 2: Clear entity registry
                LoadEnemiesForMap(map baru)?
                  TIDAK -> SpawnEnemiesFromMap() dari spawn points
                  YA -> Musuh direstore dari saved state
                LoadItemsForMap(map baru)?
                  TIDAK -> SpawnItemWave()
                  YA -> Item direstore dari saved state
       Stage 3: Init player di target door, WriteAutosave("quick.json")
                -> PLAY
```

- Entitas mati yang sudah dibunuh di map ini sebelumnya dilacak di set `DeadEntities`
- `SpawnEnemiesFromMap()` melewati spawn point yang `IsAlreadyDead()` mengembalikan true
- File musuh/item per-map adalah cara state bertahan di map yang pernah dikunjungi sebelumnya

### 8. Map Switching (Kembali ke Map Sebelumnya)

Sama seperti #7. `LoadEnemiesForMap()` menemukan file per-map dari kunjungan terakhir, merestore musuh ke state sebelumnya (mati tetap mati, hidup direstore dengan HP/posisi/AI state).

### 9. Save File Corrupt

```txt
Load Game diklik
  +-- ReadSaveFile() mencoba parse JSON
  +-- Exception json::parse_error tertangkap
  +-- Mengembalikan false
  +-- mainMenu: hapus file corrupt, tampilkan popup "corrupted"
  +-- Klik OK -> kembali ke main menu
```

- File **dihapus** untuk mencegah error berulang
- Tidak ada upaya perbaikan -- corrupt = dihapus

### 10. Version Mismatch

Sama seperti #9. `ReadSaveFile()` memeriksa `version == SAVE_VERSION` (saat ini 2). Jika tidak cocok (misalnya file v1 dari versi lama), mengembalikan false. Version field adalah integer yang bertindak sebagai schema guard -- versi yang tidak cocok langsung ditolak sebelum deserialisasi apapun dimulai.

### 11. Periodic Autosave 60 Detik

Saat di PLAY state, setiap 60 detik:

```txt
autosaveTimer += frameTime
if (timer >= 60.0f) {
    WriteAutosave("periodic.json")
    timer = 0
}
```

- Timer **berhenti** saat pause menu aktif
- Timer **di-reset** saat manual save

### 12. Save Error (Disk Penuh / Permission)

```txt
Pause "Save Game" diklik
  +-- WriteSaveFile mengembalikan false
  +-- Popup error "Failed to save game. Check disk space."
```

### 13. Load Saat Save Merujuk ke Map yang Tidak Ada

```txt
RestoreGameState() memeriksa savedMapState.mapPath
  +-- File ada?
       TIDAK -> Fallback ke "assets/maps/tutorial.json" dengan warning log
       YA -> Load secara normal
```

## Ringkasan: Apa yang Tersimpan Kapan

| Event | Apa yang dipersisten |
| --- | --- |
| Pause **Save Game** | Semua ke `slot0.json` |
| Pause **Return to Menu** | Semua ke `slot0.json` (otomatis) |
| **Map switch** | Musuh ke `saves/enemies/<path>.json`, item ke `saves/items/<path>.json`, + full state ke `quick.json` |
| **Timer 60 detik** | Full state ke `periodic.json` |
| **Fresh game start** | Full state ke `spawn.json` |
| **Main menu Load Game** | Baca `slot0.json`, restore semua |

## Worldgen Runtime Persistence

> Sistem save paralel untuk world generation. Stage worldgen memiliki state runtime dinamis (musuh, chest, crate, bomb, item drop, barrier) yang berubah selama satu run. State ini dikelola oleh WorldgenIO, terpisah dari main save system (game_state_saver) yang menangani state player dan non-worldgen.

### File Locations

Data worldgen disimpan di bawah `assets/maps/World_generation/worldseed/`, dalam subfolder `save_N` per slot. Tiap slot berisi file per-stage untuk runtime state dan satu file meta untuk metadata run.

| File | Path | Isi |
| --- | --- | --- |
| **Runtime State** | `worldseed/save_N/runtime.json` | State runtime per stage (chests, crates, bombs, deadEnemies, itemDrops, barrier) |
| **Meta Data** | `worldseed/save_N/meta.json` | Seeds array, currentStage, prevStage, currentSlot |
| **Stage Map** | `worldseed/save_N/maps/stage_M.json` | File map hasil worldgen (copy dari background_map.json dengan texture path yang disesuaikan) |

### Key Data di runtime.json

Tiap stage punya key `"stage_X"` di runtime.json yang berisi:

- **`chests`**: `Vector2[]` — posisi chest yang sudah dikonsumsi
- **`crates`**: `Vector2[]` — posisi crate yang sudah dikonsumsi
- **`bombs`**: `Vector2[]` — posisi bomb yang sudah dikonsumsi
- **`deadEnemies`**: `string[]` — set entity IDs musuh yang sudah mati di stage ini
- **`itemDrops`**: `array` — item yang di-drop di lantai, tiap entry punya `defId`, `amount`, `x`, `y`
- **`barrier`**: `object` — `cleared` (bool) dan `hasReLocked` (bool)

### Key Data di meta.json

| Field | Tipe | Deskripsi |
| --- | --- | --- |
| `seeds` | `uint32[]` | Array 5 seed deterministic untuk tiap stage |
| `currentStage` | `int` | Index stage saat ini (0-4) |
| `prevStage` | `int` | Index stage sebelumnya untuk back navigation (-1 jika tidak ada) |
| `currentSlot` | `int` | Nomor save slot aktif |

### Save Flow

`SaveRuntimeState(stageIndex)` membaca state dari game world dan menulisnya ke `runtime.json`:

1. Baca file `runtime.json` yang sudah ada (atau buat object kosong)
2. Tulis data stage saat ini ke key `"stage_X"`
3. Atomic write ke disk

Dipanggil dari:

- **`WorldgenIO::NextStage()`** — sebelum pindah ke stage berikutnya
- **`WorldgenIO::PrevStage()`** — sebelum kembali ke stage sebelumnya
- **`PauseMenu::HandleButtonClick()`** (baris 359, 386) — barengan dengan `SaveGameState()` + `WriteSaveFile()`, sehingga player state dan worldgen runtime state tersimpan bersama

```txt
Pause "Save Game" (mid-worldgen):
  WorldgenIO::SaveRuntimeState(currentStage)  -> runtime.json
  SaveGameState() + WriteSaveFile(slot0.json) -> slot0.json
```

### Load Flow

`LoadRuntimeState(stageIndex)` membaca data stage tertentu dari `runtime.json` dan meng-overwrite state game world:

1. Baca `runtime.json`
2. Cari key `"stage_X"` yang sesuai
3. Restore chests, crates, bombs consumed positions
4. **Overwrite DeadEntities** dengan set musuh mati stage ini
5. Restore item drops di lantai
6. Restore barrier state (cleared, hasReLocked)

Dipanggil dari `loading_screen.cpp` baris 115, di jalur worldgen map-switch:

```txt
Case 1 loading_screen (worldseed map detected):
  LoadMap()
  RunWorldgen(seed, isBossStage)
  WorldgenIO::LoadRuntimeState(currentStage)  -> overwrite DeadEntities
  SetWorldgenPending(false)
```

### Boundary Antara Sistem

Kedua sistem save berjalan paralel dengan tanggung jawab terpisah:

| Aspek | WorldgenIO | game_state_saver |
| --- | --- | --- |
| **File** | `worldseed/save_N/runtime.json`, `meta.json` | `saves/manual/slot0.json`, `saves/autosave/*.json` |
| **Lingkup** | Per-stage (map objects, enemies, drops, barrier) | Player (HP, mana, inventory, position), non-worldgen world state |
| **DeadEntities** | Overwrite per stage via `LoadRuntimeState` | Simpan/restore via `savedMapState.deadEntities` |
| **Pemicu Save** | Stage transition, pause menu | Pause menu, map switch, autosave timer |
| **Pemicu Load** | Map switch ke worldgen stage | Load Game dari main menu atau pause menu |

Keduanya dipanggil barengan di pause menu (pauseMenu.cpp:359-361), memastikan player state dan worldgen runtime state konsisten saat save.

### SetWorldgenPending Flag

`SetWorldgenPending(bool)` adalah static flag di `game_state_saver.cpp` yang melindungi dead entity restoration dari korupsi antar-stage. Flag ini penting karena ada tiga jalur berbeda yang bisa merestore DeadEntities, dan mereka bisa saling bertabrakan:

**Masalah**: Saat save game mid-worldgen, `savedMapState.deadEntities` di `slot0.json` menangkap snapshot dead entities dari main save system. Jika Load Game dari main menu merestore snapshot ini, lalu player masuk ke worldgen door, `LoadRuntimeState` akan meng-overwrite DeadEntities dengan subset stage saat ini. Tapi antara Load Game dan door entry, snapshot dari slot0 sudah sempat digunakan -- menyebabkan musuh mati dari stage lain ikut terbawa.

**Cara kerja flag**:

1. **Set** di `interaction.cpp` baris 105 — saat player menyentuh worldgen door (sebelum map switch)
2. **Set** di `loading_screen.cpp` baris 224 dan 295 — saat ReadSaveFile membaca save yang mapPath-nya mengandung `"worldseed/save_"` (indikasi save dilakukan mid-worldgen)
3. **Guard** di `loading_screen.cpp` baris 229 dan 300 — `RestoreDeadEntities()` dilewati jika `IsWorldgenPending()` true, karena WorldgenIO yang akan mengisi DeadEntities
4. **Clear** di `loading_screen.cpp` baris 119 — setelah `LoadRuntimeState` selesai dijalankan
5. **Clear** di `mainMenu.cpp` baris 141 — saat New Game (Start Game), bersama `ClearSavedState()`
6. **Clear** di `ClearSavedState()` baris 811 — reset flag untuk fresh start

```txt
Skenario load mid-worldgen:
  Main Menu -> Load Game
    ReadSaveFile() -> savedMapState.mapPath = "worldseed/save_1/maps/stage_1.json"
    SetWorldgenPending(true)           [guard aktif]
    RestoreDeadEntities() dilewati      [aman, LoadRuntimeState akan handle]
    InitAll() + RestoreGameState()      [player direstore dari slot0, DeadEntities kosong]
    PLAY

  Player masuk worldgen door:
    SetWorldgenPending(true)             [guard tetap aktif]
    LOADING -> LoadMap -> RunWorldgen
    WorldgenIO::LoadRuntimeState()       [overwrite DeadEntities dengan subset stage ini]
    SetWorldgenPending(false)            [guard nonaktif]
```

### ClearSavedState Worldseed Cleanup

Saat New Game, `ClearSavedState()` di `game_state_saver.cpp` (baris 813-825) membersihkan folder worldgen:

```cpp
// Iterasi worldseed/save_* dan hapus tiap subfolder
for (auto& entry : std::filesystem::directory_iterator(worldseedDir))
    if (entry.is_directory() && entry.path().string().find("save_") != std::string::npos)
        std::filesystem::remove_all(entry.path());
```

Detail:

- Hanya folder dengan pola `save_` di pathnya yang dihapus
- Root `worldseed/` dipertahankan
- Setiap penghapus dicatat via `TraceLog(LOG_INFO, ...)`
- Dipanggil di jalur Start Game dengan konfirmasi overwrite

Tanpa cleanup ini, worldgen run baru akan mendeteksi `save_1` yang sudah ada dan melanjutkan slot yang sudah ada, bukan memulai run fresh.

### Integrasi slot0.json dengan Worldgen

Save Game mid-worldgen menghasilkan `slot0.json` dengan `savedMapState.mapPath` yang menunjuk ke `worldseed/save_N/maps/stage_M.json`. Saat Load Game dari main menu, loading screen mendeteksi path ini dan:

1. Meng-set `SetWorldgenPending(true)` untuk melewati `RestoreDeadEntities()`
2. Di stage 1 loading, masuk ke jalur worldgen (loading_screen.cpp:110)
3. Menjalankan `RunWorldgen()` dengan seed dari `g_SeedManager`
4. Memanggil `WorldgenIO::LoadRuntimeState()` untuk merestore state stage tersebut

Ini berarti **meta.json harus ada** agar worldgen bisa regenerasi map yang benar dari seed. Jika meta.json hilang (misalnya karena manual cleanup), load save mid-worldgen akan gagal di worldgen detection karena `g_SeedManager.LoadMeta()` tidak dijalankan di jalur load biasa.

## File Source Utama

| File | Peran |
| --- | --- |
| `src/core/game_state_saver.cpp` | Logika save/load inti, JSON I/O, autosave, RestoreDeadEntities |
| `include/core/game_state_saver.h` | Struct state global, deklarasi fungsi, konstanta SAVE_VERSION |
| `include/core/utils.h` | GenerateUUID() -- utility untuk UUID 32-hex-char |
| `src/items/item.cpp` | Per-map item persistence (SaveItemsForMapDir, LoadItemsForMapDir) |
| `src/ui/mainMenu.cpp` | Main menu save-aware Start Game + Load Game, cleanup saves/items/ |
| `src/ui/pauseMenu.cpp` | Pause menu Save/Load/Return to Menu |
| `src/core/loading_screen.cpp` | Integrasi loading screen, stage map switch, panggil RestoreDeadEntities sebelum InitAll |
| `src/map/map.cpp` | Fungsi map switching (SwitchMap, GoBack, InitMap) -- panggil SaveItemsForMapDir |
| `src/entities/enemies/enemy.cpp` | Per-map enemy persistence (SaveEnemiesForMap, LoadEnemiesForMap, SpawnEnemiesFromMap) |
| `src/entities/entities.cpp` | Dead entity registry (RegisterDeath, IsAlreadyDead, PruneDeadEntities) |
| `include/map/propsbehavior.h` | BombManager/CrateManager -- GetConsumedPositions / SetConsumedPositions accessors |
| `src/ui/popup.cpp` | Two-button confirmation popup |
| `src/map/worldgenio.cpp` | Worldgen runtime persistence (SaveRuntimeState, LoadRuntimeState, InitRun, NextStage, PrevStage) |
| `include/map/worldgenio.h` | Deklarasi fungsi WorldgenIO namespace |
| `src/core/seedmanager.cpp` | SeedManager -- seed generation, SaveMeta / LoadMeta, run state management |
| `include/core/seedmanager.h` | Deklarasi SeedManager class, SEED_COUNT constant, g_SeedManager extern |
| `src/systems/interaction.cpp` | Worldgen door handler -- InitRun, SetWorldgenPending(true), NextStage/PrevStage |

## Known Issues / Notices

> Daftar bug yang diketahui dan catatan penting tentang sistem save/load saat ini. Bagian ini **WAJIB** tetap berada di posisi paling akhir dokumen.

| Issue | Status | Detail |
| --- | --- | --- |
| Crash saat load save dari map berbeda | Intermiten | Terjadi secara tidak konsisten saat loading save yang dibuat di map berbeda dari map saat ini. Belum bisa direproduksi secara andal. Kemungkinan terkait dengan enemy/item restore yang merujuk ke entitas yang belum di-spawn. |
| Dead enemies respawn setelah Load Game | Resolved | Disebabkan DeadEntities kosong saat spawn -- diperbaiki dengan `RestoreDeadEntities()` sebelum `InitAll()` dan `PruneDeadEntities()` safety net. Lihat [Restore Ordering Fix](#restore-ordering-fix). |
| UUID matching untuk entitas | Resolved | Semua entitas yang dapat dipersist sekarang memiliki UUID. Multi-pass matching (UUID -> fallback) menangani kompatibilitas save lawan. Lihat [Entity Identity (UUID)](#entity-identity-uuid). |
| Enemy position reset setelah Save/Load | Resolved | UUID matching selalu gagal karena `SpawnEnemiesFromMap()` menghasilkan UUID acak baru setiap spawn. Fallback MapObjectID+Name memiliki `break` sehingga semua enemy dari spawn point yang sama hanya mencocokkan enemy pertama di registry. Diperbaiki dengan `std::unordered_set<Enemy*>` tracker yang memastikan setiap saved enemy mengkonsumsi tepat satu spawned enemy yang berbeda. Lihat [Matching Saat Restore](#matching-saat-restore). |
