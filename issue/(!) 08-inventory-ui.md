# Issue 08 — UI Inventory (#15)

## Ringkasan
Redesign legend & feedback visual pada inventory. Drop teks instruksi drag & drop yang sekarang, ganti dengan dynamic keybind text + visual feedback pas drop item.

---

## 1. Legend Saat Ini
**File**: `src/rendering/hud.cpp:553-572`

Sekarang ada 4 hint di kanan atas inventory:
```
1. [Left-Click Drag] Arrange    → 🗑️ DROP
2. [Ctrl+Click] Merge           → ✅ TETAP, dynamic keybind
3. [Right-Click Drag] Split     → ✅ TETAP, dynamic keybind
4. [Drop Outside Menu] Drop     → 🗑️ DROP
```

## 2. Legend Baru
- **Merge** & **Split** → pake dynamic keybind text (sama kayak #9 HUD redesign)
- **Merge & Split ditaruh di bawah legenda** (bukan di kanan atas)
- **Teks "Press I to Close"** → jadi dynamic keybind juga, font dibesarin, mungkin ganti ke `fontKeybindEntry`

### Dynamic Keybind Text
Teks otomatis ngikut keybind yang ke-assign. Misal Merge di-remap ke `[Space]`, teks berubah. Pattern sama kayak di HUD redesign (#9).

## 3. Drop Visual Feedback (Ganti Teks)
Teks instruksi Arrange & Drop di-drop. Ganti dengan visual feedback:

### Saat drag item (ghost):
- **Normal drag** (cursor di atas grid): GOLD tint di slot sumber, ghost item putih biasa
- **Cursor di luar grid** (drop zone):
  - Ghost item 36×36 box: **red background fill** (`ColorAlpha(RED, ~0.25f)`) + **red outline** (`DrawRectangleLines`)
  - Sprite item di atas background merah (tetep putih atau tint merah tipis)
  - Slot sumber di grid: **RED tint** (`ColorAlpha(RED, 0.25f)`) + **red outline** ganti dari GOLD

### Alasan:
Player langsung lihat perubahan visual tanpa perlu baca teks. Intuitif.

### Raylib Implementation Notes:
- **Tint sprite**: Line 103 `hud.cpp` — `DrawTexturePro(..., WHITE)` → ganti `WHITE` jadi `RED` / semi-red
- **Background fill ghost**: Draw rectangle di ghost box 36×36 sebelum sprite
- **Outline**: `DrawRectangleLines()` di bounding box ghost item
- **Slot tint**: Line 425 `hud.cpp` — `ColorAlpha(GOLD, 0.25f)` → ganti jadi `ColorAlpha(RED, 0.25f)` pas kursor di luar grid

### Deteksi Drop Zone:
Drop zone = **seluruh area di luar inventory grid**. Bedanya sama Arrange/Drop skrg cuma soal visual — logika drop tetep sama (line 513 `hud.cpp`).

## 4. Item Terkait
- **#9** — HUD icon redesign (pake dynamic keybind pattern yang sama)
- **#12** — Guard 1 keybind = 1 action (backend untuk dynamic keybind)
- **#14** — Keybind group mapping (Merge & Split di grup Inventory)

## 5. Files Affected
- `src/rendering/hud.cpp` — visual feedback, legend rendering, ghost item
