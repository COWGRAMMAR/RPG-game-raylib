# Font Audit Plan — Cek Semua Font di Game

> **Referensi data font**: `plans/font-system-rework.md` — berisi inventory lengkap tiap file, jumlah call, isi pesan, size, warna.

---

## 1. Ruang Lingkup Audit

Audit ini mencakup **semua text yang dirender di game** — baik yang udah pake font system baru (`GetOrLoad` / `DrawDefaultText`) maupun yang masih pake raw `DrawText` / `GetFontDefault()`.

### Yang di-audit:
- **82 calls** `GetOrLoad(FontId::...)` — 9 files (udah migrasi)
- **48 calls** `DrawDefaultText(...)` — 6 files (udah migrasi)
- **button.h constructor** — default font saat button tanpa explicit font
- **Raw DrawText** yang mungkin terlewat — cari sisa-sisa yang belum pake font system
- **Semua font assets** — 11 file .ttf/.otf, validasi path dan loading

---

## 2. Steps Audit

### Step 1: Scan Semua DrawText / GetFontDefault / LoadFontEx
- Cari semua `DrawText(` calls (raw, bukan DrawDefaultText)
- Cari semua `GetFontDefault()` — pastikan gak ada yang masih pake
- Cari semua `LoadFontEx(` — validasi atlas resolution

### Step 2: Verifikasi tiap file yang udah migrasi
- **9 file GetOrLoad**: cocokin dengan tabel di font-system-rework.md
- **6 file DrawDefaultText**: pastikan font size & color sesuai konteks
- Cek konsistensi FontId per konteks (apakah pilihan font udah tepat?)

### Step 3: Cek Font Assets
- 11 font file ada semua? Path bener?
- Atlas resolution (RES_256) cukup untuk ukuran font yang dipake?
- Font name internal (font.fontName) sesuai?

### Step 4: Validasi Visual
- Render test: tiap konteks text tampil dengan font yang benar
- Size konsisten antar konteks sejenis
- Gak ada clipping / overflow

### Step 5: button.h Audit
- Semua button object: apakah ada yang perlu explicit font override?
- Default font button cukup Poppins-Bold?

---

## 3. Checklist Audit per File

### Hud (27 GetOrLoad + 2 DrawDefaultText)
- [ ] Item name / stack amount — font size sesuai?
- [ ] Keybind hints — hintFontSize(25/22) konsisten?
- [ ] Loot popup — font size 16 pas?
- [ ] DrawDefaultText: loot popup item name & "[Klik kiri] untuk tutup"

### KeybindsTab (19 calls)
- [ ] Header (KEYBIND_HEADER / NewDawn) — size 32 pas?
- [ ] Entry (KEYBIND_ENTRY / Poppins-Regular) — size 28 pas?
- [ ] Toast/pesan (DEFAULT) — size 30 pas?

### Popup (16 GetOrLoad)
- [ ] Dialog text & subMessage — fontSize(30) konsisten?
- [ ] OK/Cancel label — fontSize(30)

### SaveLoadScreen (10 calls)
- [ ] Header "MANUAL SAVE" / "AUTO SAVE" — size 22?
- [ ] Entry text (KEYBIND_ENTRY) — size 14-20 variasi sesuai?
- [ ] Header area — headerFontSize(28) konsisten?

### LoadingScreen (6 GetOrLoad)
- [ ] "Loading..." — size 32
- [ ] Progress "%" — size 20
- [ ] Nama map — size 18

### PauseMenu (4 GetOrLoad)
- [ ] ON/OFF toggle — labelFontSize(24)?
- [ ] "Reset Tab" / "Reset All" — size 20?

### AudioTab (5 GetOrLoad)
- [ ] Label volume — FONT_SIZE(30) konsisten?
- [ ] Value persen — FONT_SIZE(30) dengan warna BLACK?

### Item (2 GetOrLoad)
- [ ] Stack count — size 14?

### VideoTab (2 GetOrLoad)
- [ ] "Fullscreen" / "Show FPS" — fontSize(34)?

### DebugMode (26 DrawDefaultText)
- [ ] Semua debug info — size 10-18 sesuai?
- [ ] Warna sesuai konteks (RED/PURPLE/MAGENTA/dll)?

### CombatTurn (14 DrawDefaultText)
- [ ] "MENANG" size 80 — atlas RES_256 cukup?
- [ ] HP/MP text — size 16?
- [ ] Action button label — size 20?

### Effects (3 DrawDefaultText)
- [ ] Damage number — size 10 kecil, masih terbaca?
- [ ] Shadow & foreground alpha correct?

### VideoScreen (2 DrawDefaultText)
- [ ] Loading text — size 20?
- [ ] "Tekan SPACE untuk skip" — size 20?

### ScreenHandler (1 DrawDefaultText)
- [ ] FPS counter — size 20 warna GREEN

### Button.h
- [ ] Default constructor font — sudah pake font system?
- [ ] Button object tanpa explicit font — pake DEFAULT?

---

## 4. Temuan Potensial & Risiko

| Area | Risiko | Severity |
|------|--------|----------|
| Combat "MENANG" size 80 di atlas RES_256 | Mungkin perlu RES_512 | Medium |
| Damage number size 10 | Kecil banget, mungkin gak kebaca | Low |
| DrawDefaultText di hud.cpp (2 calls) | Udah migrasi tapi font size beda | Info |
| Font assets 11 file | Path hardcoded? Butuh verifikasi | Low |

---

## 5. Deliverable Audit

Setelah audit selesai, hasilnya:
1. **Laporan temuan** — daftar inkonsistensi font / size / color
2. **Rekomendasi** — FontId mana yang perlu diganti untuk konteks tertentu
3. **Update font-system-rework.md** — sync status terbaru
4. **Patch/PR** — perbaikan temuan (kalo ada)
