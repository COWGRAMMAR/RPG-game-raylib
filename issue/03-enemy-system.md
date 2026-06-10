# Enemy System — #5 #7 #8 Enemy/Boss/Elite gak nyerang

## Masalah
Enemy (normal, elite, boss) gak bisa detect player meskipun player udah masuk radius detection. Enemy diem aja (gak transisi ke chase mode).

| #   | Tipe       | Detail                                  |
| --- | ---------- | --------------------------------------- |
| 5   | Bug        | Enemy elite gak bisa detect player      |
| 7   | Bug        | Boss gak bisa detect (sama #5)          |
| 8   | Bug        | Enemy normal gak bisa detect (sama #5)  |

## Root Cause
Enemy AI pake raycast buat ngecek **line-of-sight** ke player sebelum transisi ke chase. Raycast-nya collide sama ghost obstacle dari `cachedObstacleList`, jadi enemy gak pernah dapet LOS → state machine mentok di idle/patrol.

**Pipeline obstacle:**
```
BuildObstacleList()
  ├── Tileson collision layer (static: rects, polygons)
  └── TilesonGetObjectsByType("crate", "bomb", "chest", "barrier", ...)
       └── Ini baca dari tilesonMap->objectIndex.byType — DATA STATIC PARSED MAP
```

**Masalahnya:** Pas crate/bomb diancurin:
-  Objek dihapus dari `DynamicObstacles` (runtime rect list di `mapLogic.h:278`)
-  `RebuildObstacleCache()` dipanggil
-  Tapi `BuildObstacleList()` tetap include objek yang udah hancur, karena `TilesonGetObjectsByType()` baca dari data Tiled map yang **statik**

Jadi enemy raycast collide sama crate yang udah gak ada → pathfinding mental.

## Fix Approach
**Filter obstacle list dengan runtime state.**

Ada 2 opsi:

### Opsi 1: Intersection check dengan DynamicObstacles
Di `BuildObstacleList()`, setelah ambil objek dari `TilesonGetObjectsByType()`, filter dengan ngecek apakah rect objek itu MASIH ada di `DynamicObstacles`. Kalo gak ada → skip.

```cpp
// BuildObstacleList — after reading Tileson objects
for (auto &obj : objects) {
    Rectangle r = GetTilesonObjectBounds(obj);
    bool stillExists = std::any_of(DynamicObstacles.begin(), DynamicObstacles.end(),
        [&](const Rectangle &dr) { return CheckCollisionRecs(r, dr); });
    if (!stillExists) continue;  // object destroyed, skip
    obstacles.push_back(r);
}
```

**Kelebihan:** Minimal invasif, gak perlu nambah state baru.
**Kekurangan:** `DynamicObstacles` pake rect approximation — bisa ada false match kalo posisi beda dikit. Tapi crate/bomb di-snap ke tile grid → aman.

### Opsi 2: Reference set object yang masih hidup
Pas SpawnObject / DestroyObject di propsbehavior.cpp, catat/tandai object mana yang masih aktif. Filter obstacle list berdasarkan set itu.

**Kelebihan:** Exact, gak ada false match.
**Kekurangan:** Nambah tracking state baru.

### Rekomendasi: **Opsi 1** dulu
- Simple, gak perlu nambah struct baru
- Tile-snapped objects → rect comparison akurat
- Kalo ada false match nanti baru upgrade ke Opsi 2

## File yang terlibat
- `src/ai/enemy_ai.cpp` — `BuildObstacleList()` (filter logic)
- `include/map/mapLogic.h` — `DynamicObstacles` (extern reference)
