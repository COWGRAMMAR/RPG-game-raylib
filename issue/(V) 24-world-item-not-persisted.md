# (!) 24 — World Item & Chest State Tidak Persisten Antar Map

## Masalah

Item di world berubah posisi tiap ganti stage, chest nutup lagi, item dari chest ilang:

1. **Posisi item berubah** — tiap balik ke stage yang udah pernah dikunjungi, item-item di lantai posisinya beda
2. **Item dari chest ilang** — chest udah dibuka, item keluar, tapi pas balik lagi item itu gak ada
3. **Chest nutup lagi** — chest yang udah pernah dibuka, pas balik ke stage jadi ketutup lagi
4. **Item dari enemy/player drop ilang** — apapun yang di-drop ke lantai ilang pas ganti map

## Root Cause

Ada **dua sumber item** yang konflik:

### Source A: Tiled Spawn Areas (SpawnAll)
- Waktu `InitMap()` → `ItemSpawnManager::SpawnAll()` dipanggil
- `SpawnAll()` **clear** `activeItems` → spawn ulang dari spawn area Tiled pake **RNG random position**
- Ini terjadi SETIAP map di-load, TERMASUK pas balik ke stage lama

### Source B: Snapshot/Checkpoint Items (snap.items)
- `CaptureSnapshot()` nyimpen semua `itemData.activeItems` ke `snap.items`
- Tapi di flow HandleMapSwitch, snapshot items cuma dipake buat **UUID matching** (update pickup state), BUKAN full replacement

### Flow detail:

**HandleMapSwitch case 1:**
```
if (HasCheckpoint):
    ApplyPreSpawn()    // set chestConsumed, crateConsumed — ✅ works
                       // TIDAK set items — ❌

SpawnObject()
  → SpawnAll()        // activeItems di-clear, spawn item baru dari Tiled dgn RNG — ❌
```

**HandleMapSwitch case 2:**
```
if (HasCheckpoint):
    ApplyPreSpawn()    // set consumed positions lagi — redundant, fine
    ApplyCheckpointData()
      → Items: UUID/position matching — update isPickedUp, posisi, definitionId
      → TAPI item yang ADA di checkpoint tapi TIDAK ADA di fresh spawn → ILANG
      → item yang ADA di fresh spawn tapi TIDAK ADA di checkpoint → TETAP MUNCUL
```

### Kenapa chest nutup lagi?

Ada **2 skenario**:

**Skenario A — Checkpoint gak ketemu:**
- `HandleMapSwitch` cek `HasCheckpoint(pendingMapPath, -1)` → `false`
- `ApplyPreSpawn` gak jalan → `chestConsumed` gak di-restore
- Chest spawn dengan state default (tertutup)

**Skenario B — Checkpoint ketemu tapi SpawnObject override:**
- `ApplyPreSpawn` set `chestConsumed` → chest-trigger skip spawn item
- TAPI item-item yang udah keluar dari chest (di `activeItems` sebelum checkpoint save) ilang karena `SpawnAll()` nge-clear

### Item dari enemy/player drop:

Chest/enemy drop item via `SpawnItemAtLocation()` → masuk `activeItems`. Item ini:
- Ke-capture di `CaptureSnapshot()` 
- Tapi pas map di-load ulang, `SpawnAll()` hapus semua dan cuma spawn item Tiled 
- Checkpoint gak restore item ini (cuma UUID matching, bukan full replacement)

## Akar Masalah — Design Gap

`ApplyCheckpointData()` melakukan **partial item restore** (UUID/position matching):
```cpp
// savemanager.cpp:1150-1183
// Hanya update existing items, TIDAK nambahin item dari checkpoint
for (const auto &saved : snap.items) {
    for (ItemSpawn &item : itemData.activeItems) {
        if (item.uuid == saved.uuid) {
            // update pickup state, position, etc.
        }
    }
}
```

Sementara `ApplyPostSpawn()` (dipanggil di `RestoreGameState` aja, bukan di map switch) melakukan **full replacement**:
```cpp
// savemanager.cpp:982-1003
itemData.activeItems.clear();
for (const auto &saved : snap.items) {
    ItemSpawn item;
    item.position = saved.position;
    // ... full restore
    itemData.activeItems.push_back(item);
}
```

## Fix

Bikin `ApplyCheckpointData()` pake **full replacement** buat items, sama kaya `ApplyPostSpawn()`:

```cpp
void SaveManager::ApplyCheckpointData(const GameSnapshot &snap)
{
    // ... existing enemy handling ...

    /*--- Items: full replacement dari checkpoint (source of truth) ---*/
    itemData.activeItems.clear();
    for (const auto &saved : snap.items)
    {
        ItemSpawn item;
        item.position = saved.position;
        item.isPickedUp = saved.isPickedUp;
        item.definitionId = saved.definitionId;
        item.amount = saved.amount;
        item.uuid = saved.uuid;
        // Rekonstruksi hitbox dari definisi
        const ItemDefinition &def = itemDefs.GetById(item.definitionId);
        float halfW = def.hitboxSize.x / 2.0f;
        float halfH = def.hitboxSize.y / 2.0f;
        item.hitbox = {item.position.x - halfW, item.position.y - halfH,
                       def.hitboxSize.x, def.hitboxSize.y};
        item.spawnTime = (float)GetTime();
        item.isAdded = item.isPickedUp;
        itemData.activeItems.push_back(item);
    }

    // ... rest of checkpoint data restore ...
}
```

## File yang terlibat

| File | Perubahan |
|------|-----------|
| `src/core/savemanager.cpp` | `ApplyCheckpointData()` — ganti partial UUID matching → full replacement items |
| `src/core/loading_screen.cpp` | Mungkin perlu urutan ulang biar SpawnAll gak override checkpoint items |

## Catatan

- `snap.items` di checkpoint berisi ALL world items (dari CaptureSnapshot → activeItems)
- Data ini lengkap: position, definitionId, amount, uuid, isPickedUp
- Full replacement aman karena checkpoint = source of truth untuk state map terdahulu
- Items dari chest/enemy/player drop tetap ke-capture karena semua ada di `activeItems`
- Pastikan `snap.items` dari checkpoint gak kosong sebelum full replace (guard `if (snap.items.empty() && !hasSavedState)`)
