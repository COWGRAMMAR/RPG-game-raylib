# Buff Indicator System -- Dokumentasi Integrasi

## Overview

Sistem indikator buff menampilkan progress bar + sprite untuk efek aktif (Damage Up, Speed Up, Invincibility) di HUD player. Dibuat sebagai fitur shared yang bisa dipake baik di mode real-time maupun turn-based combat nanti.

## Pipeline Lengkap

```txt
UsePotion() [inventory.cpp]
  ↓
  Set buff timers + multipliers di Player instance
  Set per-kategori cooldown
  ↓
Player::Update() [player.cpp]
  ↓
  Tick timer: -= Time::DELTA_TIME per frame
  Reset multiplier + log event pas timer abis
  ↓
DrawPlayerHUD() [hud.cpp]
  ↓
  Panggil DrawBuffIndicators() -- render stacked progress bars
```

### 1. Trigger: UsePotion() -- `src/items/inventory.cpp:90-178`

Fungsi `UsePotion()` di-invoke pas player minum potion dari hotbar. Urutan:

1. **Cooldown check** (baris 106-111) -- per-kategori, tolak kalo masih cooldown
2. **Heal/mana** (baris 130-143) -- kalo potion punya healValue
3. **Set buff** (baris 146-167):
   - Damage: set `BuffDamageTimer`, `BuffDamageTimerMax`, `BuffDamageMultiplier`
   - Speed: set `BuffSpeedTimer`, `BuffSpeedTimerMax`, `BuffSpeedMultiplier`
   - Invincibility: set `InvincibilityTimer`, `InvincibilityTimerMax`
4. **Set cooldown** (baris 168-170) -- per-kategori

### 2. Tick: Player::Update() -- `src/entities/player.cpp:199-229`

Dipanggil tiap frame di `Player::Update()`. Tiap buff:

```txt
if (BuffDamageTimer > 0) {
    BuffDamageTimer -= Time::DELTA_TIME;
    if (BuffDamageTimer <= 0) {
        BuffDamageTimer = 0;
        BuffDamageMultiplier = 1.0f;
        Effects::AddLog("Efek Damage Berakhir");
    }
}
```

Hal yang sama untuk Speed dan Invincibility.

### 3. Reset on Death -- `src/ui/gameOverScreen.cpp:66-71`

Pas player mati (game over screen), buff di-reset:

```cpp
PlayerInstance.BuffDamageTimer = 0.0f;
PlayerInstance.BuffDamageTimerMax = 0.0f;
PlayerInstance.BuffSpeedTimer = 0.0f;
PlayerInstance.BuffSpeedTimerMax = 0.0f;
PlayerInstance.InvincibilityTimer = 0.0f;
PlayerInstance.InvincibilityTimerMax = 0.0f;
```

### 4. Render: DrawBuffIndicators() -- `src/rendering/hud.cpp:916-1008`

Static function, dipanggil dari `DrawPlayerHUD()` (baris 1239).

## Detail Implementasi DrawBuffIndicators()

### Lokasi

- **File**: `src/rendering/hud.cpp`
- **Fungsi**: `static void DrawBuffIndicators()` (static -- cuma dipake di file ini)
- **Dipanggil dari**: `DrawPlayerHUD()` baris 1239 -- SEBELUM `DrawHotbar()`
- **Posisi render**: Dihitung dari health bar position, stack ke ATAS (dari dash bar → mana bar → health bar → buff bars)

### Struktur Visual

Tiap entry buff terdiri dari:

```txt
[sprite 30px] ▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░  5s
```

- **Sprite**: `Frame` dari spritesheet (key: `damagePotionMedium`, `speedPotionMedium`, `invincibilityPotionMedium`)
- **Bar background**: `DrawRectangleRounded` DARKGRAY, 150x18px
- **Bar fill**: Warna khas tiap buff, ratio = timer / maxTimer
- **Highlight**: Overlay putih tipis 15% alpha di bagian atas bar
- **Timer text**: `FontId::HUD_PLAYER` 22px, `%ds` (≥1s) / `%.1fs` (<1s)
- **Spacing**: `buffGap=4px` antar entry

### Data Buff

