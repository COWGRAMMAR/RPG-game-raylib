# Dokumentasi Projek

## Isi folder ini

Dokumentasi arsitektur, pipeline, dan sistem di projek ini. Setiap file mencakup satu subsistem dengan API reference, flow diagram, dan constraint. Untuk pengembang manusia atau AI agent yang ingin memahami kode sebelum mengerjakannya.

## Daftar Isi

Folder ini bertambah seiring projek berkembang. Kalau menambah atau mengurangi, update `./README.md` ini juga.

1. [`./assets-structure.md`](./assets-structure.md) : Panduan meletakkan aset dan resource.
2. [`./build.md`](./build.md) : Cara build dan run projek.
3. [`./save-system.md`](./save-system.md) : Arsitektur save/load: slot_-1 workspace, GameSnapshot, per-slot isolation, UUID entity identity, pipeline flow (save/switch/load/new/restart), coverage matrix, bug history, changelog Waves 1-8.
4. [`./sign-system-ui.md`](./sign-system-ui.md) : Sign/UI system (belum diisi).
5. [`./source-structure.md`](./source-structure.md) : Struktur direktori source code dan header.
6. [`./worldgeneration.md`](./worldgeneration.md) : World generation system.
7. [`./debug-keybinds.md`](./debug-keybinds.md) : Keybind debug dan akses pengembang.
8. [`./buff-indicator-system.md`](./buff-indicator-system.md) : Indikator buff aktif (Damage/Speed/Invul): pipeline, integrasi, design decisions untuk turn-based combat.
9. [`./font-system.md`](./font-system.md) : Font system: pipeline atlas resolution cache, API, panduan pemakaian, daftar font, best practices.
