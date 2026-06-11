# Player / Combat — #3 Item Spawn Mati + #6 Gameover Double

### Status: Both fixed di commit sebelumnya
- #6: Hapus `Health=0` dari DEFEAT phase — `isDead` jadi satu-satunya trigger
- #3: `DropAllItems()` pake `player.GetCenter()`, single spread 40, collision check via `IsPositionSafe`

---

## #6 — Player mati 2× saat lawan boss

### Masalah
Player kalah di turn-based boss → menu gameover muncul → revive → game freeze bentar → gameover muncul lagi.
Harus revive 2× biar bisa lanjut.

### Root Cause
DEFEAT phase di `combatTurn.cpp` punya **2 mekanisme death yang overlap**:

```
DEFEAT phase:
  1. isDead = true (immediate)  → main.cpp → GAME_OVER (1st trigger)
  2. 2 detik → Health = 0       → HandleDead() lihat Health ≤ 0 → isDead lagi → GAME_OVER (2nd trigger)
```

`isDead` flag adalah trigger pertama ke game over screen. Tapi DEFEAT phase juga set `Health = 0` setelah delay 2 detik.

Setelah revive:
- Health di-set ke MaxHealth
- Tapi `HandleDead()` di `Combat::Update()` lihat Health — udah > 0, skip 
- **Tapi** `defeatCooldown` masih jalan, boss `isTurnBasedMode = true` (di-set tiap frame via `UpdateAI()`)
- Selama `defeatCooldown > 0`, trigger TurnCombat di-guard (gak re-trigger)
- Boss menyerang player di real-time → player mati → gameover lagi

Intinya: `Health = 0` di DEFEAT itu bikin **double entry point** untuk death. Dua jalur beda yang ujungnya sama: gameover screen.

### Fix
**Hapus `Health = 0` dari DEFEAT phase. `isDead` jadi satu-satunya trigger.**

```cpp
// combatTurn.cpp — DEFEAT phase (sebelum)
case TurnPhase::DEFEAT: {
    if (!state.keyProcessed) {
        PlayAnimation(state.player->Anim, DEAD, RIGHT);
        state.player->Anim.isDead = true;
        state.keyProcessed = true;
    }
    if (state.combatTimer >= 2.0f || ...) {
        state.player->Health = 0;              // ❌ HAPUS INI
        state.defeatCooldown = 6.0f;
        Shutdown();
    }
    break;
}
```

```cpp
// combatTurn.cpp — DEFEAT phase (sesudah)
case TurnPhase::DEFEAT: {
    if (!state.keyProcessed) {
        PlayAnimation(state.player->Anim, DEAD, RIGHT);
        state.player->Anim.isDead = true;
        // Drop items langsung disini, bukan nunggu HandleDead
        state.player->DropAllItems();
        state.player->hasDroppedItems = true;
        state.keyProcessed = true;
    }
    if (state.combatTimer >= 2.0f || ...) {
        state.defeatCooldown = 6.0f;
        Shutdown();
    }
    break;
}
```

### Flow setelah fix
```
DEFEAT → isDead=true, drop items → 2s → Shutdown()
  → main.cpp liat isDead → GAME_OVER (1× aja)
  → Revive → isDead=false, Health=MaxHealth
  → HandleDead() cek Health > 0 → skip ✅
  → defeatCooldown masih jalan → guard trigger ✅
  → Player aman
```

### File yang terlibat
| File | Perubahan |
|------|-----------|
| `src/core/combatTurn.cpp` | Hapus `Health=0` dari DEFEAT, drop items langsung di phase |
| `src/core/gameOverScreen.cpp` | Revive handler — sudah benar (reset isDead + Health) |
| `src/core/combat.cpp` | `HandleDead()` — mungkin perlu penyesuaian kalo cuma dipake buat non-boss death |

---

## #3 — Item spawn saat player mati

### Masalah
Sekarang item di-drop via `DropAllItems()` pas player mati. Posisi spawn item random di sekitar player.
Ada 2 fungsi yang overlapping:

| Fungsi | Origin | Spread | Retry |
|--------|--------|--------|-------|
| `DropAllItems()` | `player.Position` (top-left) | ±40px di caller + ±32px di `CreateItem` | 5 |
| `SpawnLootSafe()` | Center posisi chest/crate | ±spread per retry | 15 |

### Expected behavior
Item drop saat player mati — simple, gak perlu perfect placement. Kalo ada item yang overlap (jatuh di posisi sama) itu wajar.

### Analisis
`DropAllItems()` udah jalan cukup baik:
- 5 retry per item
- Random position ±40 dari player
- Fallback ke random position (walau mungkin di wall)

Yang bisa diperbaiki:
- **Double spread** — `DropAllItems()` ngasih spread ±40, trus `CreateItem()` nambah spread ±32 lagi. Jadi total spread gak konsisten.
- **Origin inconsistency** — `player.Position` itu top-left, bukan center. Better pake `player.Center`.

### Fix (minor)
1. Panggil `DropAllItems()` langsung di DEFEAT phase (udah di-cover di #6 fix)
2. Konsistenin spread — pake 1 level aja (di `DropAllItems`, bukan di `CreateItem`)
3. Pake `player.Center` sebagai origin, bukan `player.Position`

### File yang terlibat
| File | Perubahan |
|------|-----------|
| `src/items/item.cpp` | `CreateItem()` — hapus extra spread, pake origin langsung |
| `include/entities/player.h` | `DropAllItems()` — signature maybe need center param |
