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
   - **Screen dim** — semi-transparent black
   - **Dialog box** — rounded rectangle di bagian bawah layar
   - **Text** — baris-baris dialog
   - **Hint** — "[Klik kiri] untuk tutup"
4. Player klik **kiri di mana aja** → dialog tutup, game lanjut

---

## Yang Perlu Didesign (Placeholder Saat Ini)

### 1. Screen Dim

Sekarang: `ColorAlpha(BLACK, 0.4f)` — 40% hitam full screen

| Bisa diubah | Keterangan |
| --- | --- |
| Warna | Ganti dari hitam ke warna lain |
| Intensitas | 0.0 — 1.0 |
| Efek | Bisa pake blur atau gradient |

### 2. Dialog Box

Sekarang: rectangle rounded, `DARKGRAY` 95% alpha, border putih

| Properti | Nilai Sekarang | Keterangan |
| --- | --- |
| Posisi X | `GameScreenWidth * 0.1` | 10% margin kiri |
| Posisi Y | `GameScreenHeight * 0.6` | 60% dari atas |
| Lebar | `GameScreenWidth * 0.8` | 80% lebar layar |
| Tinggi | `GameScreenHeight * 0.3` | 30% tinggi layar |
| Corner radius | `0.15f` | 15% rounded |
| Background | `DARKGRAY` 95% alpha | |
| Border | `WHITE` | |

Bisa diganti: ukuran, posisi, warna, border, background texture, font, dll.

### 3. Teks Dialog

Sekarang: font default raylib, size 16, putih, pake loop per baris

| Properti | Nilai Sekarang |
| --- | --- |
| Font | Default raylib |
| Size | 16 |
| Color | WHITE |
| Padding kiri | 16 px dari border box |
| Padding atas | 16 px dari border box |
| Line spacing | 22 px |

### 4. Hint Dismiss

Sekarang: text `[Klik kiri] untuk tutup` di pojok kanan bawah box

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
