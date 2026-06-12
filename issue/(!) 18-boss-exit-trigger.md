# Issue #13 — Boss Exit Trigger Terlalu Besar

## Tipe
Revisi (Design)

## Deskripsi
Object layer trigger untuk balik ke main menu setelah lawan boss ukurannya 928×928 (hampir seluruh prefab 992×992). Player bisa interact di mana aja di room boss.

## Root Cause
Di semua 3 boss prefab (`boss1_u.json`, `boss2_u.json`, `boss3_u.json`), object layer "object" berisi:

```json
{
    "height": 928,
    "id": 108,
    "name": "boss",
    "type": "pass",
    "width": 928,
    "x": 32,
    "y": 32,
    "properties": [
        {"name": "target_door", "type": "string", "value": "spawn"},
        {"name": "target_map", "type": "string", "value": "assets/maps/tutorial.json"}
    ]
}
```

Trigger 928×928 mencakup semua tile kecuali pinggir 32px.

## Alur Kode
1. `interaction.cpp:CheckDoors()` — `GetObjectsByType("pass")` ambil semua door
2. `CheckCollisionRecs(playerHitbox, door->bounds)` — deteksi overlap
3. `door->name == "boss"` — cek boss status
4. Boss mati → `g_SeedManager.ResetRun()` + `gState->currentScreen = MAIN_MENU`

## Solusi
Kecilin trigger jadi 32×32 atau 64×64, taruh di titik keluar (pintu masuk room boss atau area yang accessible setelah boss mati). Edit di 3 file:
- `assets/maps/World_generation/rooms/boss/boss1_u.json`
- `assets/maps/World_generation/rooms/boss/boss2_u.json`
- `assets/maps/World_generation/rooms/boss/boss3_u.json`

## Prioritas
Rendah (design issue, gak nge-break game)
