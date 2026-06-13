# Enemy System — #4 Enemy idle gak wandering

## Masalah
Enemy pas state IDLE diem aja di tempat, gak gerak random (wander/patrol). Kayaknya obstacle padet di prefab bikin enemy gak bisa nemu patrol target.

## Root Cause

### HandleIdle Flow (enemy.cpp:411-443)
```
PatrolTimer += Time::DELTA_TIME
if (PatrolTimer >= PatrolWaitTime)     // 2 detik
  → PatrolTarget = SpawnPoint          // fallback
  → patrolRetryLimit = 10
  → for (i = 0; i < 10; i++)
      → random angle 0-360°
      → random radius FRAME_SIZE(32) sampai patrolRadius(130)
      → potentialTarget = SpawnPoint + (angle, radius)
      → if (IsPositionSafe(potentialTarget, ...))
          → PatrolTarget = potentialTarget
          → break
  → AIState = ENEMY_PATROL
```

### Masalah utama: **Obstacle density tinggi**
- Prefab room ukuran 31×31 tiles (992×992 px) — penuh tembok, furniture, props
- `IsPositionSafe()` ngecek collision dengan **semua** obstacles (tiles + props)
- `patrolRetryLimit` cuma **10** — di room padat, 10 random attempt hampir pasti gagal
- Fallback → `PatrolTarget = SpawnPoint` → enemy patrol ke spawn point sendiri → begitu sampe (distance < 10px) → balik IDLE lagi → loop forever

### Akar masalah detail
1. `patrolRadius` default 130px — tapi banyak prefab yang radius segitu udah nembus dinding
2. `patrolRetryLimit = 10` — terlalu kecil untuk room 31×31 tiles dengan obstacle density tinggi
3. Gak ada fallback progressive — kalo 10× gagal, enemy cuma patrol ke SpawnPoint (diem doang)

## Fix Approach (belum diimplementasi)

### Opsi 1: Naikin retry limit + smarter fallback
- Naikin `patrolRetryLimit` dari 10 ke 50
- Kalo semua retry gagal → coba offset kecil (1-2 tile) dari spawn point, bukan spawn point itself

### Opsi 2: Dynamic patrol radius
- Kurangi `patrolRadius` secara bertahap tiap retry
- Mula-mula 130px, turun 10px tiap retry → lebih gampang dapet safe position di radius kecil
- Atau pake multi-radius sweep: 130, 100, 70, 40

### Opsi 3: Pake random walk incremental
- Daripada random position absolute, coba **random offset kecil** dari current position
- 1-3 tile random direction → lebih mungkin safe daripada lompat jauh

### Rekomendasi: **Opsi 1 + 3**
- Naikin `patrolRetryLimit` → 50
- Kalo semua gagal → incremental random walk (offset 1-2 tile)

## File yang terlibat
- `src/entities/enemies/enemy.cpp` — HandleIdle(), patrolRetryLimit, IsPositionSafe call
- `include/entities/enemy.h` — PatrolWaitTime constant

## Status
- Root cause:  Found
- Fix approach:  Dicatat (menunggu implementasi)
- **Fix**: patrolRetryLimit 10→50, fallback incremental 10× (32-64px) dari Position — Selesai 12 Jun 2026
