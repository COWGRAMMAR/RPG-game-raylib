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

## Setup & Run (Satu Kali)

Jalankan script sesuai OS kamu:

### Windows

```powershell
.\run-windows.ps1
```

Atau double-click `run-windows.bat`.

### Linux/macOS

```bash
bash run-linux.sh
```

Script ini akan otomatis menjalankan setup dependensi, build project, dan langsung meluncurkan game.

> Untuk opsi build lanjutan, lihat [Panduan Build](docs/build.md). Untuk dokumentasi projek lainnya, lihat [Dokumentasi Projek](docs/README.md).
