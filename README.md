<h1 align="center">BREACH &amp; LOOT</h1>

<!-- Game logo -->
![Game Logo](assets/textures/logo.png)

<!-- Screenshot gameplay 2x2 grid -->
<div align="center">
  <table>
    <tr>
      <td align="center" width="50%">
        <img src="assets/images/gameplayScreenshots/screenshot_mainMenu.png" alt="Main Menu" width="100%">
      </td>
      <td align="center" width="50%">
        <img src="assets/images/gameplayScreenshots/screenshot_gameplay1.png" alt="Gameplay" width="100%">
      </td>
    </tr>
    <tr>
      <td align="center" width="50%">
        <img src="assets/images/gameplayScreenshots/screenshot_turnbased.png" alt="Turn-Based Combat" width="100%">
      </td>
      <td align="center" width="50%">
        <img src="assets/images/gameplayScreenshots/screenshot_bossgameplay.png" alt="Boss Combat" width="100%">
      </td>
    </tr>
    <tr>
      <td align="center" width="50%">
        <img src="assets/images/gameplayScreenshots/screenshot_explosion.png" alt="Explosion Effects" width="100%">
      </td>
      <td align="center" width="50%">
        <img src="assets/images/gameplayScreenshots/screenshot_attackbow.png" alt="Bow Attack" width="100%">
      </td>
    </tr>
  </table>
</div>

Game RPG 2D bertema fantasi yang dibuat dengan Raylib (C++).
<!-- TODO: tambah deskripsi lebih detail -- genre, setting, gameplay loop -->

## Fitur

- Movement & interaksi tilemap (Tileson JSON)
- Combat turn-based dengan entity arrow & efek buff
- Inventory management dengan BST sorting
- Save/load system multi-slot (GameSnapshot + SaveManager)
- UI settings: video, audio, keybind customization
- World generation procedural
- Font system dengan lazy-load atlas caching
- Debug mode dengan overlay informasi

## Persyaratan

- **OS**: Windows 10/11, macOS, atau Linux
- **Compiler**: gcc atau clang
- **CMake**: >= 3.20
- **Ninja**
- **Git**

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

1. Download Raylib 6.0 ke direktori platform-specific (`lib/raylib-windows/` di Windows, `lib/raylib-linux/` di Linux, `lib/raylib-macos/` di macOS)
2. Compile semua file .cpp (Unity build)
3. Link dengan library
4. Copy raylib.dll ke folder output (Windows)

> **PERHATIAN**: Setup untuk Linux/macOS bersifat eksperimental dan memerlukan pengujian lebih lanjut.

### Jalankan Game

```bash
# Linux/macOS
./build/bin/main

# Windows (Powershell)
.\build\bin\main.exe   # Windows PowerShell
```

### Build

Untuk instruksi build dan dokumentasi lainnya, lihat [Dokumentasi Projek](docs/README.md)
