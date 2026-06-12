# Bug #11 — Bomb damage tembus obstacle

## Metadata
- **Tipe**: Bug
- **Prioritas**: Medium
- **Terkait**: Bug #7 (melee tembus obstacle) — root cause serupa

## Deskripsi
Saat bomb meledak, damage dari ledakan bomb bisa menembus obstacle (wall/dinding). Player atau enemy yang ada di belakang obstacle tetap kena damage padahal secara visual obstacle harusnya nge-block.

## Reproduksi
1. Temukan prefab room dengan bomb dan obstacle (wall) di antara bomb dan player
2. Serang bomb untuk memicu ledakan
3. Player kena damage padahal ada obstacle di antaranya

## Root Cause
`BombManager::Explode()` (propsbehavior.cpp:609-660) langsung cek `IsInExplosionRadius()` ke player/enemy **tanpa ngecek** apakah ada `DynamicObstacles` yang nge-block line of sight.

### Flow Explode():
```
Explode() dipanggil
  → IsInExplosionRadius(bombCenter, playerBounds)  // Cuma distance check
  → player->TakeDamage(BOMB_DAMAGE)                 // Langsung apply damage
  → IsInExplosionRadius(bombCenter, enemy->GetHitbox())  // Juga tanpa obstacle check
```

### `IsInExplosionRadius()` (line 672-679):
```cpp
float nearestX = Clamp(bombPos.x, target.x, target.x + target.width);
float nearestY = Clamp(bombPos.y, target.y, target.y + target.height);
float dist = Vector2Distance(bombPos, {nearestX, nearestY});
return dist <= BOMB_EXPLOSION_RADIUS;
```
Purely distance-based — gak ada obstacle awareness.

## Relevant Code
- `BombManager::Explode()` — propsbehavior.cpp:609-660
- `BombManager::IsInExplosionRadius()` — propsbehavior.cpp:672-679
- `DynamicObstacles` — vector of Rectangle untuk obstacle bounds

## Approach Fix
Tambahkan obstacle check di `Explode()`:
1. Untuk player damage (line 633): sebelum `player->TakeDamage()`, cek apakah ada DynamicObstacles antara bombCenter dan playerBounds
2. Untuk enemy damage (line 645): obstacle check yang sama

Pattern bisa ngikutin melee attack fix atau projectile obstacle check:
- Projectile arrow sudah punya `CheckCollisionAgainstRects(arrow.bounds, DynamicObstacles)` sebelum apply damage
- Melee fix approach ada di `issue/12-combat-obstacle-penetration.md`

## Catatan
- Bomb udah remove dirinya sendiri dari DynamicObstacles sebelum damage (line 620-624), jadi obstacle tersebut adalah dinding/obstacle lain, bukan bomb itu sendiri
- Perlu `g_PendingObstacleRebuild` atau `RebuildObstacleCache()` kalo obstacle list berubah
