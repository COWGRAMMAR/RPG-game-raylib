# Struktur Proyek RPG Game Raylib

Dokumentasi ini menjelaskan struktur folder lengkap proyek RPG Game yang dibangun dengan Raylib dan C++ menggunakan sistem *unity build*.

## Pohon Struktur Proyek

```txt
rpg-game-raylib/
├── assets/          # Aset game (audio, tekstur, peta)
├── build/           # Hasil kompilasi (otomatis dibuat oleh CMake)
├── docs/            # Dokumentasi proyek
├── include/         # File header (.h) terstruktur per modul
├── lib/             # Dependensi pihak ketiga (Raylib, Tileson)
├── src/             # File sumber (.cpp) terstruktur per modul
├── CMakeLists.txt   # Konfigurasi utama CMake
├── CMakePresets.json # Preset build CMake (Release/Debug)
├── setup.ps1        # Skrip setup dependensi (Windows)
├── setup.sh         # Skrip setup dependensi (Linux/macOS)
├── README.md        # Dokumentasi pengantar proyek
├── AGENTS.md        # Instruksi untuk agen AI pengembang
├── TODO.md          # Daftar tugas pengembangan
└── .gitignore       # Konfigurasi abaikan file untuk Git
```

## Deskripsi Folder Utama

| Folder | Deskripsi |
| --- | --- |
| `assets/` | Menyimpan semua aset game non-kode: audio (efek suara), tekstur (sprites, tiles, UI), dan peta (format JSON Tileson). |
| `build/` | Direktori hasil kompilasi otomatis. Berisi executable (`build/bin/main.exe`) dan file sementara CMake/Ninja. **Jangan edit file di sini secara manual**. |
| `docs/` | Dokumentasi tambahan proyek (panduan build, struktur proyek, dll). |
| `include/` | File header (.h) yang diorganisir per modul (core, entities, items, dll). |
| `lib/` | Dependensi pihak ketiga yang diunduh otomatis: Raylib 6.0 dan Tileson. |
| `src/` | File sumber (.cpp) yang diorganisir per modul, menggunakan sistem *unity build*. |

## Cara Navigasi Proyek

1. **Kode Sumber**: Semua logika game ada di `src/` dan header terkait di `include/`, diorganisir per fitur (lihat `docs/source-structure.md` untuk detail).
2. **Aset Game**: Semua file non-kode ada di `assets/`, dikelompokkan berdasarkan jenis (lihat `docs/assets-structure.md` untuk detail).
3. **Build & Dependensi**: Konfigurasi build ada di `CMakeLists.txt`, dependensi otomatis diunduh ke `lib/` saat menjalankan skrip `setup.ps1`/`setup.sh`.

## Ketergantungan Build

- Proyek menggunakan **Unity Build**: Semua file `.cpp` di `src/` digabung menjadi satu unit kompilasi.
- CMake akan otomatis mendeteksi file `.cpp` baru, namun memerlukan rekonfigurasi (`cmake --preset ninja`) setelah menambahkan file sumber baru.
- Dependensi (Raylib, Tileson) diunduh otomatis ke `lib/` saat setup pertama kali.
