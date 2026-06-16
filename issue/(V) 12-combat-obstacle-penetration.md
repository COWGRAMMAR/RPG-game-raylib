# Bug #7 — Melee Attack Tembus Obstacle

**Tipe**: Bug  
**Komponen**: Combat  
**Status**: Selesai diperbaiki  

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

## Fix Implementasi

### Final Approach: Per-Entity LOS Step-based

**File**: `src/systems/combat.cpp` — `PerformHitDetection()`

**Konsep**: Tiap entity di-cek line-of-sight secara individu. Step dari `attackCenter` menuju `entityCenter` per FRAME_SIZE (32px). Setiap step: cek rect kecil (8x8) terhadap `solidObstacles`. Kena → skip entity itu aja.

**Solid Obstacles filter**:
- `gCollisionCache.rects` — static collision tiles dari Tiled (walls, etc.)
- `DynamicObstacles` minus crate + bomb (crate/bomb = destructible, gak nge-block attack)
- SLAM tetap pakai full `DynamicObstacles` (corridor hitbox)

**Perubahan** (dari tile tracing → per-entity LOS):
1. Hapus `effectiveReach` dan tile tracing loop (sebelumnya menghitung reach truncation)
2. Hapus binary guard (return early kena solid — false positive di stair walls)
3. Ganti dengan per-entity step-based LOS
4. `HitPropsByAttack()` tetap jalan normal tanpa LOS check — crate/bomb hancur walau ada barrier

**Kode** (PerformHitDetection, bagian entity loop):
```cpp
// Build solid obstacles
std::vector<Rectangle> solidObstacles = gCollisionCache.rects;
for (const auto &obs : DynamicObstacles)
{
    if (!crateManager.IsCratePos(obs) && !bombManager.IsBombPos(obs))
        solidObstacles.push_back(obs);
}

// ... entity loop ...
if (hit)
{
    // SLAM: corridor hitbox vs full DynamicObstacles
    // Non-SLAM: step-based LOS dari attackCenter ke entityCenter vs solidObstacles
    Vector2 entityCenter = {entity->Position.x + FRAME_SIZE/2, ...};
    Vector2 losDir = Vector2Subtract(entityCenter, attackCenter);
    // step per FRAME_SIZE, checkRect 8x8
    // kena solid → blocked, skip entity
}
```

## File Terkait

- `src/systems/combat.cpp` — PerformHitDetection(), CheckRadialCollision()
- `include/systems/combat.h` — Combat namespace
- `include/map/mapLogic.h` — DynamicObstacles declaration
- `src/map/mapLogic.cpp` — DynamicObstacles definition
