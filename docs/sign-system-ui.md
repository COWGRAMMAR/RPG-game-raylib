# Sign System — Panduan UI Designer

## Cara Pasang Sign di Tiled

1. Buat **object** di layer `"object"` (sama kayak chest, crate, dll)
2. Set **type** → `"sign"`
3. Set **name** → terserah (contoh: `"sign_intro"`, `"sign_warning"`)
4. Tambah **custom property**:
   - **Name**: `dialog`
   - **Type**: `string`
   - **Value**: teks yang mau ditampilkan

**Contoh properti dialog (pake `|` buat baris baru):**

```txt
Halo adventurer!|Selamat datang di dungeon.|Waspada dengan monster di depan!
```

Hasilnya bakal tampil sebagai 3 baris:

```txt
Halo adventurer!
Selamat datang di dungeon.
Waspada dengan monster di depan!
```

Kalo gamau pake `|`, bisa pake Enter langsung di Tiled (multiline) — nanti otomatis split per baris.

---

## Cara Kerja Sign

1. Player arahkan mouse ke sign (raycast) → muncul prompt interaksi
2. Player tekan **E** → dialog terbuka
3. Layar game nge-freeze total (kayak pause), muncul overlay:
   - **Dialog box** — render dengan texture `assets/textures/dialogBox.png` (posisi horizontal tengah, centered di 75% tinggi layar (texY = GameScreenHeight * 0.6f + GameScreenHeight * 0.15f - dialogTex.height / 2))
   - **Text** — baris-baris dialog, font **`HUD_PLAYER`** (Poppins-Bold, 22px, hitam), word-wrap otomatis dalam area konten texture (dikurangi 83px dekorasi kanan)
   - **Hint** — `"[Left-Click] To Close"` (font 16px, hitam)
   - **Tidak ada screen dim** — game terlihat di belakang texture transparan
4. Player klik **kiri di mana aja** → dialog tutup, game lanjut

> **Fallback**: Jika `dialogBox.png` gagal di-load (`dialogTex.id == 0`), system fallback ke:
> - Screen dim: `ColorAlpha(BLACK, 0.4f)` full screen
> - Dialog box: `DrawRectangleRounded()` — DARKGRAY 95% alpha, border WHITE
> - Text: FontId::DEFAULT (Poppins-Bold), 16px, WHITE
> - Hint: `[Left-Click] To Close` (16px, GRAY)

---

## Yang Perlu Didesign (Placeholder Saat Ini)

### 1. Screen Dim

**Primary path** (texture `dialogBox.png` berhasil di-load):
- **Tidak ada screen dim.** Dialog hanya menampilkan texture di atas gameplay.

**Fallback** (texture gagal di-load):
- `ColorAlpha(BLACK, 0.4f)` — 40% hitam full screen
- Screen dim hanya muncul saat texture tidak tersedia.

| Bisa diubah | Keterangan |
| --- | --- |
| Warna | Ganti dari hitam ke warna lain |
| Intensitas | 0.0 — 1.0 |
| Efek | Bisa pake blur atau gradient |

### 2. Dialog Box

**Primary path** (texture berhasil di-load):
- Texture `assets/textures/dialogBox.png` (file-static `dialogTex`)
- Posisi: tengah layar (horizontal & vertical centering)
- Tidak ada rounded rect, border, atau background color — semua visual ada di texture.

**Fallback** (texture gagal di-load):
- Rectangle rounded, `DARKGRAY` 95% alpha, border putih

| Properti | Nilai Sekarang | Keterangan |
| --- | --- | --- |
| Posisi X | `GameScreenWidth * 0.1` | 10% margin kiri |
| Posisi Y | `GameScreenHeight * 0.6` | 60% dari atas |
| Lebar | `GameScreenWidth * 0.8` | 80% lebar layar |
| Tinggi | `GameScreenHeight * 0.3` | 30% tinggi layar |
| Corner radius | `0.15f` | 15% rounded |
| Background | `DARKGRAY` 95% alpha | |
| Border | `WHITE` | |

Bisa diganti: ukuran, posisi, warna, border, background texture, font, dll.

### 3. Teks Dialog

Sekarang: font `HUD_PLAYER` (Poppins-Bold), size 22, hitam, dengan texture `dialogBox.png`. Word-wrap otomatis di dalam area konten texture (dikurangi 83px dekorasi kanan).

| Properti | Nilai Sekarang |
| --- | --- |
| Font | `HUD_PLAYER` (`GetOrLoad(FontId::HUD_PLAYER)`) |
| Size | `SIGN_DIALOG_FONT_SIZE = 22` |
| Color | BLACK |
| Texture | `dialogBox.png` (file-static `dialogTex`) — posisi tengah layar |
| Line spacing | 30 px |
| Font size close hint | `CLOSE_HINT_FONT_SIZE = 16` |

### 4. Hint Dismiss

**Primary path**: text `[Left-Click] To Close` (hitam, 16px) di pojok kanan bawah texture, dihitung dari `contentRight - 150`.

**Fallback**: text `[Left-Click] To Close` (GRAY, 16px) di pojok kanan bawah box.

---

## API yang Tersedia buat UI

Semua lewat `signManager` (global):

```cpp
// Cek apakah dialog lagi aktif
signManager.IsDialogActive()            → bool

// Ambil baris-baris dialog (udah di-split)
signManager.GetActiveDialogLines()      → const std::vector<std::string>&

// Tutup dialog (dipanggil pas klik kiri)
signManager.DismissDialog()
```

**Catatan:** Dismiss dialog udah di-handle otomatis di `main.cpp`. UI designer gak perlu mikirin logika dismiss — cukup render aja pas `IsDialogActive() == true`.

---

## File yang Relevan

| File | Fungsinya |
| --- | --- |
| `include/rendering/hud.h` | Deklarasi `DrawSignDialog()` |
| `src/rendering/hud.cpp` | Implementasi render dialog (yang bakal diedit UI designer) |
| `src/core/screen_handler.cpp` | Panggil `DrawSignDialog()` di `DrawUIOverlay()` |
| `src/core/main.cpp` | Logika dismiss + freeze game logic |
| `include/map/propsbehavior.h` | Deklarasi class `SignManager` + `extern SignManager signManager` |
| `src/map/propsbehavior.cpp` | Definisi `signManager` global + implementasi method (`SpawnSigns`, `Render`, `Interact`, `DismissDialog`, dll) |
| `src/systems/interaction.cpp` | Panggil `signManager.Interact()` saat player interaksi |
| `src/entities/entities.cpp` | Panggil `signManager.Render()` dan `signManager.Clear()` |
