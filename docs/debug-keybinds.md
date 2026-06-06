# Debug Keybinds

Dokumen ini mencatat semua keybind untuk keperluan **debug dan development**. Tidak seperti keybind biasa (Movement, Combat, Inventory, Hotbar), keybind ini tidak muncul di UI Settings > Keybinds karena disembunyikan secara sengaja -- hanya bisa diakses melalui default binding yang sudah di-set di `keybindManager`.

## Daftar Debug Keybinds

| Action | Default Key | Deskripsi |
| --- | --- | --- |
| `REVIVE` | `R` | Menghidupkan kembali player (debug). Berguna saat player mati dan ingin testing tanpa restart. |
| `TEST_LOSE_HP` | `K` | Mengurangi HP player untuk testing damage, knockback, atau death animation. |
| `GO_BACK` | `B` | Kembali ke state sebelumnya (debug). Untuk testing map switching atau scene transition tanpa harus lewat trigger normal. |
| `PAUSE_MENU` | `Escape` | Membuka/menutup pause menu. **Tidak bisa di-rebind melalui UI settings.** Escape selalu hardcoded sebagai toggle pause. |
| `DEBUG_TOGGLE` | `Tab` | Menampilkan/menyembunyikan overlay debug utama (FPS, koordinat, memory usage, dll). |
| `DEBUG_TOGGLE_ENEMY` | `\` | Menampilkan/menyembunyikan debug info enemy -- bounding box, HP, AI state, pathfinding target, dll. |
| `DEBUG_TOGGLE_PLAYER` | `]` | Menampilkan/menyembunyikan debug info player -- position, velocity, state machine, hitbox, cooldown, dll. |

## Kenapa Disembunyikan?

Keybind ini sengaja **tidak dimasukkan ke section manapun** di `keybindsTab.cpp` (lihat struct `sections[]` yang hanya mencakup MOVEMENT, COMBAT, INVENTORY, HOTBAR). Akibatnya:

- Tidak bisa di-rebind melalui UI Settings > Keybinds
- Tidak muncul di daftar keybind yang bisa diedit player
- Tetap **berfungsi penuh** karena `keybindManager` tetap memuat default binding-nya

Jika suatu saat diperlukan rebindability, cara termudah adalah menambahkan section baru di `keybindsTab.cpp` atau mengekspos action ini di debug menu.

## PAUSE_MENU -- Pengecualian Khusus

`PAUSE_MENU` (Escape) memiliki status khusus:

- Default key `KEY_ESCAPE` di-set di `keybindManager`, tapi logika pause di `main.cpp` dan `screen_handler.cpp` tidak selalu membaca dari `keybindManager`.
- Escape juga berfungsi sebagai cancel key di rebind listener (`keybindsTab.cpp` baris 92: `if (key == KEY_ESCAPE) listeningAction = -1`), sehingga merubah bind Escape bisa menyebabkan konflik.
- Untuk alasan ini, PAUSE_MENU tidak disarankan untuk di-rebind dan saat ini tidak ada di UI settings.

## File Source Terkait

| File | Peran |
| --- | --- |
| `include/systems/keybindManager.h` | Definisi enum `Action` (REVIVE = 15, TEST_LOSE_HP = 16, ..., DEBUG_TOGGLE_PLAYER = 21) dan class KeybindManager |
| `src/systems/keybindManager.cpp` | Default bindings (`InitDefaults()`), action names, JSON persistence ke `saves/settings/keybindsTab.json` |
| `src/ui/keybindsTab.cpp` | UI settings keybinds -- struct `sections[]` hanya mencakup action 0-14, debug action 15-21 sengaja tidak diikutsertakan |
| `src/core/screen_handler.cpp` | Pengecekan input debug toggle di game loop |
| `src/core/main.cpp` | Pengecekan PAUSE_MENU dan debug input di main loop |

## Catatan

- Semua debug keybind menggunakan **keyboard key**, tidak ada yang menggunakan mouse button.
- Keybind disimpan ke `saves/settings/keybindsTab.json` bersama keybind normal lainnya. Mengedit file JSON secara manual bisa mengubah binding debug action, meskipun tidak ada di UI.
- Untuk mereset semua keybind (termasuk debug) ke default, bisa menggunakan opsi Reset Defaults di UI Settings > Keybinds.
