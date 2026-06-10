# Audio System — #10 #11 #13

## #10 — Volume Fallback di Run Ke-2

### Masalah
Di startup, urutan inisialisasi salah:

```cpp
// main.cpp
LoadAudioSettings();     // (1) baca saved settings → misal 50,50,50
AudioManager::Init();    // (2) OVERWRITE: master=100, music=80, sfx=100
AudioManager::LoadAudioAssets();
```

`LoadAudioSettings()` baca file `saves/settings/audioTab.json` & set volume, tapi baris berikutnya `AudioManager::Init()` ngereset semua ke hardcode default (`1.0f, 0.8f, 1.0f`). Efek: saved settings gak pernah kepake, volume balik ke default tiap game restart.

### Expected
Saved settings dipake sesuai yang terakhir disimpan player.

### Fix
Balik urutan — `Init()` dulu, baru `LoadAudioSettings()`:

```cpp
AudioManager::Init();              // init state dulu
AudioManager::LoadAudioAssets();   // load music
LoadAudioSettings();               // apply saved settings (overwrite default kalo ada)
```

### File yang diubah
- `src/core/main.cpp` — tukar urutan baris 131 dan 134
- `src/systems/audioManager.cpp` — `_musicVolume = 0.8f` → `1.0f`
- `src/ui/audioTab.cpp` — `g_sliders = {100, 80, 100, ...}` → `{100, 100, 100, ...}`

### Status:  Fixed di commit `fix audio #10 #11`
- `LoadAudioSettings()` dipanggil setelah `AudioManager::Init()` + `LoadAudioAssets()`

---

## #11 — Reset Music Default ke 80% Bukan 100%

### Masalah
Hardcode `_musicVolume = 0.8f` di `AudioManager::Init()` sementara `_masterVolume` dan `_sfxVolume` pake `1.0f`. Gak konsisten.

Juga `g_sliders` di `audioTab.cpp`:
```cpp
SliderState g_sliders = {100, 80, 100, false, -1};
//                        ^     ^    ^
//                      master music sfx
```

Music default 80%, sisanya 100%.

### Expected
Semua default volume 100%.

### Fix
- `audioManager.cpp`: ganti `_musicVolume = 0.8f` → `1.0f`
- `audioTab.cpp`: ganti `g_sliders = {100, 80, 100, ...}` → `{100, 100, 100, ...}`

### File yang diubah
| File | Baris | Perubahan |
|------|-------|-----------|
| `src/systems/audioManager.cpp` | Init() | `_musicVolume = 0.8f` → `1.0f` |
| `src/ui/audioTab.cpp` | deklarasi `g_sliders` | `{100, 80, 100, ...}` → `{100, 100, 100, ...}` |

---

## #13 — UI Audio Tab Dipercantik

### Masalah
Tampilan audio tab masih polos — slider tanpa visual knob, font standar, warna default.

### Rencana
1. **Texture knob** di ujung slider — asset dari user/temen
2. **Ganti font** — pake font yang udah siap di sistem
3. **Ganti warna slider** — sesuai tema

### Font yang udah siap
| Variable | Font | File |
|----------|------|------|
| `fontKeybindEntry` | Poppins-Regular | `include/rendering/fonts.h` |
| `fontKeybindHeader` | NewDawn | `include/rendering/fonts.h` |
| `fontLoadingTitle` | Poppins-Bold | `include/rendering/fonts.h` |

### Catatan
- Texture knob: **user nyari sendiri** — typenya gambar kecil (16×16 atau 32×32) buat digambar di atas slider thumb
- Implementasi terpisah setelah asset siap

### File yang mungkin diubah
- `src/ui/audioTab.cpp` — render slider, font, warna
