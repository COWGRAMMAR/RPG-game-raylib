# Bug #9 — Item Pickup Tembus Obstacle

**Tipe**: Design Issue  
**Komponen**: Item  
**Status**: Low priority — biarkan dulu  

## Deskripsi

Item bisa dipickup walaupun secara visual terhadap obstacle (crate, barrier, dll). `CheckCollisionRecs(playerHitbox, item.hitbox)` gak ngecek apakah ada obstacle di antara player dan item.

## Root Cause

**File**: `src/items/item.cpp:489-494`

```cpp
if (CheckCollisionRecs(playerHitbox, item.hitbox))
{
    item.isPickedUp = true;
}
```

Pickup detection cuma hitbox overlap biasa. Tidak ada line-of-sight check terhadap `DynamicObstacles` atau `CollisionRects`.

## Rekomendasi — Biarkan Saja

**Alasan low priority:**

1. **Room 31×31** — ukuran prefab kecil dengan obstacle density tinggi. Obstacle-check buat pickup bakal sering false positive — item di belakang crate gak bisa diambil padahal crate bisa dihancurin.

2. **Magnet system** — item udah pake magnet (ditarik ke player dalam radius). Kalau magnet juga harus obstacle-aware, implementasinya complex (item bakal nyangkut di obstacle).

3. **Mayoritas obstacle destructible** — crate, barrel, bomb bisa dihancurin player kapan aja. Barrier cuma di entrance/exit room.

4. **Cost vs benefit** — perlu raycast tiap frame buat tiap active item di list untuk cek DynamicObstacles. Gameplay improvement minimal.

**Kalaupun mau implement:**
Sederhananya di `ItemRenderManager::Update()` — pas `CheckCollisionRecs` true, trace ray dari playerCenter → itemCenter. Kalau intersect `DynamicObstacles`, skip pickup. Tapi saran gw skip dulu.

## File Terkait

- `src/items/item.cpp` — `ItemRenderManager::Update()` (line 457-496)
- `include/map/mapLogic.h` — `DynamicObstacles` declaration
