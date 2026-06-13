# Issue #10 — Player Stuck di Barrier Saat Boss Room Re-lock

**Tipe**: Bug (Guard / Edge Case)  
**Komponen**: Barrier / Boss  
**Severitas**: KRITIS  
**Status**: Belum diperbaiki  

## Deskripsi

Saat player masuk ke room boss setelah 90% enemy dibunuh, barrier re-lock dan nge-trap player di dalam barrier kalau hitbox player masih overlap dengan tile barrier.

## Kronologi

1. Player bunuh 90% enemy → `RemoveAllBarriers()` — barrier hilang dari DynamicObstacles
2. Player jalan masuk ke boss room → hitbox player intersect `bossStageBounds`
3. `ReLockBarriers()` dipanggil — barrier dipasang lagi di entrance
4. **Masalah**: kalo posisi player tepat di tile barrier, player nge-stuck karena barrier jadi obstacle

## Root Cause

**File**: `src/map/propsbehavior.cpp`

**Flow re-lock** (BarrierManager::Update, line 1054-1063):
```cpp
if (cleared && !hasReLocked && bossStageBounds.width > 0 && bossStageBounds.height > 0)
{
    if (CheckCollisionRecs(playerBounds, bossStageBounds))
    {
        ReLockBarriers();   // ← barrier di-restore tanpa cek overlap player
        hasReLocked = true;
    }
}
```

**ReLockBarriers** (line 1170-1181):
```cpp
void BarrierManager::ReLockBarriers()
{
    for (auto &b : barriers)
    {
        if (b.isActive) continue;
        b.isActive = true;
        DynamicObstacles.push_back(b.tile.bounds);  // ← langsung push tanpa guard
    }
    cleared = false;
    RebuildObstacleCache();
}
```

Tidak ada pengecekan apakah player hitbox overlap dengan barrier yang akan di-restore.

## Saran Fix

### Opsi A — Cancel lock kalo overlap (user suggestion)
Di `ReLockBarriers()`, sebelum barrier di-restore, cek tiap barrier bounds terhadap player hitbox:

```cpp
void BarrierManager::ReLockBarriers()
{
    Rectangle playerBounds = PlayerInstance.GetHitbox();
    for (auto &b : barriers)
    {
        if (b.isActive) continue;
        
        // Guard: skip barrier yang masih ke overlap player
        if (CheckCollisionRecs(playerBounds, b.tile.bounds))
            continue;  // barrier ini ditunda sampai player gak overlap
        
        b.isActive = true;
        DynamicObstacles.push_back(b.tile.bounds);
    }
    cleared = false;
    RebuildObstacleCache();
}
```

Kekurangan: barrier yang di-skip gak akan pernah ke-restore. Perlu logic retry/monitor.

### Opsi B — Timer jeda (user suggestion)
Jangan langsung re-lock pas player masuk. Kasih delay (misal 0.5-1 detik) sebelum barrier beneran di-restore:

```cpp
// Di BarrierManager — tambahin lockDelay timer
if (cleared && !hasReLocked && lockDelay <= 0 
    && CheckCollisionRecs(playerBounds, bossStageBounds))
{
    lockDelay = 0.5f;  // 0.5 detik delay
}

if (lockDelay > 0)
{
    lockDelay -= Time::DELTA_TIME;
    if (lockDelay <= 0)
    {
        ReLockBarriers();
        hasReLocked = true;
    }
}
```

Player punya waktu 0.5 detik untuk keluar dari area barrier.

### Opsi C — Guard overlap + retry di Update
Gabungan: skip barrier yang overlap, dan di `Update()` tiap frame cek apakah barrier yang di-skip masih overlap:

```cpp
void BarrierManager::Update()
{
    // ... existing logic ...
    
    // Cek barrier yang pending (di-skip karena overlap)
    if (hasReLocked && !cleared)
    {
        Rectangle playerBounds = PlayerInstance.GetHitbox();
        for (auto &b : barriers)
        {
            if (!b.isActive && !CheckCollisionRecs(playerBounds, b.tile.bounds))
            {
                b.isActive = true;
                DynamicObstacles.push_back(b.tile.bounds);
            }
        }
        RebuildObstacleCache();
    }
}
```

## File Terkait

- `src/map/propsbehavior.cpp` — BarrierManager::Update(), ReLockBarriers()
- `include/map/propsbehavior.h` — BarrierManager class
- `include/map/mapLogic.h` — DynamicObstacles
