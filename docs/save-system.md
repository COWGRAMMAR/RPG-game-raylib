# Sistem Save/Load

> **Save Format v3** (sejak Wave 2 save enhancement). File save sebelumnya dengan `version=1` atau `version=2` langsung ditolak oleh `ReadSaveFile()` -- version field bertindak sebagai schema guard, memastikan file dengan format lawan tidak pernah masuk ke proses deserialisasi. Lihat [Migration v2->v3 Pipeline](#migration-v2-v3-pipeline) untuk upgrade dari format lama.

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
| `WriteAutosave(name)` | `SaveGameState()` + tulis ke `saves/slot_N/autosave/` dengan timestamp, lalu prune ke 5 terbaru |
| `SetActiveSlot(slot)` | Set slot save yang aktif (0-4) untuk operasi save/load selanjutnya |
| `GetActiveSlot()` | Dapatkan nomor slot aktif (-1 jika tidak ada) |
| `GetSlotPath(slot, type)` | Dapatkan path file save untuk slot dan tipe ("manual" / "autosave") |
| `EnsureSlotDirectory(slot)` | Buat struktur direktori `saves/slot_N/manual/`, `autosave/`, `enemies/`, `items/` |
| `GetMapDisplayName(mapPath)` | Ekstrak nama map human-readable dari file path untuk preview UI |
| `NeedsMigration()` | Periksa apakah migrasi v2->v3 diperlukan (cek sentinel) |
| `RunMigration()` | Jalankan pipeline migrasi v2->v3 (copy + rename + cleanup) |
| `ResetMemoryState()` | Reset state memory tanpa hapus worldseed directories |
| `ResetWorldseed(slotIndex)` | Hapus folder `worldseed/save_{slotIndex}` untuk fresh worldgen start |

## Layout Direktori Per-Slot

Mulai save format v3, semua data game disimpan dalam direktori per-slot di bawah `saves/`. Setiap slot (0-4) memiliki struktur direktori terisolasi:

```
saves/
├── slot_0/
│   ├── manual/
│   │   └── manual.json          # Full state (version=3)
│   ├── autosave/
│   │   ├── autosave_01-01-2025-12-00-00.json
│   │   ├── autosave_01-01-2025-12-05-00.json
│   │   └── ...                  # Maksimal 5 file, terlama dihapus
│   ├── enemies/
│   │   └── <sanitized_map_path>.json
│   └── items/
│       └── <sanitized_map_path>.json
├── slot_1/
│   └── ...
├── slot_4/
│   └── ...
└── .migration_completed_v3      # Sentinel migrasi v2->v3
```

| Path | Pemicu | Isi |
| --- | --- | --- |
| `saves/slot_N/manual/manual.json` | Pause "Save", Pause "Return to Menu" | Full state (version=3) |
| `saves/slot_N/autosave/autosave_DD-MM-YYYY-HH-MM-SS.json` | Map switch, periodic 60s, fresh game | Full state. Format timestamp, tidak overwrite. Maksimal 5 file -- terlama dihapus. |
| `saves/slot_N/enemies/<sanitized_map_path>.json` | Map switch | Posisi/HP/status mati musuh per-map |
| `saves/slot_N/items/<sanitized_map_path>.json` | Map switch | Status pickup item per-map |

`EnsureSlotDirectory(slot)` membuat keempat subdirektori ini untuk slot tertentu. `GetSlotPath(slot, "manual")` mengembalikan `saves/slot_N/manual/manual.json`, sedangkan `GetSlotPath(slot, "autosave")` mengembalikan path ke direktori `saves/slot_N/autosave/`.

### Isolasi Per-Slot

Setiap slot benar-benar terisolasi. Save di slot 0 tidak mempengaruhi slot 1. Ini dicapai dengan:

1. **Path routing**: Semua operasi save/load menggunakan `GetSlotPath()` yang menghasilkan path berdasarkan slot aktif.
2. **Active slot tracking**: `SetActiveSlot(N)` dan `GetActiveSlot()` mengelola slot yang sedang digunakan.
3. **Per-slot worldgen mapping**: `worldgenSlot` di `manual.json` memetakan slot save ke folder `worldseed/save_N/`.
4. **Global variable**: `g_ActiveSaveSlot` (int) dan `g_SaveSlotActive` (bool) melacak slot aktif secara global.
5. **Reset terpisah**: `ResetMemoryState()` hanya reset state memory. `ResetWorldseed(slotIndex)` membersihkan worldgen untuk slot tertentu saja.

### Slot Valid

- Slot 0-4: Manual save slots (bisa save dan load). Nama file: `saves/slot_N/manual/manual.json`
- Slot 5-9: Autosave slots (hanya load, tidak bisa di-save manual). Data autosave disimpan di `saves/slot_N/autosave/`
- Slot -1: Tidak ada slot aktif (autosave tidak akan berjalan)

## Active Slot Tracking

Slot aktif dilacak secara global melalui dua variabel:

| Variabel | Tipe | Deskripsi |
| --- | --- | --- |
| `g_ActiveSaveSlot` | `int` | Nomor slot aktif (0-4 valid, -1 = tidak ada) |
| `g_SaveSlotActive` | `bool` | true jika `g_ActiveSaveSlot` di range 0-4 |

### Fungsi

| Fungsi | Deskripsi |
| --- | --- |
| `SetActiveSlot(int slot)` | Set slot aktif. Slot 0-4 valid, -1 untuk nonaktifkan. Dipanggil saat manual save, manual load, autosave, new game. Reset ke -1 saat kembali ke main menu. |
| `GetActiveSlot()` | Return `g_ActiveSaveSlot` (-1 jika tidak ada) |
| `IsSlotActive()` | Return true jika ada slot yang aktif |

### Alur Tracking

```txt
SaveLoadScreen: pilih slot -> SetActiveSlot(slot) -> SaveGameState()
WriteAutosave:  cek g_ActiveSaveSlot >= 0 -> EnsureSlotDirectory -> tulis autosave
Return to Menu: SaveGameState + WriteSaveFile -> SetActiveSlot(-1)
New Game:       SetActiveSlot(0) -> ResetMemoryState() -> ResetWorldseed(0)
```

## Autosave Per-Slot Rotation

`WriteAutosave()` menulis state game ke direktori autosave per-slot dengan mekanisme rotasi:

```cpp
bool WriteAutosave(const std::string& filename)
{
    if (g_ActiveSaveSlot < 0) return false;     // Tidak ada slot aktif, skip

    EnsureSlotDirectory(g_ActiveSaveSlot);        // Buat direktori jika belum ada
    SaveGameState(gState);                        // Snapshot state

    // Nama file: autosave_DD-MM-YYYY-HH-MM-SS.json
    // Format timestamp mencegah overwrite
    std::string path = GetSlotPath(g_ActiveSaveSlot, "autosave");
    path += "/autosave_" + timestamp + ".json";

    // Atomic write via .tmp + rename
    WriteSaveFile(path);

    // Prune: hanya 5 file terbaru yang dipertahankan
    // File terlama dihapus
    SortDescending(autosaveFiles);
    if (autosaveFiles.size() > 5)
        for (int i = 5; i < autosaveFiles.size(); i++)
            std::filesystem::remove(autosaveFiles[i]);
}
```

Detail:

- **Timestamp format**: `DD-MM-YYYY-HH-MM-SS` (menit, bukan detik -- memberikan resolusi 1 menit)
- **Prune threshold**: 5 file per slot, berdasarkan last_write_time
- **Atomic write**: Semua file ditulis via `.tmp` + rename untuk mencegah korupsi
- **Slot guard**: Jika tidak ada slot aktif (g_ActiveSaveSlot < 0), fungsi return false tanpa error
- **Pemicu**: Map switch, periodic 60s timer, fresh game start
- **Tidak ada overwrite**: Setiap autosave adalah file baru dengan timestamp unik

## GetMapDisplayName

Fungsi utility untuk menghasilkan nama map human-readable dari file path. Digunakan oleh SaveLoadScreen untuk preview:

```txt
"assets/maps/tutorial.json"      -> "tutorial"
"assets/maps/forest/area1.json"  -> "area1"
"worldseed/save_1/maps/stage_1.json" -> "stage_1" (via extname cleanup)
```

Fungsi ini dipanggil di `SaveGameState()` untuk mengisi `savedPlayerState.mapDisplayName`, yang kemudian diserialisasi ke `manual.json` sebagai field `mapDisplayName`.

## Apa yang Disimpan

Semua state game disimpan di `saves/slot_N/manual/manual.json` (satu file JSON, version=3):

- **Player**: position (x,y), health, maxHealth, mana, maxMana, hotbar[4] (definitionId, amount), bag[12] (definitionId, amount), animation state (state, direction, isDead), active hotbar slot, dashCooldown, manaRegenTimer, attack (serialized sebagai JSON object: active, timer, duration, raycastAngle, center, pressHeld), **slotIndex** (0-4), **saveType** ("manual"/"autosave"), **playTime** (placeholder), **mapDisplayName** (nama map human-readable untuk preview UI), **worldgenSlot** (mapping ke folder worldseed/save_N)
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

File per-map (`saves/slot_N/enemies/<path>.json`, `saves/slot_N/items/<path>.json`) juga menyertakan UUID. Saat `LoadEnemiesForMap()` merestore musuh, UUID di-set kembali via `SetUUID()`. Ini memastikan kontinuitas identitas entitas melintasi map switch dan full save/load cycles.

## Per-Map Item Persistence

Item map sekarang disimpan per-map ke file sistem filesystem, mengikuti pola yang sama seperti `SaveEnemiesForMap` / `LoadEnemiesForMap`:

| Lokasi | Format | Pemicu |
| --- | --- | --- |
| `saves/slot_N/items/<sanitized_map_path>.json` | JSON array, setiap item: definitionId, positionX/Y, isPickedUp, amount, uuid | Map switch (via `SaveItemsForMapDir` / `LoadItemsForMapDir`) |

### Detail Implementasi

- **Save** (`SaveItemsForMapDir` di `item.cpp`): Serialisasi `itemData.activeItems` ke JSON array. Atomic write via `.tmp` + rename untuk mencegah korupsi jika proses crash di tengah write -- pola yang sama digunakan di semua penulisan file sistem.
- **Load** (`LoadItemsForMapDir` di `item.cpp`): Baca file per-map, parse JSON array, rekonstruksi setiap `ItemSpawn` termasuk hitbox dari `ItemDefinition`. Fallback ke 16x16 jika definisi tidak ditemukan.
- **Cleanup**: `mainMenu.cpp` menghapus `saves/slot_0/enemies/` dan `saves/slot_0/items/` saat fresh game start via `EnsureSlotDirectory(0)` yang mengosongkan direktori. Juga memanggil `ResetWorldseed(0)` untuk membersihkan folder worldgen.

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
- Autosave `spawn.json` dibuat di `saves/slot_N/autosave/spawn.json` (slot ditentukan oleh slot aktif)

### 2. Fresh Game (Dengan Save File yang Sudah Ada)

Main Menu -> **Start Game** -> popup "Overwrite existing save?"

```txt
Start Game diklik
  +-- HasSaveFile(GetSlotPath(0, "manual"))? YA
  +-- Tampilkan popup dua-tombol: [Start New] [Cancel]
  +-- Klik [Start New]:
       DeleteSaveFile(GetSlotPath(0, "manual"))
       SetActiveSlot(0)
       ResetMemoryState()
       ResetWorldseed(0)
       Screen = LOADING
  +-- Klik [Cancel]:
       Popup disembunyikan, kembali ke main menu
```

- Save file slot 0 dihapus saat konfirmasi, worldseed dibersihkan
- Setelah itu berjalan persis seperti Skenario 1

### 3. Bermain: Manual Save

Tekan tombol grave -> Pause Menu -> **Save Game**

```txt
Save Game diklik
  +-- Buka SaveLoadScreen dalam SAVE_MODE
  +-- Player pilih slot (0-4)
  +-- SetActiveSlot(slot terpilih)
  +-- SaveGameState() -> mengisi global structs dari game world
  +-- WriteSaveFile(GetSlotPath(slot, "manual")) -> tulis ke saves/slot_N/manual/manual.json
  +-- Popup "Game Saved!" muncul
```

- Menyimpan semua ke `saves/slot_N/manual/manual.json`, N tergantung slot yang dipilih

### 4. Bermain: Load Game dari Pause

Tekan tombol grave -> Pause Menu -> **Load Game**

```txt
Load Game diklik
  +-- Buka SaveLoadScreen dalam LOAD_MODE
  +-- Player pilih slot manual (0-4) atau autosave (5-9)
  +-- Popup konfirmasi: [Load Save] [Cancel]
  +-- Klik [Load Save]:
       SetActiveSlot(slot terpilih)
       ReadSaveFile(GetSlotPath(slot, "manual")) -> screen = LOADING
       +-- Fast path: InitAll() -> RestoreGameState() -> PLAY
  +-- Klik [Cancel]:
       Kembali ke pause menu
```

- **PERINGATAN**: Progress saat ini digantikan oleh saved state
- Tidak ada auto-save sebelum loading (state saat ini hilang)

### 5. Bermain: Return to Main Menu

Tekan tombol grave -> Pause Menu -> **Return to Main Menu**

```txt
Return to Menu diklik
  +-- Popup konfirmasi: "Return to main menu?" + sub-message
  +-- Klik [Confirm]:
       SaveGameState() + WriteSaveFile(GetSlotPath(g_ActiveSaveSlot, "manual"))
       SetActiveSlot(-1)  // reset slot tracking
       Screen = MAIN_MENU
```

- **Auto-save ke slot yang aktif**
- Jika kemudian klik **Load Game** dari main menu, SaveLoadScreen menampilkan slot yang tersedia
- Tidak ada cara untuk "return to menu tanpa save" saat ini

### 6. Main Menu: Load Game

Main Menu -> **Load Game**

```txt
Load Game diklik
  +-- Buka SaveLoadScreen dalam LOAD_MODE
  +-- Player pilih slot (0-9)
       +-- Klik slot kosong: tidak ada reaksi
       +-- Klik slot terisi:
            Popup konfirmasi [Load Save] [Cancel]
            +-- Klik [Load Save]:
                 SetActiveSlot(slot terpilih)
                 ReadSaveFile(GetSlotPath(slot, "manual"))
                 screen = LOADING
                 +-- Fast path: InitAll() -> RestoreGameState() -> PLAY
            +-- Klik [Cancel]:
                 Kembali ke SaveLoadScreen
```

### 7. Map Switching (Pintu ke Map Baru)

Masuk pintu di game world -> LOADING -> Map Baru -> PLAY

```txt
Masuk pintu di game world
  +-- SwitchMap() menyimpan musuh ke `saves/slot_N/enemies/` dan item ke `saves/slot_N/items/`
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

Sama seperti #9. `ReadSaveFile()` memeriksa `version == SAVE_VERSION` (saat ini 3). Jika tidak cocok (misalnya file v1 atau v2 dari versi lama), mengembalikan false. Version field adalah integer yang bertindak sebagai schema guard -- versi yang tidak cocok langsung ditolak sebelum deserialisasi apapun dimulai. File v2 yang ada akan dimigrasi otomatis ke v3 oleh pipeline migrasi saat startup (lihat [Migration v2->v3 Pipeline](#migration-v2-v3-pipeline)).

### 11. Periodic Autosave 60 Detik

Saat di PLAY state, setiap 60 detik:

```txt
autosaveTimer += frameTime
if (timer >= 60.0f) {
    WriteAutosave("periodic.json")     // -> saves/slot_N/autosave/periodic_DD-MM-YYYY-HH-MM-SS.json
    timer = 0
}
```

- Timer **berhenti** saat pause menu aktif
- Timer **di-reset** saat manual save
- `WriteAutosave()` hanya bekerja jika ada slot aktif (`g_ActiveSaveSlot >= 0`). Jika tidak ada slot aktif, autosave dilewati.
- Setiap autosave menulis file dengan timestamp unik, tidak pernah overwrite.
- Setelah menulis, autosave directory di-prune: hanya 5 file terbaru yang dipertahankan.

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
| Pause **Save Game** | Semua ke `saves/slot_N/manual/manual.json` (N = slot dipilih via SaveLoadScreen) |
| Pause **Return to Menu** | Semua ke `saves/slot_N/manual/manual.json` (slot aktif) |
| **Map switch** | Musuh ke `saves/slot_N/enemies/<path>.json`, item ke `saves/slot_N/items/<path>.json`, + full state ke autosave |
| **Timer 60 detik** | Full state ke `saves/slot_N/autosave/periodic_DD-MM-YYYY-HH-MM-SS.json` |
| **Fresh game start** | Full state ke autosave slot 0 |
| **Main menu Load Game** | SaveLoadScreen: pilih slot, baca `saves/slot_N/manual/manual.json`, restore semua |

## Migration v2->v3 Pipeline

Pipeline migrasi otomatis yang meng-upgrade file save format lama (v2) ke struktur per-slot (v3). Berjalan sekali saat startup game.

### Sentinel File

Migrasi dicegah agar tidak berjalan ulang dengan sentinel file `saves/.migration_completed_v3`. Sentinel adalah file kosong yang ditulis setelah migrasi berhasil. Jika sentinel sudah ada, `NeedsMigration()` return false.

### Fungsi Pipeline

| Fungsi | Deskripsi |
| --- | --- |
| `NeedsMigration()` | Return true jika `saves/manual/slot0.json` ada DAN `saves/.migration_completed_v3` tidak ada |
| `RunMigration()` | Eksekusi pipeline 4 langkah. Return false jika ada langkah yang gagal (atomic abort). |
| `MarkMigrationComplete()` | Tulis sentinel file `saves/.migration_completed_v3` |

### Urutan Migrasi

```txt
Startup Game (main.cpp)
  +-- NeedsMigration()?
       TIDAK -> Lanjut normal
       YA -> RunMigration()
              +-- Task 14: Copy + upgrade saves/manual/slot0.json
              |    -> saves/slot_0/manual/manual.json (v2->v3 upgrade)
              |    Tambah field: slotIndex, saveType, playTime,
              |    mapDisplayName, worldgenSlot
              |    Atomic write via .tmp + rename
              |
              +-- Task 15: Pindahkan saves/enemies/
              |    -> saves/slot_0/enemies/ (rename files)
              |
              +-- Task 16: Pindahkan saves/items/
              |    -> saves/slot_0/items/ (rename files)
              |
              +-- Task 17: Hapus direktori lama + tulis sentinel
                   Hapus: saves/manual/ (lama), saves/enemies/ (lama),
                          saves/items/ (lama)
                   Tulis: saves/.migration_completed_v3
              
              GAGAL -> Log warning, lanjut dengan save normal
                       (migrasi akan dicoba lagi di startup berikutnya)
              BERHASIL -> Lanjut startup normal
```

### Detail Task 14: Copy + Upgrade v2->v3

Langkah ini membaca `saves/manual/slot0.json` (format v2), memverifikasi strukturnya, lalu menambahkan field baru yang diperkenalkan di v3:

```json
{
  "version": 3,
  "slotIndex": 0,
  "saveType": "manual",
  "playTime": 0.0,
  "mapDisplayName": "",
  "worldgenSlot": -1,
  "player": { ... },
  "enemies": [ ... ],
  "items": [ ... ],
  "map": { ... }
}
```

Field `mapDisplayName` akan diisi oleh `GetMapDisplayName()` saat save berikutnya. `worldgenSlot` diisi oleh `SaveGameState()` berdasarkan `g_ActiveSaveSlot` saat itu.

### Keamanan (Atomic)

- Jika Task 14 gagal (disk full, file corrupt), pipeline berhenti dan return false
- Task 15/16 menggunakan rename (bukan copy) sehingga operasi cepat dan atomik di filesystem yang sama
- Sentinel hanya ditulis setelah semua langkah berhasil
- Jika migrasi gagal, save lama tetap utuh dan migrasi akan dicoba lagi di startup berikutnya
- Setelah migrasi berhasil, save lama dihapus

### Catatan

- Migrasi hanya untuk slot 0 (slot tunggal dari sistem sebelumnya)
- Slot 1-4 tetap kosong dan bisa digunakan untuk save baru
- Worldgen data lama di `saves/manual/` tidak dimigrasi -- worldgen menggunakan sistem per-slot sendiri (`worldseed/save_N/`)

## SaveLoadScreen UI

### Class Overview

`SaveLoadScreen` adalah kelas UI untuk layar Save/Load Game yang di-render di atas game screen dengan latar belakang gelap. Mengikuti pola yang sama dengan `OptionsScreen`.

| Aspek | Deskripsi |
| --- | --- |
| **File** | `include/ui/saveLoadScreen.h`, `src/ui/saveLoadScreen.cpp` |
| **Global instance** | `SaveLoadScreen saveLoadScreen` di `main.cpp` |
| **Screen state** | `SAVE_LOAD` (di `ScreenState` enum) |
| **Mode** | `SAVE_MODE` (simpan) atau `LOAD_MODE` (muat) |

### Mode Operasi

`SaveLoadMode::SAVE_MODE`:
- Menampilkan 5 slot manual (0-4) yang bisa di-save
- Slot autosave (5-9) dinonaktifkan (tidak bisa di-save manual)
- Slot kosong: langsung save tanpa konfirmasi
- Slot terisi: popup "Overwrite existing save?" sebelum timpa
- Setelah save: panggil `SetActiveSlot()`, `SaveGameState()`, tutup layar

`SaveLoadMode::LOAD_MODE`:
- Menampilkan semua slot (0-9) yang terisi data
- Slot kosong dinonaktifkan
- Slot terisi: popup "Load this save?" sebelum load
- Setelah load: panggil `SetActiveSlot()`, `ReadSaveFile()`, `RestoreGameState()`, tutup layar

### Layout UI

```
+------------------------------------------+
|           SAVE GAME / LOAD GAME           |
|                                          |
|  MANUAL SAVE                             |
|  +----------+ +----------+ +----------+  |
|  | Slot 0   | | Slot 1   | | Slot 2   |  |
|  | tutorial | | forest   | | (empty)  |  |
|  | 01-01    | | 02-01    | |          |  |
|  +----------+ +----------+ +----------+  |
|  +----------+ +----------+               |
|  | Slot 3   | | Slot 4   |               |
|  | (empty)  | | cave     |               |
|  |          | | 03-01    |               |
|  +----------+ +----------+               |
|                                          |
|  AUTO SAVE                               |
|  +----------+ +----------+ +----------+  |
|  | Slot 5   | | Slot 6   | | Slot 7   |  |
|  +----------+ +----------+ +----------+  |
|  +----------+ +----------+               |
|  | Slot 8   | | Slot 9   |               |
|  +----------+ +----------+               |
|                                          |
|  [BACK]                                   |
+------------------------------------------+
```

### Detail Implementasi

| Method | Fungsi |
| --- | --- |
| `Show()` | Aktifkan layar, muat texture, hitung dimensi, refresh metadata semua slot |
| `Hide()` | Nonaktifkan layar |
| `Update()` | Handle input: klik slot -> popup -> save/load. Handle back button. |
| `Draw()` | Render header, slot grid, back button, popup. |
| `SetReturnScreen(screen)` | Set layar tujuan saat tombol BACK diklik |
| `SetMode(mode)` | Set SAVE_MODE atau LOAD_MODE |
| `CalculateDimensions()` | Hitung posisi dan ukuran elemen UI (850x500 area) |
| `GetSlotAtPosition(pos)` | Deteksi slot mana yang diklik berdasarkan posisi mouse |
| `DrawSlotBox(index, x, y, occupied, mapName, timestamp, mousePos, enabled)` | Gambar satu slot box dengan informasi |
| `DrawSlotGrid(mousePos)` | Gambar seluruh grid slot (manual + autosave) |
| `RefreshSlotMetadata()` | Scan `saves/slot_N/manual/manual.json` untuk tiap slot, baca mapDisplayName dan timestamp |

### Metadata Per-Slot

`RefreshSlotMetadata()` mengiterasi slot 0-9 dan membaca:
1. `saves/slot_N/manual/manual.json` -> jika ada, parse JSON
2. Baca `mapDisplayName` untuk nama map (preview)
3. Baca `timestamp` atau fallback ke `last_write_time` filesystem
4. Tandai slot sebagai occupied atau empty

Informasi ini ditampilkan di setiap slot box untuk membantu player memilih save yang tepat.

### Popup Konfirmasi

Dua jenis popup dengan tombol konfirmasi/batal:

- **Overwrite popup**: "Overwrite existing save?" -- muncul saat SAVE_MODE dan slot sudah terisi
- **Load popup**: "Load this save?" -- muncul saat LOAD_MODE dan slot diklik

Keduanya menggunakan class `Popup` dua-tombol yang sudah ada.

### Wiring

SaveLoadScreen terhubung dari tiga entry point:

| Entry Point | File | Mode |
| --- | --- | --- |
| Pause Menu -> Save Game | `pauseMenu.cpp` case 1 | SAVE_MODE |
| Pause Menu -> Load Game | `pauseMenu.cpp` case 2 | LOAD_MODE |
| Main Menu -> Load Game | `mainMenu.cpp` case 1 | LOAD_MODE |

Semua mengubah `state->currentScreen = SAVE_LOAD`, dan main loop di `main.cpp` menangani rendering/update SAVE_LOAD state dengan memanggil `saveLoadScreen` methods.

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
    SaveGameState() + WriteSaveFile(GetSlotPath(g_ActiveSaveSlot, "manual")) -> manual.json
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
| **File** | `worldseed/save_N/runtime.json`, `meta.json` | `saves/slot_N/manual/manual.json`, `saves/slot_N/autosave/*.json` |
| **Lingkup** | Per-stage (map objects, enemies, drops, barrier) | Player (HP, mana, inventory, position), non-worldgen world state |
| **DeadEntities** | Overwrite per stage via `LoadRuntimeState` | Simpan/restore via `savedMapState.deadEntities` |
| **Pemicu Save** | Stage transition, pause menu | Pause menu, map switch, autosave timer |
| **Pemicu Load** | Map switch ke worldgen stage | Load Game dari main menu atau pause menu |
| **Slot Mapping** | Menggunakan `g_ActiveSaveSlot` atau fallback ke `g_SeedManager.GetCurrentSlot()` | Menggunakan `g_ActiveSaveSlot` yang di-set oleh `SetActiveSlot()` |

Keduanya dipanggil barengan di pause menu (pauseMenu.cpp:359-361), memastikan player state dan worldgen runtime state konsisten saat save.
WorldgenIO menggunakan `g_ActiveSaveSlot` untuk menentukan folder `worldseed/save_N/` yang sesuai dengan slot save aktif. Jika `g_ActiveSaveSlot < 0`, fallback ke `g_SeedManager.GetCurrentSlot()`.

### SetWorldgenPending Flag

`SetWorldgenPending(bool)` adalah static flag di `game_state_saver.cpp` yang melindungi dead entity restoration dari korupsi antar-stage. Flag ini penting karena ada tiga jalur berbeda yang bisa merestore DeadEntities, dan mereka bisa saling bertabrakan:

**Masalah**: Saat save game mid-worldgen, `savedMapState.deadEntities` di manual save slot menangkap snapshot dead entities dari main save system. Jika Load Game dari main menu merestore snapshot ini, lalu player masuk ke worldgen door, `LoadRuntimeState` akan meng-overwrite DeadEntities dengan subset stage saat ini. Tapi antara Load Game dan door entry, snapshot dari slot sudah sempat digunakan -- menyebabkan musuh mati dari stage lain ikut terbawa.

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

### Integrasi manual.json dengan Worldgen

Save Game mid-worldgen menghasilkan `manual.json` dengan `savedMapState.mapPath` yang menunjuk ke `worldseed/save_N/maps/stage_M.json`. Seluruh mapping slot dikelola via `worldgenSlot` field di `SavedPlayerState` dan `g_ActiveSaveSlot`. Saat Load Game dari main menu, loading screen mendeteksi path worldgen dan:

1. Meng-set `SetWorldgenPending(true)` untuk melewati `RestoreDeadEntities()`
2. Di stage 1 loading, masuk ke jalur worldgen (loading_screen.cpp:110)
3. Menjalankan `RunWorldgen()` dengan seed dari `g_SeedManager`
4. Memanggil `WorldgenIO::LoadRuntimeState()` untuk merestore state stage tersebut

Ini berarti **meta.json harus ada** agar worldgen bisa regenerasi map yang benar dari seed. Jika meta.json hilang (misalnya karena manual cleanup), load save mid-worldgen akan gagal di worldgen detection karena `g_SeedManager.LoadMeta()` tidak dijalankan di jalur load biasa.

## File Source Utama

| File | Peran |
| --- | --- |
| `src/core/game_state_saver.cpp` | Logika save/load inti, JSON I/O, WriteAutosave, RestoreDeadEntities, SetActiveSlot, GetSlotPath, EnsureSlotDirectory, migration pipeline |
| `include/core/game_state_saver.h` | Struct state global, deklarasi fungsi, konstanta SAVE_VERSION, g_ActiveSaveSlot |
| `include/core/utils.h` | GenerateUUID() -- utility untuk UUID 32-hex-char |
| `src/items/item.cpp` | Per-map item persistence (SaveItemsForMapDir, LoadItemsForMapDir) |
| `src/ui/mainMenu.cpp` | Main menu save-aware Start Game + Load Game via SaveLoadScreen, ResetWorldseed |
| `src/ui/pauseMenu.cpp` | Pause menu Save/Load/Return to Menu via SaveLoadScreen |
| `src/ui/saveLoadScreen.cpp` | UI layar Save/Load dengan slot grid (5 manual + 5 autosave), popup konfirmasi |
| `include/ui/saveLoadScreen.h` | Deklarasi SaveLoadScreen class, SaveLoadMode enum |
| `src/core/loading_screen.cpp` | Integrasi loading screen, stage map switch, panggil RestoreDeadEntities sebelum InitAll |
| `src/map/map.cpp` | Fungsi map switching (SwitchMap, GoBack, InitMap), GetMapDisplayName |
| `src/entities/enemies/enemy.cpp` | Per-map enemy persistence (SaveEnemiesForMap, LoadEnemiesForMap, SpawnEnemiesFromMap) |
| `src/entities/entities.cpp` | Dead entity registry (RegisterDeath, IsAlreadyDead, PruneDeadEntities) |
| `include/map/propsbehavior.h` | BombManager/CrateManager -- GetConsumedPositions / SetConsumedPositions accessors |
| `src/ui/popup.cpp` | Two-button confirmation popup |
| `src/map/worldgenio.cpp` | Worldgen runtime persistence (SaveRuntimeState, LoadRuntimeState, InitRun, NextStage, PrevStage) |
| `include/map/worldgenio.h` | Deklarasi fungsi WorldgenIO namespace |
| `src/core/seedmanager.cpp` | SeedManager -- seed generation, SaveMeta / LoadMeta, run state management |
| `include/core/seedmanager.h` | Deklarasi SeedManager class, SEED_COUNT constant, g_SeedManager extern |
| `src/systems/interaction.cpp` | Worldgen door handler -- InitRun, SetWorldgenPending(true), NextStage/PrevStage |
| `src/core/main.cpp` | Main loop: autosave timer 60s, SAVE_LOAD screen state, migration startup |
| `src/core/screen_handler.cpp` | Inisialisasi game state, screen management |

## Known Issues / Notices

> Daftar bug yang diketahui dan catatan penting tentang sistem save/load saat ini. Bagian ini **WAJIB** tetap berada di posisi paling akhir dokumen.

| Issue | Status | Detail |
| --- | --- | --- |
| Crash saat load save dari map berbeda | Intermiten | Terjadi secara tidak konsisten saat loading save yang dibuat di map berbeda dari map saat ini. Belum bisa direproduksi secara andal. Kemungkinan terkait dengan enemy/item restore yang merujuk ke entitas yang belum di-spawn. |
| Dead enemies respawn setelah Load Game | Resolved | Disebabkan DeadEntities kosong saat spawn -- diperbaiki dengan `RestoreDeadEntities()` sebelum `InitAll()` dan `PruneDeadEntities()` safety net. Lihat [Restore Ordering Fix](#restore-ordering-fix). |
| UUID matching untuk entitas | Resolved | Semua entitas yang dapat dipersist sekarang memiliki UUID. Multi-pass matching (UUID -> fallback) menangani kompatibilitas save lawan. Lihat [Entity Identity (UUID)](#entity-identity-uuid). |
| Enemy position reset setelah Save/Load | Resolved | UUID matching selalu gagal karena `SpawnEnemiesFromMap()` menghasilkan UUID acak baru setiap spawn. Fallback MapObjectID+Name memiliki `break` sehingga semua enemy dari spawn point yang sama hanya mencocokkan enemy pertama di registry. Diperbaiki dengan `std::unordered_set<Enemy*>` tracker yang memastikan setiap saved enemy mengkonsumsi tepat satu spawned enemy yang berbeda. Lihat [Matching Saat Restore](#matching-saat-restore). |
