# Game RPG - Raylib

Game RPG 2D yang dibuat dengan Raylib.

## Fitur

- Main menu dengan button components
- Player movement system
- Tilemap support (Tileson JSON)
- Virtual screen rendering (640x360)

## Persyaratan

- **OS**: Windows 10/11, macOS, atau Linux
- **Compiler**: gcc atau clang
- **CMake**: >= 3.20
- **Ninja**
- **Git**

## Dependencies

- **Raylib 6.0**: Auto-download via `setup.ps1` (Windows) atau `setup.sh` (Linux/macOS)
- **Tileson**: Download manual dari [GitHub Tileson](https://github.com/SSBMTonberry/tileson), copy `tileson.hpp` ke `lib/tileson/`

> **PERHATIAN**: Dukungan untuk sistem Unix (Linux/macOS) bersifat eksperimental dan memerlukan pengujian lebih lanjut. Skrip `setup.sh` telah disediakan namun mungkin mengalami kendala pada beberapa distribusi.

## Setup Pertama Kali

### Windows

```powershell
# Jalankan skrip setup untuk mengunduh dependensi
.\setup.ps1

# Build project
cmake --preset ninja && cmake --build --preset ninja
```

### Linux/macOS

```bash
# Jalankan skrip setup untuk mengunduh dependensi
bash setup.sh

# Build project
cmake --preset ninja && cmake --build --preset ninja
```

Ini akan:

1. Download Raylib 6.0 ke `lib/raylib/` (otomatis sesuai OS)
2. Compile semua file .cpp (Unity build)
3. Link dengan library
4. Copy raylib.dll ke folder output (Windows)

> **PERHATIAN**: Setup untuk Linux/macOS bersifat eksperimental dan memerlukan pengujian lebih lanjut.

### Jalankan Game

```bash
./build/bin/main.exe   # Linux/macOS
.\build\bin\main.exe   # Windows PowerShell
```

### Build

Untuk instruksi build, lihat [Halaman Build](docs/build.md)
