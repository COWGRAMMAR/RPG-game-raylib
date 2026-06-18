# Issue #14 — Boss Music Trigger Point — FIXED

## Tipe
Revisi (Design) — **SELESAI**

## Deskripsi
Boss music (`BossMusic.mp3`) diputar saat player **masuk prefab boss room** atau **masuk detection range boss**, dan balik ke music biasa saat player keluar area boss.

## Implementasi Final

### Additional Fix (2026-06-13)
- Trigger dari `IsCellTypeAtWorldPos(CELL_BOSS)` diganti ke **`TiledHelper::GetObjectsByType("boss_music")`** — object `boss_music` yang ditambah di prefab JSON
- `DrawBossHPBar()` juga pake trigger yang sama: overlap Tiled object `boss_music` + detection range boss
- `UpdateBossMusic()` dan `DrawBossHPBar()` konsisten pake `inRange || inBossArea`

### Ambient System (hud.cpp — UpdateBossMusic())
```cpp
static bool s_BossMusicActive = false;

bool boss = false;
for (auto &e : Entities::GetEnemyRegistry()) {
    if (e.IsActive && e.rank == ENEMY_BOSS && e.Health > 0) {
        Vector2 playerPos = PlayerInstance.GetCenter();
        float range = e.DetectionRange * 1.5f;
        Vector2 bossCenter = e.GetCenter();
        bool inRange = CheckCollisionCircleRec(playerPos, range, {bossCenter.x - range, bossCenter.y - range, range * 2, range * 2});
        bool inPrefab = IsCellTypeAtWorldPos(PlayerInstance.GetCenter(), CELL_BOSS);
        boss = (inRange || inPrefab || TurnCombat::IsActive());
        break;
    }
}

if (boss && !s_BossMusicActive) {
    AudioManager::PlayTrack("Boss");
    s_BossMusicActive = true;
} else if (!boss && s_BossMusicActive) {
    // Victory phase guard — biar WinTheme dari combatTurn jalan tanpa gangguan
    if (TurnCombat::IsActive() && TurnCombat::GetPhase() == TurnPhase::VICTORY) {
        AudioManager::BlockAutoSwitch();
    } else {
        AudioManager::UnblockAutoSwitch();
        AudioManager::ResetToScreenTrack();
    }
    s_BossMusicActive = false;
}
```

### AudioManager Changes
- `_isBossMusic`, `EnterBossMusic()`, `ExitBossMusic()`, `IsBossMusicActive()` — **Dihapus** (static local flag di hud.cpp)
- `BlockAutoSwitch()` / `UnblockAutoSwitch()` — flag `_blockAutoSwitch` skips auto-switch di `Update()`

### Guard Logic
- `s_BossMusicActive` mencegah double-play
- Victory phase: `BlockAutoSwitch()` biar WinTheme jalan tanpa overwrite
- Player mati: flag cleared, `AudioManager::Update()` handle GAME_OVER auto-switch

### Cleanup
- `enemy.cpp`: Old `bossMusicPlaying` + PlayTrack("Boss") di UpdateAI dihapus
- `enemy.cpp`: `ClearEnemies()` gak perlu `IsBossMusicActive()` lagi
- `combatTurn.cpp`: PlayTrack("Boss") line 101 dihapus (ambient system handles it)
- `enemy.h`: `bossMusicPlaying` field removed

### File yang Diubah
| File | Perubahan |
|------|----------|
| `include/systems/audioManager.h` | Hapus Enter/Exit/IsBossMusicActive, tambah Block/UnblockAutoSwitch |
| `src/systems/audioManager.cpp` | Hapus _isBossMusic, enter/exit/IsActive. Tambah _blockAutoSwitch guard |
| `include/rendering/hud.h` | Update comment (no Enter/ExitBossMusic) |
| `src/rendering/hud.cpp` | UpdateBossMusic rewrite pake static local flag + victory guard |
| `src/core/main.cpp` | Hapus AudioManager::ExitBossMusic() di isDead branch |
| `src/entities/enemies/enemy.cpp` | Hapus bossMusicPlaying + IsBossMusicActive di ClearEnemies |
| `include/entities/enemy.h` | Hapus bossMusicPlaying field |
| `src/systems/combatTurn.cpp` | Hapus PlayTrack("Boss") line 101 |
