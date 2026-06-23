# Struktur Kode Sumber

Dokumentasi ini menjelaskan struktur folder `src/` (file `.cpp`) dan `include/` (file `.h`) proyek Breach & Loot. Proyek menggunakan sistem *unity build* di mana semua file `.cpp` digabung menjadi satu unit kompilasi oleh CMake.

## Pohon Struktur Kode Sumber

### Folder `src/` (File Sumber)

```txt
src/
├── core/                       # Logika inti game
│   ├── game_state_saver.cpp    # Penyimpanan state game (save/load)
│   ├── loading_screen.cpp      # Layar loading saat memuat aset/peta
│   ├── main.cpp                # Entry point aplikasi
│   ├── savemanager.cpp         # Manajer penyimpanan multi-slot
│   ├── screen_handler.cpp      # Manajemen render texture dan resolusi
│   └── seedmanager.cpp         # Manajemen seed untuk procedural generation
├── debug/                      # Fitur mode debug
│   └── debugmode.cpp           # Logika aktivasi dan fitur debug
├── entities/                   # Entitas game
│   ├── entities.cpp            # Logika dasar semua entitas
│   ├── player.cpp              # Logika pemain
│   └── enemies/                # Subdirektori entitas musuh
│       ├── enemy.cpp           # Logika dasar musuh
│       └── enemy_ai.cpp        # Kecerdasan buatan musuh
├── items/                      # Sistem item dan inventory
│   ├── inv-bst-sort.cpp        # Sorting inventory dengan BST
│   ├── inventory.cpp           # Manajemen inventory pemain
│   └── item.cpp                # Logika item dasar
├── map/                        # Logika pemetaan dan world generation
│   ├── map.cpp                 # Manajemen peta utama
│   ├── mapLogic.cpp            # Logika interaksi peta
│   ├── mapstack.cpp            # Stack navigasi antar peta
│   ├── minimap.cpp             # Sistem minimap (fog of war, tile coloring, viewport)
│   ├── propsbehavior.cpp       # Perilaku props di dunia game
│   ├── worldgenenartion.cpp    # Generasi dunia procedural
│   ├── worldgenio.cpp          # Input/output world generation
│   └── worldgenlogic.cpp       # Logika inti world generation
├── media/                      # Media tambahan
│   └── videoPlayer.cpp         # Pemutar video (format .ikn)
├── rendering/                  # Logika rendering grafis
│   ├── animation.cpp           # Sistem animasi sprite
│   ├── fonts.cpp               # Sistem font dan atlas caching
│   └── hud.cpp                 # Rendering Heads-Up Display (HUD)
├── systems/                    # Sistem game (logika lintas fitur)
│   ├── audioManager.cpp        # Manajemen audio dan suara
│   ├── combat.cpp              # Logika sistem combat
│   ├── combatTurn.cpp          # Manajemen giliran dalam combat
│   ├── datadriven.cpp          # Sistem data-driven (JSON)
│   ├── effects.cpp             # Efek visual dan audio
│   ├── input.cpp               # Manajemen input pemain
│   ├── inputLinkedList.cpp     # Struktur data linked list untuk input
│   ├── interaction.cpp         # Logika interaksi pemain dengan objek
│   ├── keybindManager.cpp      # Manajemen tombol kustom
│   └── movement.cpp            # Logika pergerakan entitas
└── ui/                         # Antarmuka pengguna (UI)
    ├── audioTab.cpp            # Tab pengaturan audio di menu
    ├── gameOverScreen.cpp      # Layar game over
    ├── keybindsTab.cpp         # Tab pengaturan tombol di menu
    ├── mainMenu.cpp            # Menu utama game
    ├── pauseMenu.cpp           # Menu pause game
    ├── popup.cpp               # Komponen popup pesan
    ├── saveLoadScreen.cpp      # Layar save/load game
    ├── videoScreen.cpp         # Layar pengaturan video
    └── videoTab.cpp            # Tab pengaturan video di menu
```

### Folder `include/` (File Header)

