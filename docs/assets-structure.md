# Struktur Folder Assets

Dokumentasi ini menjelaskan struktur folder `assets/` yang menyimpan semua aset non-kode proyek Breach & Loot.

## Pohon Folder Assets

```txt
assets/
├── audio/                        # Aset audio game
│   ├── music/                    #   Musik latar (MP3)
│   └── sfx/                      #   Efek suara (MP3)
├── data/                         # Data statis game (JSON: animasi, musuh, item, sprite, tile)
├── fonts/                        # Aset font (TTF / OTF)
├── images/                       # Gambar untuk dokumentasi dan README
│   ├── gameplayScreenshots/      #   Folder screenshot — kosong (file dihapus di PR #82 akbarazy)
│   └── logo/                     #   (kosong — untuk logo alternatif)
├── maps/                         # File peta Tileson (JSON)
│   └── World_generation/         #   Template room, koridor, dan world seed
├── textures/                     # Sprite sheet dan UI (PNG)
└── video/                        # Video untuk background menu dan cutscene
    └── intro/                    #   Cutscene pembuka (MKV)
```

## Deskripsi per Kategori

### Audio (`assets/audio/`)

| Subfolder | Isi |
| --- | --- |
| `music/` | Musik latar game (MP3): Dungeon, GameOver, MainMenu, Boss, WinTheme, dll. |
| `sfx/` | Efek suara pendek (MP3): arrow, slash, walk, explosion, hurt, chest, crate, dll. |

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

| File | Format | Typeface | Kegunaan |
| --- | --- | --- | --- |
| `NewDawn.ttf` | TTF | New Dawn | Font dekoratif gothic untuk header dan judul |
| `Poppins-Regular.ttf` | TTF | Poppins | Font sans-serif utama untuk teks UI |
| `Poppins-Bold.ttf` | TTF | Poppins | Varian bold untuk teks tebal |
| `Quicksand-Bold.ttf` | TTF | Quicksand | Teks UI alternatif — varian bold |
| `Quicksand-Light.ttf` | TTF | Quicksand | Teks UI alternatif — varian light |
| `Quicksand-Medium.ttf` | TTF | Quicksand | Teks UI alternatif — varian medium |
| `Quicksand-Regular.ttf` | TTF | Quicksand | Teks UI alternatif — varian regular |
| `Quicksand-SemiBold.ttf` | TTF | Quicksand | Teks UI alternatif — varian semi-bold |
| `Norse.otf` | OTF | Norse | Font dekoratif untuk elemen tematik Norse |
| `Norsebold.otf` | OTF | Norsebold | Varian bold dari Norse |
| `MedievalSharp-Regular.ttf` | TTF | MedievalSharp | Font bergaya abad pertengahan |

Font dimuat secara **lazy** via `GetOrLoad(FontId)` — dipanggil pertama kali saat render, bukan di startup. Font di-cache dalam `std::unordered_map` dengan key `(FontId << 16) | AtlasRes`. Lihat `docs/font-system.md` untuk detail pipeline dan API.

### Gambar (`assets/images/`)

Gambar untuk keperluan dokumentasi dan README proyek.

