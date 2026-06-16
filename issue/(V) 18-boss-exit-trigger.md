# Issue #13 — Boss Exit Trigger Terlalu Besar — FIXED

## Tipe
Revisi (Design) — **SELESAI**

## Deskripsi
Object layer trigger untuk balik ke main menu setelah lawan boss ukurannya 928×928 (hampir seluruh prefab 992×992). Player bisa interact di mana aja di room boss.

## Root Cause
Di semua 3 boss prefab (`boss1_u.json`, `boss2_u.json`, `boss3_u.json`), object layer "object" berisi trigger "boss" dengan ukuran 928×928 — mecakup hampir seluruh prefab.

## Fix
- Trigger "boss" (type="pass") dikecilin dari 928×928 → **64×64**, dipindah ke pintu utara (x=448, y=32)
- Object BARU "boss_music" (type="boss_music") ditambah: 928×928, x=32, y=32 — sebagai area trigger music + HP bar
- `nextobjectid` di-increment tiap file

### File yang Diubah
| File | Perubahan |
|------|----------|
| `assets/maps/World_generation/rooms/boss/boss1_u.json` | Exit trigger 64×64 (x=448,y=32), +boss_music object |
| `assets/maps/World_generation/rooms/boss/boss2_u.json` | Sama |
| `assets/maps/World_generation/rooms/boss/boss3_u.json` | Sama |
| `src/rendering/hud.cpp` | `UpdateBossMusic()` — CELL_BOSS → TiledHelper::GetObjectsByType("boss_music"); `DrawBossHPBar()` — pake trigger yang sama |