```txt
include/
├── config/                     # Konfigurasi konstanta game
│   └── game_constants.h        # Konstanta global game
├── core/                       # Header inti
│   ├── game_state_saver.h
│   ├── loading_screen.h
│   ├── savemanager.h
│   ├── screen.h
│   ├── seedmanager.h
│   └── utils.h
├── data/                       # Data tambahan (kosong)
├── debug/                      # Header debug
│   └── game_debug.h
├── entities/                   # Header entitas
│   ├── enemy.h
│   ├── enemy_ai.h
│   ├── entities.h
│   ├── entity.h
│   └── player.h
├── items/                      # Header item
│   ├── inv-bst-sort.h
│   ├── inventory.h
│   └── item.h
├── map/                        # Header peta
│   ├── map.h
│   ├── mapLogic.h
│   ├── minimap.h
│   ├── propsbehavior.h
│   ├── worldgenenartion.h
│   └── worldgenio.h
├── media/                      # Header media
│   └── videoPlayer.h
├── rendering/                  # Header rendering
│   ├── animation.h
│   ├── fonts.h
│   └── hud.h
├── systems/                    # Header sistem
│   ├── audioManager.h
│   ├── combat.h
│   ├── combatTurn.h
│   ├── datadriven.h
│   ├── effects.h
│   ├── input.h
│   ├── inputLinkedList.h
│   ├── interaction.h
│   ├── keybindManager.h
│   └── movement.h
├── ui/                         # Header UI
│   ├── audioTab.h
│   ├── button.h
│   ├── buttonImg.h
│   ├── buttonTxt.h
│   ├── gameOverScreen.h
│   ├── keybindsTab.h
│   ├── mainMenu.h
│   ├── pauseMenu.h
│   ├── popup.h
│   ├── saveLoadScreen.h
│   ├── videoScreen.h
│   └── videoTab.h
└── utils/                      # Header utilitas
    ├── effectQueue.h
    └── mapstack.h
```

## Daftar File per Kategori

### Core (Inti Game)

| File | Tujuan |
| --- | --- |
| `main.cpp` | Entry point aplikasi, inisialisasi Raylib dan game loop |
| `screen_handler.cpp` | Mengatur render texture dinamis, resize, fullscreen, dan resolusi |
| `loading_screen.cpp` | Menampilkan layar loading saat memuat aset atau peta |
| `game_state_saver.cpp` | Menyimpan dan memuat state game (save/load) |
| `savemanager.cpp` | Manajer penyimpanan multi-slot dengan isolasi per slot |
| `seedmanager.cpp` | Mengelola seed untuk procedural world generation |

### Entities (Entitas)

| File | Tujuan |
| --- | --- |
| `player.cpp` | Logika pergerakan, animasi, dan interaksi pemain |
| `entities.cpp` | Logika dasar semua entitas (posisi, kolisi, rendering) |
| `enemies/enemy.cpp` | Logika dasar musuh (health, damage, tipe) |
| `enemies/enemy_ai.cpp` | Kecerdasan buatan untuk perilaku musuh |

### Items (Item & Inventory)

| File | Tujuan |
| --- | --- |
| `item.cpp` | Logika dasar item (nama, tipe, efek) |
| `inventory.cpp` | Manajemen inventory pemain (tambah, hapus, gunakan item) |
| `inv-bst-sort.cpp` | Sorting inventory menggunakan Binary Search Tree |

### Map (Peta & World Generation)

| File | Tujuan |
| --- | --- |
| `map.cpp` | Memuat dan mengelola peta dari file JSON Tileson |
| `mapLogic.cpp` | Logika interaksi peta (transisi, event) |
| `mapstack.cpp` | Stack navigasi untuk berpindah antar peta |
| `minimap.cpp` | Sistem minimap: fog of war, tile coloring, viewport camera, multi-layer grid |
| `propsbehavior.cpp` | Perilaku props dan objek interaktif di peta |
| `worldgenenartion.cpp` | Algoritma generasi peta procedural |
| `worldgenio.cpp` | Input/output data world generation ke file |
| `worldgenlogic.cpp` | Logika inti world generation (rule-based) |

### Media (Media)

| File | Tujuan |
| --- | --- |
| `videoPlayer.cpp` | Pemutar video format .ikn (cutscene) |

### Systems (Sistem Game)

