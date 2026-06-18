# Issue #10 — Player Stuck di Barrier Saat Boss Room Re-lock

**Tipe**: Bug (Guard / Edge Case)  
**Komponen**: Barrier / Boss  
**Severitas**: KRITIS  
**Status**:  FIXED  

## Deskripsi

Saat player masuk ke room boss setelah 90% enemy dibunuh, barrier re-lock dan nge-trap player di dalam barrier kalau hitbox player masih overlap dengan tile barrier.

## Kronologi

1. Player bunuh 90% enemy → `RemoveAllBarriers()` — barrier hilang dari DynamicObstacles
2. Player jalan masuk ke boss room → hitbox player intersect `bossStageBounds`
3. `ReLockBarriers()` dipanggil — barrier dipasang lagi di entrance
4. **Masalah**: kalo posisi player tepat di tile barrier, player nge-stuck karena barrier jadi obstacle

## Root Cause

**File**: `src/map/propsbehavior.cpp`

Tidak ada pengecekan apakah player hitbox overlap dengan barrier yang akan di-restore.

## Fix

### 1. ReLockBarriers() — Skip overlap (Opsi C)
Barrier yang overlap player hitbox di-skip, di-set `pendingReLock = true`.

### 2. Update() Step 4 — Retry pending barriers
Tiap frame, barrier yang masih pending dicek: kalo player udah gak overlap, barrier di-restore.

### 3. Update() Step 5 — Unlock saat player keluar boss room
Guard tambahan: kalo `hasReLocked && !cleared` tapi player udah di luar `bossStageBounds`, barrier di-unlock otomatis. Fix untuk kasus player mati saat lawan boss → respawn di prefab start → barrier tetap lock.

**Flow:**
- Player di area boss → Step 2 lock barrier
- Player mati → respawn di luar → Step 5 unlock barrier
- Player masuk lagi → Step 2 lock lagi
- Boss mati → Step 3 unlock permanen

## File Berubah

- `src/map/propsbehavior.cpp` — BarrierManager::Update(), ReLockBarriers()
- `include/map/propsbehavior.h` — BarrierManager class (pendingReLock, hasReLocked, bossStageBounds)
