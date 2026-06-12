# Projectile — #16 Animasi Proyektil Hancur

## #16 — Animasi Proyektil Hancur Kalo Gak Kena Target

### Masalah
Sekarang pas projectile (`Arrow`) gak kena target atau nabrak halangan, langsung `IsActive = false` tanpa visual apapun — langsung disappear.

Kodenya di `src/systems/combat.cpp:707-759` (`Arrow::Update()`):

```cpp
// Semua kondisi ini bikin arrow langsung ilang:
if (LifeTime >= MaxLifeTime) { IsActive = false; return; }    // batas waktu
if (jarak >= Reach)          { IsActive = false; return; }    // batas jangkauan
if (nabrak collision)        { IsActive = false; return; }    // tembok
if (nabrak DynamicObstacles) { IsActive = false; return; }    // prop
if (nabrak entity)           { IsActive = false; return; }    // kena target
```

### Expected
Pas projectile mau ilang (kena apa pun), muncul animasi "hancur" / impact effect di posisi terakhir, baru setelah animasi selesai baru dihapus.

### Rencana Implementasi
**Opsi A — Pake explosion animation yang udah ada**
Udah ada animasi `explosion` di `assets/data/animations.json:236`:
- 10 frame: `explosion1` — `explosion10`
- Durasi per frame: `0.06s`
- Non-looping
- Tinggal dipanggil di posisi arrow pas `IsActive` mau di-set false

**Opsi B — Bikin animasi impact baru**
Temen lo bikin sprite impact baru (misal `impact1` — `impact5`), daftarin di `animations.json`, panggil dari `Arrow::Update()`.

### Cara Kerja
Di `Arrow::Update()`, daripada langsung `IsActive = false`, ganti jadi:
1. Set state baru (misal `IsDying = true`) + simpen posisi
2. Render animasi impact/explosion di posisi itu
3. Begitu animasi selesai, baru `IsActive = false`

Atau alternatif: spawn entitas effect sementara via `Entities::AddDynamic()` — misal `ImpactEffect` yang durasinya pendek, render explosion sprite, auto-hapus.

### File yang relevan
| File | Keterangan |
|------|-----------|
| `src/systems/combat.cpp` | `Arrow::Update()` — tambah logika impact |
| `src/systems/combat.cpp` | `Arrow::Render()` — render impact kalo `IsDying` |
| `include/systems/combat.h` | `Arrow` class — mungkin tambah field `IsDying`, `DeathTimer` |
| `assets/data/animations.json` | `explosion` entry — udah ada, tinggal pake |

### Catatan
- Buat temen lo yang ngerjain ini
- Kalo pake explosion yang udah ada, gak perlu asset baru
