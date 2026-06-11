# Input — #12 Guard 1 keybind = 1 action

## Masalah
Saat ini 1 tombol keyboard bisa dipake untuk beberapa action berbeda.
Contoh: player set keybind `move_up = W` dan `interact = W` → player bisa maju sekaligus interact secara bersamaan.

Expected: 1 keybind hanya boleh terdaftar untuk 1 action.

## Fix Approach
**Hybrid: auto-swap + info toast.**

Flow:
1. User set key `X` ke action `A`
2. Cek apakah `X` sudah dipake action `B`:
   - Iya → auto-swap: action `A` dapet `X`, action `B` dapet key lama action `A`
   - Tidak → langsung assign
3. Tampilkan info toast di UI (bukan blocking popup):
   `"X dipindah dari B ke A. B sekarang: [key_lama_A]"`
4. Toast ilang otomatis setelah ~2 detik

## Kenapa bukan confirmation popup
Codebase belum punya sistem modal dialog. Bikin Yes/No popup cuma buat 1 use case = overkill.

## File yang terlibat
- `src/ui/keybindsTab.cpp` — logika rebind
- `include/input/keybindManager.h` / `src/input/keybindManager.cpp` — method `FindActionByKeycode()` (baru), swap logic

### Status: Fixed di commit sebelumnya
- Swap logic + toast notification otomatis tiap rebind