```cpp
struct {
    const char *spriteKey;    // Key frame spritesheet
    float *timer;             // Pointer ke Player::BuffDamageTimer dkk
    float *maxTimer;          // Pointer ke Player::BuffDamageTimerMax dkk
    Color barColor;           // Warna progress bar
} buffs[] = {
    {"damagePotionMedium",     &PlayerInstance.BuffDamageTimer,     &PlayerInstance.BuffDamageTimerMax,     Color{255,106,0,255}},    // #FF6A00
    {"speedPotionMedium",      &PlayerInstance.BuffSpeedTimer,      &PlayerInstance.BuffSpeedTimerMax,      Color{0,255,233,255}},    // #00FFE9
    {"invincibilityPotionMedium", &PlayerInstance.InvincibilityTimer, &PlayerInstance.InvincibilityTimerMax, Color{142,167,178,255}}, // #8EA7B2
};
```

Hanya buff dengan timer > 0 yang dirender. Urutan stack: Damage → Speed → Invul (dari atas ke bawah).

### Variables di Player -- `include/entities/player.h:212-217`

```cpp
float BuffDamageTimer = 0.0f;        ///< Timer durasi buff damage
float BuffDamageTimerMax = 0.0f;     ///< Max durasi buff damage (untuk progress bar)
float BuffSpeedTimer = 0.0f;         ///< Timer durasi buff speed
float BuffSpeedTimerMax = 0.0f;      ///< Max durasi buff speed (untuk progress bar)
float InvincibilityTimer = 0.0f;     ///< Timer durasi invincibility
float InvincibilityTimerMax = 0.0f;  ///< Max durasi invincibility (untuk progress bar)
```

Juga:

```cpp
float BuffDamageMultiplier = 1.0f;   // Dipakai di sistem damage
float BuffSpeedMultiplier = 1.0f;    // Dipakai di sistem movement
```

### Cooldown Potion -- terpisah dari buff timer

Cooldown potion per-kategori disimpan di:

```cpp
float PotionCategoryCooldowns[POTION_CATEGORY_COUNT];      // Current cooldown
float PotionCategoryCooldownMax[POTION_CATEGORY_COUNT];    // Max cooldown (dari JSON)
```

Ini di-tick di `HandleInventoryActions()` tiap frame, dan di-render sebagai overlay CircleSector + teks di hotbar/inventory slot. **BUKAN** bagian dari buff indicator -- dua sistem terpisah.

## Design Decisions (untuk temen yang mau handle turn-based)

### 1. DrawBuffIndicators() saat ini hardcoded untuk HUD real-time

- Posisi Y dihitung relatif dari `GameScreenHeight - padding`
- Kalo turn-based punya screen sendiri / layout sendiri, bisa:
  - **Panggil ulang** `DrawBuffIndicators()` di screen render function turn-based
  - **Atau** extract logika per-entry loop (baris 960-1007) ke fungsi terpisah biar lebih reusable
  - **Atau** matikan panggilan di `DrawPlayerHUD()` kalo lagi turn-based mode

### 2. Array buffs[] static -- gampang ditambah

Array `buffs[]` di baris 934-938 hardcoded 3 entries. Kalo ada buff baru (Stamina regen, Poison, etc.), tinggal tambah entry + variable di Player. Warna panggung sendiri.

### 3. Sprite key hardcoded

Pake sprite `damagePotionMedium`, `speedPotionMedium`, `invincibilityPotionMedium`. Kalo mau ganti icon biar beda (misal icon sword buat damage buff), tinggal ganti string `spriteKey` di array.

### 4. Format timer

- `>= 1s`: tampil `%ds` (misal: `5s`)
- `< 1s`: tampil `%.1fs` (misal: `0.5s`)
- Bisa diganti sesuai kebutuhan turn-based (misal pake turn counter instead of seconds)

### 5. Posisi turn-based

Fungsi `DrawBuffIndicators()` saat ini nge-render di atas health bar (pojok kiri bawah). Kalo turn-based combat screen punya layout beda, recommend:

- Copy approach: panggil `DrawBuffIndicators()` dengan posisi X/Y yang di-pass sebagai parameter
- Atau buat version turn-based sendiri yang manggil loop entry yang sama

### 6. Reset buff antar mode

Pas transisi dari real-time ke turn-based (atau sebaliknya), buff timers tetap berjalan. Kalo mau pause buff timer pas turn-based, bisa set `Time::DELTA_TIME = 0` atau skip tick di `Player::Update()`.

### 7. Gak ada dependency ke combat system

`DrawBuffIndicators()` cuma baca variable Player dan render. Gak ada coupling ke sistem combat. Jadi temen bisa panggil kapan aja tanpa takut side effects.