| File | Tujuan |
| --- | --- |
| `input.cpp` | Memproses input keyboard dan mouse pemain |
| `movement.cpp` | Logika pergerakan semua entitas |
| `combat.cpp` | Logika serangan, damage, dan HP |
| `combatTurn.cpp` | Manajemen sistem giliran dalam combat |
| `interaction.cpp` | Logika interaksi pemain dengan objek/NPC |
| `effects.cpp` | Efek visual (partikel, transisi) dan audio |
| `inputLinkedList.cpp` | Struktur data linked list untuk antrean input |
| `audioManager.cpp` | Manajemen pemutaran suara dan musik |
| `keybindManager.cpp` | Konfigurasi tombol kontrol kustom |
| `datadriven.cpp` | Sistem data-driven untuk membaca konfigurasi JSON |

### Rendering (Rendering)

| File | Tujuan |
| --- | --- |
| `animation.cpp` | Sistem animasi sprite berbasis frame |
| `fonts.cpp` | Sistem font dengan lazy-load atlas caching |
| `hud.cpp` | Rendering HUD (HP bar, inventory, minimap) |

### UI (Antarmuka Pengguna)

| File | Tujuan |
| --- | --- |
| `mainMenu.cpp` | Menu utama (mulai game, pengaturan, keluar) |
| `pauseMenu.cpp` | Menu pause (lanjutkan, pengaturan, keluar) |
| `popup.cpp` | Komponen popup untuk pesan singkat |
| `audioTab.cpp` | Pengaturan volume audio di menu |
| `videoTab.cpp` | Pengaturan resolusi/fullscreen di menu |
| `videoScreen.cpp` | Layar penuh pengaturan video |
| `keybindsTab.cpp` | Pengaturan tombol kontrol di menu |
| `gameOverScreen.cpp` | Layar game over (mati, restart) |
| `saveLoadScreen.cpp` | Layar untuk menyimpan dan memuat game |

### Debug (Mode Debug)

| File | Tujuan |
| --- | --- |
| `debugmode.cpp` | Fitur debug (tampilkan FPS, kolisi, teleportasi) |

### Utils (Utilitas)

| File | Tujuan |
| --- | --- |
| `effectQueue.h` | Header antrian efek (digunakan oleh sistem effects) |
| `mapstack.h` | Header stack navigasi peta |

## Ketergantungan Antar Modul

Berikut adalah ketergantungan utama antar modul (modul di kiri bergantung pada modul di kanan):

```txt
entities --> core, map, systems
items --> entities, core
map --> core, lib/tileson, systems
systems --> entities, core, ui, map
rendering --> entities, map, core
ui --> core, systems, rendering, map
media --> core, rendering
debug --> core, entities, map, systems
utils --> (digunakan oleh systems, map)
```

Penjelasan singkat:

- Semua modul bergantung pada `core` sebagai fondasi dasar.
- Modul `entities` dan `map` sering digunakan oleh modul lain untuk logika dunia game.
- Modul `systems` mengorkestrasi logika lintas modul (input, movement, combat, audio).
- Modul `map` mencakup world generation, props behavior, dan stack navigasi peta.
- Modul `media` berdiri sendiri sebagai pemutar video cutscene.

## Cara Navigasi Kode

1. **Entry Point**: Mulai dari `src/core/main.cpp` untuk memahami alur game utama.
2. **Logika Pemain**: Lihat `src/entities/player.cpp` dan `include/entities/player.h`.
3. **UI**: Semua komponen UI ada di `src/ui/` dan header terkait di `include/ui/`.
4. **Peta & World Gen**: Logika peta ada di `src/map/` dengan world generation di `worldgen*.cpp`.
5. **Combat**: Sistem pertarungan di `src/systems/combat.cpp` dan `combatTurn.cpp`.
6. **Save/Load**: Pipeline penyimpanan di `src/core/game_state_saver.cpp` dan `savemanager.cpp`.
7. **Menambahkan Fitur Baru**: Buat file `.cpp` di subfolder `src/` yang sesuai, buat header `.h` di `include/` yang sesuai, lalu jalankan `cmake --preset ninja` untuk rekonfigurasi build.

## Konfigurasi Unity Build

- Proyek menggunakan *unity build*: semua file `.cpp` di `src/` digabung menjadi satu unit kompilasi oleh CMake.
- File `.cpp` baru akan otomatis terdeteksi oleh CMake, namun memerlukan rekonfigurasi preset setelah penambahan file.
- Jangan mengubah konfigurasi unity build di `CMakeLists.txt` kecuali diperlukan.
- Saat menambah file baru, pastikan untuk menambahkan `#include` yang sesuai di `unitybuild.cpp` atau file gateway lain jika diperlukan.
