# Bug #16 — Restart dari pause menu saat combat turn aktif

**Tipe**: Bug
**Status**: (V) Selesai (12 Jun 2026) — TurnCombat::Shutdown() sebelum Entities::Clear()
**Prioritas**: Critical
**File terkait**: `pauseMenu.cpp`, `combatTurn.cpp`

---

## Deskripsi

Saat player melawan boss dalam turn-based combat (TurnCombat aktif), lalu membuka pause menu dan memilih **Restart**, player akan stuck di sistem combat turn. Kadang game juga crash. **Terjadi di map non-worldgen sama kemungkin map worldgen.**

## Kronologi

1. Player trigger boss → `TurnCombat::Init()` dipanggil → `state.active = true`, `state.boss`指向 boss enemy instance
2. Player buka pause menu (ESC)
3. Player klik **Restart** (pause menu button index 4)
4. `HandleButtonClick(4, state)` → `restartConfirmPopup.Show()`
5. Player confirm restart → `restartConfirmPopup.IsConfirmClicked()` → true
6. Restart flow di `PauseMenu::Update()` (line 631-693) jalan:
   - `Entities::Clear()` → **HAPUS SEMUA ENTITY** termasuk boss
   - Clear tile props, spike, bomb, crate, barrier, dll.
   - Spawn enemies fresh
   - `PlayerInstance.ResetForNewGame()` + `Init()`
   - `currentScreen = PLAY`
7. **`TurnCombat::Shutdown()` TIDAK PERNAH DIPANGGIL**

## Root Cause

`PauseMenu::Update()` → restart flow (line 631-693) tidak memanggil `TurnCombat::Shutdown()` sebelum `Entities::Clear()`.

Akibat:

- `TurnCombat` static state: `state.active = true` (tidak pernah direset)
- `state.boss` → **dangling pointer** (boss object sudah di-delete oleh `Entities::Clear()`)
- `state.player` → dangling pointer (Player sudah di-reinit)

Flow setelah restart:

1. Game loop frame berikutnya → `UpdateLogicAll()` dipanggil
2. `TurnCombat::IsActive()` → **true**
3. `TurnCombat::Update()` → **akses `state.boss->Health`** → **CRASH** (dangling pointer / use-after-free)
4. Kalau gak crash: stuck di combat turn karena phase gak pernah direset ke INACTIVE

## Fix

Di `PauseMenu::Update()` → restart flow, sebelum `Entities::Clear()`, panggil:

```cpp
TurnCombat::Shutdown();
```

Ini akan:

1. Set `state.active = false`
2. Set `state.phase = INACTIVE`
3. Set `state.boss->isTurnBasedMode = false`
4. Null-kan `state.boss` dan `state.player`
5. Reset camera
6. Balikin music ke screen track

`TurnCombat::Shutdown()` sudah handle dangling pointer safety:

```cpp
if (state.boss) { ... }
if (state.player) { ... }
```

Jadi aman dipanggil kapan pun, bahkan sebelum Entities::Clear().

## Reproduce

1. Start game (map non-worldgen)
2. Masuk ke room boss
3. Combat turn aktif
4. Buka pause menu (ESC)
5. Klik Restart → confirm
6. **Expected**: game restart normal, player spawn di awal
7. **Actual**: stuck di combat turn overlay / crash

## Referensi

- `src/ui/pauseMenu.cpp:631-693` — restart flow (missing Shutdown call)
- `src/systems/combatTurn.cpp:665-693` — `TurnCombat::Shutdown()` implementation
- `src/systems/combatTurn.cpp:20-51` — static state struct
