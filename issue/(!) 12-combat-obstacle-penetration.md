# Bug #7 — Melee Attack Tembus Obstacle

**Tipe**: Bug  
**Komponen**: Combat  
**Status**: Belum diperbaiki  

## Deskripsi

Player bisa menyerang musuh yang berada di belakang obstacle (chest, crate, barrier, bomb, dll). Hitbox serangan melee tidak memeriksa apakah ada penghalang antara player dan target — efeknya serangan terasa "tembus" seperti hantu.

## Kronologi

Ditemukan saat main — player nyerang musuh yang jelas-jelas di belakang crate/barrier tapi kena damage. Untuk semua senjata melee (slash, thrust, slam).

## Root Cause

**File**: `src/systems/combat.cpp` — Fungsi `PerformHitDetection()` (line 454-528)

Flow damage entity:

```
PerformHitDetection()
  └─ Loop Entities::GetRegistry()
      ├─ Skip: entity == player / !IsActive / Health <= 0 / sudah di-damage
      ├─ CheckRadialCollision()  ← cuma cek angle + distance, TIDAK cek obstacle
      │   └─ Kalau hit → ApplyHitToEntity()
      └─ HitPropsByAttack()      ← ini urusan props doang, bukan ngeblock damage
```

**Masalah**: Tidak ada obstacle check antara `attackCenter` (origin serangan) dengan target entity. `CheckRadialCollision()` hanya menghitung apakah target berada dalam area berbentuk pie (angle + reach + breadth) — tidak peduli apa pun yang menghalangi.

**Referensi — arrow sudah bener**:
```cpp
// src/systems/combat.cpp line 728-741
// Arrow::Update() — projectile version
if (CheckCollisionAgainstRects(hitbox, DynamicObstacles))
{
    HitPropsByAttack(hitbox, PlayerInstance.GetHitbox(), &PlayerInstance);
    IsActive = false;  // arrow berhenti kena obstacle
    return;
}
```

`Arrow` ngecek collision dengan `DynamicObstacles` (global `std::vector<Rectangle>`) sebelum nge-damage entity. Melee attack gak ngelakuin hal yang sama.

## Data Penting

- `DynamicObstacles` — `extern std::vector<Rectangle>` declared di `include/map/mapLogic.h:278`
- `CheckCollisionAgainstRects(hitbox, DynamicObstacles)` — util function buat cek overlap dengan obstacles
- `CheckRadialCollision()` di `combat.cpp:264-288` — collision radial tanpa obstacle check
- `HitPropsByAttack()` — fungsi buat nge-damage props, dipanggil SETELAH entity loop

## Saran Fix

Di `PerformHitDetection()` — sebelum `ApplyHitToEntity()`:

1. Dapetin attack AABB / ray dari origin ke target
2. Cek apakah ada obstacle di `DynamicObstacles` yang meng overlap garis serangan
3. Kalau ada obstacle blocking → skip entity (gak kena damage)

Pattern yang bisa ditiru dari `Arrow::Update()`:

```cpp
// Pseudocode
if (hit)
{
    // Cek obstacle blocking
    Rectangle lineToTarget = BuildRayToTarget(attackCenter, entity->GetCenter());
    if (CheckCollisionAgainstRects(lineToTarget, DynamicObstacles))
        continue;  // skip — ada obstacle ngeblock

    ApplyHitToEntity(player, entity, attackCenter);
}
```

Atau bisa juga pake tile-based check — trace tile antara player dan target, kalau ada tile collision → block damage.

## File Terkait

- `src/systems/combat.cpp` — PerformHitDetection(), CheckRadialCollision()
- `include/systems/combat.h` — Combat namespace
- `include/map/mapLogic.h` — DynamicObstacles declaration
- `src/map/mapLogic.cpp` — DynamicObstacles definition
