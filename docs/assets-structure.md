# Struktur Folder Assets

Dokumentasi ini menjelaskan struktur folder `assets/` yang menyimpan semua aset non-kode proyek RPG Game Raylib.

## Pohon Folder Assets

```txt
assets/
├── audio/                # Aset audio game
│   ├── music/            #   Musik latar (MP3)
│   └── sfx/              #   Efek suara (MP3)
├── data/                 # Data statis game (JSON: animasi, musuh, item, sprite, tile)
├── fonts/                # Aset font TTF (NewDawn, Poppins)
├── maps/                 # File peta Tileson (JSON)
└── textures/             # Sprite sheet dan UI (PNG)
```

## Deskripsi per Kategori

### Audio (`assets/audio/`)

| Subfolder | Isi |
| --- | --- |
| `music/` | Musik latar game (MP3): Dungeon, GameOver, MainMenu, dll. |
| `sfx/` | Efek suara pendek (MP3): slash, step, dll. |

### Data (`assets/data/`)

Definisi game berbasis JSON, dimuat saat startup oleh `animation.cpp` dan `datadriven.cpp`:

| File | Kegunaan |
| --- | --- |
| `animations.json` | Frame, durasi, dan loop tiap animasi sprite |
| `enemies.json` | HP, damage, ukuran, dan properti setiap jenis musuh |
| `items.json` | Kategori, rarity, efek, dan statistik item |
| `sprites.json` | Area (x, y, w, h) tiap sprite di texture sheet |
| `tiles.json` | Collision, type, dan efek tiap tile peta |

### Font (`assets/fonts/`)

| File | Kegunaan |
| --- | --- |
| `NewDawn.ttf` | Font dekoratif gothic untuk header dan judul |
| `Poppins-Regular.ttf` | Font sans-serif utama untuk teks UI dan entry keybinds |
| `Poppins-Bold.ttf` | Varian bold untuk teks tebal |

Font dimuat sekali di startup (`InitFonts()` di `main.cpp`) via variabel global `fontKeybindHeader` / `fontKeybindEntry`. Lihat `include/rendering/fonts.h`.

### Peta (`assets/maps/`)

Peta format JSON Tileson (kompatibel Tiled Map Editor): `floorA.json`, `floorB.json`, `floorC.json`, `tutorial.json`.

### Tekstur (`assets/textures/`)

Sprite sheet PNG dan aset UI: tile, karakter, musuh, item, efek, props, logo.

## Format File Aset

| Jenis Aset | Format | Keterangan |
| --- | --- | --- |
| Audio | MP3 | Efek suara dan musik latar, dikelola di folder `audio/sfx/` dan `audio/music/` |
| Tekstur | PNG | Gambar tanpa kompresi lossy, mendukung transparansi |
| Peta | JSON | Format Tileson (kompatibel dengan Tiled Map Editor) |
| Data Game | JSON | Definisi item, musuh, sprite, animasi, tile properties |
| Font | TTF | TrueType Font untuk rendering teks kustom |

## Direktori Runtime (tidak di-commit)

Folder berikut dibuat saat runtime dan tidak dilacak oleh git:

| Folder | Kegunaan |
| --- | --- |
| `saves/` | File save game, autosave, dan pengaturan per-user |

### saves/settings.json

File `saves/settings.json` menyimpan pengaturan per-user yang dibuat saat runtime:

- **Keybindings**: Semua mapping tombol kustom pemain
- **Video settings**: Resolusi, fullscreen, FPS display
- **Audio settings**: Volume musik dan SFX

File ini tidak dilacak oleh git (masuk `.gitignore`), sehingga setiap pemain memiliki konfigurasi masing-masing tanpa mengganggu repo.

## Cara Navigasi Aset

1. **Audio**: Tambahkan efek suara baru ke `assets/audio/sfx/` atau musik ke `assets/audio/music/`, format MP3.
2. **Tekstur**: Tambahkan sprite/tiles baru ke `assets/textures/`, gunakan format PNG.
3. **Peta**: Buat peta baru menggunakan Tiled, ekspor ke format JSON Tileson, simpan di `assets/maps/`.
4. **Data**: Edit `assets/data/*.json` untuk menambah/mengubah definisi item, musuh, sprite, atau tile.
5. **Font**: Tambahkan file TTF ke `assets/fonts/`, lalu deklarasikan di `include/rendering/fonts.h` dan muat di `src/rendering/animation.cpp`.
6. **Konfigurasi pemain**: File `saves/settings.json` dibuat otomatis saat pertama kali game dijalankan, jangan diedit manual — gunakan menu Options di dalam game.