| Subfolder | Isi |
| --- | --- |
| `gameplayScreenshots/` | Folder screenshot — kosong (6 file PNG dihapus di PR #82 akbarazy). README masih referensi screenshot ini — perlu update terpisah |
| `logo/` | Folder kosong — disediakan untuk logo alternatif di masa depan |

### Peta (`assets/maps/`)

Peta format JSON Tileson (kompatibel Tiled Map Editor).

| File / Folder | Keterangan |
| --- | --- |
| `floorA.json` | Lantai A — peta utama |
| `floorB.json` | Lantai B — peta utama |
| `floorC.json` | Lantai C — peta utama |
| `main_hub.json` | Area hub utama pemain |
| `tutorial.json` | Peta tutorial |
| `tutorial copy 2.json` | Duplikat/leftover dari tutorial.json (tidak digunakan) |
| `World_generation/` | Template dan data untuk world generation procedural |

#### World_generation/

Struktur folder `World_generation/`:

```txt
maps/World_generation/
├── background_map.json                 # Background map besar untuk world generation
├── template_untuk_copy_object.json     # Template untuk copy object
├── rooms/                              # Template room — tiap kategori punya varian koneksi
│   ├── start/                          #   Room spawn pemain (start1_u.json)
│   ├── enemy/                          #   Room musuh biasa (6 varian × 5 tipe koneksi = 30 file)
│   ├── elite/                          #   Room elite (3 varian × 3 tipe koneksi = 9 file)
│   ├── boss/                           #   Room boss (3 varian)
│   ├── treasure/                       #   Room harta karun (2 varian × 2 tipe koneksi = 4 file)
│   ├── trader/                         #   Room trader
│   └── finish/                         #   Room finish
├── corridor/                           # Koridor penghubung antar room
│   ├── corridor_h/                     #   Koridor horizontal (16 varian)
│   └── corridor_v/                     #   Koridor vertikal (16 varian)
└── worldseed/                          # Seed world generation tersimpan per save slot
    ├── save_1/                         #   Seed save slot 1 (meta.json + 5 stage map)
    └── save_2/                         #   Seed save slot 2 (meta.json + 5 stage map)
```

Tiap room template punya varian koneksi (u = up, d = down, l = left, r = right) yang menentukan arah konektor koridor.

### Tekstur (`assets/textures/`)

Sprite sheet PNG dan aset UI di root folder:

`autotiles.png`, `bg-dungeon.png`, `bg-forest.png`, `boss-enemies.png`, `dialogBox.png`, `effects.png`, `elite-enemies.png`, `enemies.png`, `grass-terrain.png`, `items.png`, `knight (1).png`, `logo.png`, `props.png`, `test.png`, `tiles.png`

Subdirektori:

| Subfolder | Jumlah File | Konten |
| --- | --- | --- |
| `minimap/` | 1 | Aset minimap: `mapBG.png` — background map untuk layer minimap |
| `settingsButt/` | 13 | Tombol dan elemen UI screen settings (audio, video, keybinds, scrollbar, knob, back) |
| `saveloadAsset/` | 9 | Aset UI screen save/load (box kosong, box terisi, tombol delete, title, background) |
| `pauseButt/` | 11 | Tombol pause menu (resume, save, load, settings, restart, exit, background, notifikasi) |
| `mainMenuButt/` | 4 | Tombol main menu (start, load, settings, quit) |
| `inventory/` | 3 | UI inventory (background, full-grid, complete indicator) |
| `gameOver/` | 4 | UI game over (gameover.png, revive, settings, to-main menu) |
| `hudPlayer/` | 3 | HUD player (ikon tas, kill count, ikon settings) |

Root textures:

`autotiles.png`, `bg-dungeon.png`, `bg-forest.png`, `boss-enemies.png`, `dialogBox.png`, `effects.png`, `elite-enemies.png`, `enemies.png`, `grass-terrain.png`, `items.png`, `knight (1).png`, `logo.png`, `props.png`, `test.png`, `tiles.png`

| Subfolder | Jumlah File | Konten |
| --- | --- | --- |
| `settingsButt/` | 13 | Tombol dan elemen UI screen settings (audio, video, keybinds, scrollbar, knob, back) |
| `saveloadAsset/` | 9 | Aset UI screen save/load (box kosong, box terisi, tombol delete, title, background) |
| `pauseButt/` | 11 | Tombol pause menu (resume, save, load, settings, restart, exit, background, notifikasi) |
| `mainMenuButt/` | 4 | Tombol main menu (start, load, settings, quit) |
| `inventory/` | 3 | UI inventory (background, full-grid, complete indicator) |
| `gameOver/` | 4 | UI game over (gameover.png, revive, settings, to-main menu) |
| `hudPlayer/` | 3 | HUD player (ikon tas, kill count, ikon settings) |

### Video (`assets/video/`)

Video untuk background menu dan cutscene.

| File / Folder | Keterangan |
| --- | --- |
| `bg-main-menu.mp4` | Video background untuk main menu (MP4) |
| `intro/` | Folder cutscene pembuka |
| `intro/IntroIntroductions.mkv` | Video cutscene intro (MKV) |

## Format File Aset

| Jenis Aset | Format | Keterangan |
| --- | --- | --- |
| Audio | MP3 | Efek suara dan musik latar, dikelola di folder `audio/sfx/` dan `audio/music/` |
| Tekstur | PNG | Gambar tanpa kompresi lossy, mendukung transparansi |
| Peta | JSON | Format Tileson (kompatibel dengan Tiled Map Editor) |
| Data Game | JSON | Definisi item, musuh, sprite, animasi, tile properties |
| Font | TTF / OTF | TrueType Font dan OpenType Font untuk rendering teks kustom |
| Gambar | PNG | Screenshot dan aset visual untuk dokumentasi |
| Video | MP4 / MKV | Video background menu (MP4) dan cutscene (MKV) |

## Direktori Runtime (tidak di-commit)

Folder berikut dibuat saat runtime dan tidak dilacak oleh git:

| Folder | Kegunaan |
| --- | --- |
| `saves/` | File save game, autosave, dan pengaturan per-user |

### saves/settings/

Pengaturan per-user disimpan sebagai 3 file JSON terpisah, dibuat saat runtime:

| File | Konten |
| --- | --- |
| `saves/settings/audioTab.json` | Pengaturan audio: volume musik dan SFX |
| `saves/settings/videoTab.json` | Pengaturan video: resolusi, fullscreen, FPS display |
| `saves/settings/keybindsTab.json` | Keybindings: semua mapping tombol kustom pemain |

File-file ini tidak dilacak oleh git (masuk `.gitignore`), sehingga setiap pemain memiliki konfigurasi masing-masing tanpa mengganggu repo.

## Cara Navigasi Aset

1. **Audio**: Tambahkan efek suara baru ke `assets/audio/sfx/` atau musik ke `assets/audio/music/`, format MP3.
2. **Tekstur**: Tambahkan sprite/tiles baru ke `assets/textures/`, gunakan format PNG.
3. **Peta**: Buat peta baru menggunakan Tiled, ekspor ke format JSON Tileson, simpan di `assets/maps/`.
4. **Data**: Edit `assets/data/*.json` untuk menambah/mengubah definisi item, musuh, sprite, atau tile.
5. **Font**: Tambahkan file TTF/OTF ke `assets/fonts/`, lalu daftarkan `FontId` baru di `include/rendering/fonts.h` dan definisikan `FontDef`-nya. Font akan di-load secara lazy via `GetOrLoad(FontId)` saat pertama kali dipakai.
6. **Konfigurasi pemain**: File di `saves/settings/*Tab.json` dibuat otomatis saat pertama kali game dijalankan. Jangan diedit manual — gunakan menu Options di dalam game.
