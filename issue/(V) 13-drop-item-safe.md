# Bug #8 — Drop Item Tidak Safe Position

**Tipe**: Bug  
**Komponen**: Player  
**Status**:  Selesai (12 Jun 2026)  

## Deskripsi

Player nge-drop item (dari inventory: keybind `Q` atau `Ctrl+Q`) bisa spawn item di dalam tile/wall karena gak pake `IsPositionSafe` check.

## Root Cause

**File**: `src/entities/player.cpp:510-514`

```cpp
Vector2 dropPos = {
    playerCenter.x + dropDir.x * INTERACT_RANGE,
    playerCenter.y + dropDir.y * INTERACT_RANGE};

ItemSpawn dropped = itemData.CreateItem(dropPos, slot.definitionId);
```

Posisi drop dihitung langsung dari arah tujuan (`aimDir`) dikali `INTERACT_RANGE`, lalu di-spawn langsung di posisi itu — tanpa ngecek apakah posisi itu walkable / gak colliding dengan tile.

## Fix

**Commit**: `(belum di-commit)`

Di `case ACTION_DROP_ITEM`, setelah hitung `dropPos`, ditambahkan:
1. Get `ItemDefinition` untuk akses `hitboxSize`
2. Hitung `topLeft` dari `dropPos - halfHitbox`
3. Panggil `IsPositionSafe(topLeft, hitboxSize.x, hitboxSize.y, 0, 0)`
4. Jika gak safe: coba 8 arah offset (tile size 32px)
5. Jika tetep gak safe: `break` (gak drop)

**File diubah**: `src/entities/player.cpp` — tambah ~30 baris safety check

## File Terkait

- `src/entities/player.cpp` — `case ACTION_DROP_ITEM` (line 460-570)
- `include/map/mapLogic.h` — `IsPositionSafe` declaration
