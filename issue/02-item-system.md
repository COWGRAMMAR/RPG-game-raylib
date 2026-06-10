# Item System — #2 Loot Chance + #4 Potion Cooldown

## #2 — Enemy Loot Drop Chance

### Masalah
Sekarang enemy tiap mati ***guarantee*** drop item. Seharusnya ada chance tertentu, gak selalu drop.

### Fix Approach

**3 parameter independen:**

| Parameter                  | Fungsi                   | Default Global |
| -------------------------- | ------------------------ | -------------- |
| `dropChance`               | IF drop terjadi          | 25%            |
| `potionWeight:weaponWeight` | KATEGORI item            | 70:30          |
| `rarityWeights`            | RARITY item (eksisting)  | per rank       |

Ketiganya **independen** — gak ada yang overlap atau bikin parameter lain redundant.

### Pipeline

```
┌─ Enemy Mati ──────────────────────────────────────────────────┐
│                                                                │
│  1. Roll `dropChance` (25%)                                    │
│     ├── Gagal → selesai, gak ada item                          │
│     └── Berhasil → lanjut                                      │
│                                                                │
│  2. Tentukan KATEGORI (potion vs weapon)                       │
│     Roll `potionWeight` : `weaponWeight` dari enemy type       │
│     ├── Potion → lanjut step 3a                                │
│     └── Weapon → lanjut step 3b                                │
│                                                                │
│  3a. Potion:                                                   │
│      Roll `rarityWeights` → dapet item rarity                  │
│      Cari item potion di itemDefs dengan rarity tsb            │
│      → SpawnItemAtLocation(Position, rarity, category=POTION)  │
│                                                                │
│  3b. Weapon:                                                   │
│      Roll `rarityWeights` → dapet item rarity                  │
│      Cari item weapon di itemDefs dengan rarity tsb            │
│      → SpawnItemAtLocation(Position, rarity, category=WEAPON)  │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

### Per-Enemy Type Bias (contoh awal)

| Enemy         | dropChance | Potion Weight | Weapon Weight | Keterangan                |
| ------------- | ---------- | ------------- | ------------- | ------------------------- |
| Slime         | 25%        | 9             | 1             | "si slime bawa potion"    |
| Skeleton      | 25%        | 2             | 8             | "skeleton tukang pedang"  |
| Wolf          | 25%        | 5             | 5             | Mixed                     |
| Slime_Elite   | 40%        | 3             | 7             | Elite bias weapon         |
| Boss          | 100%       | 5             | 5             | Guaranteed drop           |

> **Catatan:** Nilai di atas bisa di-tuning pas implement. Angka di sini sebagai referensi awal.

### Data Changes per Layer

| Layer                 | Isi Perubahan                                                                 |
| --------------------- | ----------------------------------------------------------------------------- |
| `assets/data/enemies.json` | Tambah 3 field tiap entry: `dropChance`, `potionWeight`, `weaponWeight`       |
| `EnemyDefinition` (enemy.h) | Tambah 3 member: `dropChance`, `potionWeight`, `weaponWeight`                 |
| `EnemyDataManager::Load()`  | Parse 3 field baru dari JSON, fallback default kalo gak ada                   |
| `enemy.cpp`            | Ganti hardcode jadi: `ShouldDropItem()` + `RollCategory()` + `SpawnItemAtLocation(rarity, category)` |

### Fungsi baru

| Fungsi                            | Logic                                                     |
| --------------------------------- | --------------------------------------------------------- |
| `ShouldDropItem(dropChance)`      | `rand() < dropChance`                                     |
| `RollCategory(potionW, weaponW)`  | `rand() % (potionW + weaponW) < potionW` → POTION else WEAPON |
| `SpawnItemAtLocation(pos, rarity, category)` | Overload baru — cari item dari itemDefs by category + rarity |

---

## #4 — Potion Cooldown Sinkron

### Masalah
1. **Cooldown global** — semua potion share 1 timer (`player.PotionCooldown`). Pake 1 potion → semua potion kena cooldown.
2. **UI cooldown unified** — animasi cooldown di hotbar ngikut 1 timer global, bukan per-kategori.
3. JSON sudah punya field `potion.cooldown` (1.0 / 5.0 / 10.0) — tapi gak kepake optimal karena timer global.
4. Belum ada field `potionCategory` di JSON — kategori cuma bisa di-infer dari field lain (rapuh).

### Expected
- **Per-kategori cooldown** — tiap kategori potion punya timer sendiri. Satu kategori kena cooldown gak ngaruh ke kategori lain.
- UI cooldown animation ngikut timer kategori, render overlay di tiap slot yang kategorinya lagi cooldown.
- Cooldown duration dibaca dari `items.json` → `potion.cooldown`.

### Skenario
Player punya 6 slot hotbar:

| Slot | Item             | Kategori | Cooldown |
| ---- | ---------------- | -------- | -------- |
| 1    | Health Small     | health   | 1s       |
| 2    | Health Medium    | health   | 5s       |
| 3    | Stamina Small    | stamina  | 1s       |
| 4    | Damage Small     | damage   | 5s       |
| 5    | Damage Medium    | damage   | 10s      |
| 6    | Speed Small      | speed    | 5s       |

Flow:
1. Player pake **slot 1** (Health Small, cd 1s) → health timer = 1s. Slot 1 & 2 cooldown. Slot 3-6 normal.
2. Player coba pake **slot 2** (Health Medium) → health timer masih jalan → DITOLAK.
3. Player pake **slot 3** (Stamina Small) → stamina timer = 1s (independent).
4. Player pake **slot 4** (Damage Small, cd 5s) → damage timer = 5s. Slot 4 & 5 cooldown.
5. Player pake **slot 6** (Speed Small) → speed timer = 5s.

Timer cooldown pake **duration potion yang terakhir dipake** di kategori itu. Contoh: pake Health Medium (5s) → timer 5s, abis itu pake Health Small (1s) → timer ganti 1s.

### Data Changes

**`assets/data/items.json`** — tiap entry potion tambah field:
```json
"potion": {
    "potionCategory": "health",  // "health" / "stamina" / "damage" / "speed"
    "healValue": 20,
    "isMana": false,
    "cooldown": 1.0
}
```

### Kenapa bukan per-slot
Kalo per-slot, player bisa spam 3 health potion dengan tinggal ganti slot → cooldown meaningless.

### Fix Approach

**Cooldown tracker di inventory, bukan di Player:**

```cpp
// struct di inventory.h — cooldown state per kategori
struct PotionCooldownState {
    float timer = 0.0f;
    float duration = 0.0f;  // dari potion.cooldown (yg terakhir dipake)
};
```

Kategori di-map pake enum:
```cpp
enum class PotionCategory {
    Health,
    Stamina,
    Damage,
    Speed,
    Count  // 4 kategori
};
```

### Flow
```
UsePotion(slot):
   1. Baca items.json → dapet potionCategory
   2. Index = (int)potionCategory
   3. Cek cooldownTracker[index].timer > 0?
      → Ya: return "Potion sedang cooldown!"
      → Tidak: lanjut
   4. Apply efek (heal / mana / buff)
   5. Set cooldownTracker[index].timer = potion.cooldown
   6. Set cooldownTracker[index].duration = potion.cooldown
   7. Kurangi amount slot
```

### UI
- Hotbar draw: tiap slot cek `cooldownTracker[(int)potionCategory].timer`
- Kalo > 0 → render cooldown overlay (muter/alpha) sesuai `timer / duration`
- Kalo 0 → normal

### File yang terlibat
| File                          | Perubahan                                                             |
| ----------------------------- | --------------------------------------------------------------------- |
| `assets/data/items.json`      | Tambah field `potionCategory` ke tiap entry potion                    |
| `include/items/inventory.h`   | Tambah `PotionCategory` enum + `PotionCooldownState` + array tracker  |
| `src/items/inventory.cpp`     | Ganti `player.PotionCooldown` → `cooldownTracker[index]`              |
| `include/entities/player.h`   | Hapus `PotionCooldown` / `PotionCooldownMax` (pindah ke inventory)    |
| `src/ui/hud.cpp` (kira-kira)  | Baca cooldown per-kategori buat render animasi                        |
