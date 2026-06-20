# Instruksi Build

> **PowerShell 5.1+ required for Windows. CMD is lacking features for the best build experience.**

---

## 1. First-Time Setup

> Langkah satu kali — selesaikan ini sebelum build pertama.

### Alat yang Diperlukan

Pasang alat-alat berikut untuk membangun proyek:

- **Windows**: Untuk kemudahan menggunakan dan mengunduh alat-alat, gunakan [scoop](https://scoop.sh/), lalu ikuti perintah setup yang ada pada halaman. Selebihnya, mohon untuk menggunakan PowerShell (5.1+) untuk memaksimalkan kemudahan.

| Alat | Windows (scoop) | macOS (brew) | Linux (apt) |
| --- | --- | --- | --- |
| **Compiler (gcc)** | `scoop install gcc` atau `scoop install mingw-mstorsjo-llvm-ucrt` (Clang) | Xcode CLT (Apple Clang, otomatis terdeteksi CMake) atau `brew install gcc` sebagai opsi | `apt install gcc` |
| **CMake** | `scoop install cmake` | `brew install cmake` | `apt install cmake` |
| **Ninja** | `scoop install ninja` | `brew install ninja` | `apt install ninja-build` |
| **ccache** | `scoop install ccache` | `brew install ccache` | `apt install ccache` |
| **FFmpeg** | (bundel melalui `setup.ps1`) | `brew install ffmpeg` | `apt install libavcodec-dev libavformat-dev libavutil-dev libswresample-dev libswscale-dev` |

### Pengaturan Pertama

1. Pasang semua alat yang diperlukan (lihat tabel di atas)
2. Jalankan skrip setup untuk mengunduh dependensi:

   #### Windows (PowerShell)

   ```powershell
   .\setup.ps1
   ```

   #### Linux/macOS (Bash)

   ```bash
   bash setup.sh
   ```

> **CATATAN**: Dukungan untuk sistem Unix (Linux/macOS) telah diuji pada Ubuntu, Debian, dan macOS. Skrip `setup.sh` telah disediakan dan `run-linux.sh` tersedia untuk menjalankan permainan.

---

## 2. Building & Running

### 2a. One-Click Run (recommended for first-timers)

Skrip ini mengkonfigurasi, membangun, dan menjalankan permainan secara otomatis.

#### Windows (PowerShell 5.1+. 7+ Recommended)

```powershell
.\run-windows.ps1
```

Atau gunakan `.\run-windows.bat` jika terdapat kebijakan eksekusi PowerShell yang membatasi.

#### Linux/macOS (Bash)

```bash
bash run-linux.sh
```

### 2b. Manual Build

Untuk pengguna yang ingin kontrol lebih atau perlu debug build.

#### Configure (sekali saja, atau setelah menambah file baru)

```bash
cmake --preset ninja       # release build (default)
cmake --preset ninja-debug # debug build dengan simbol
```

#### Build

```bash
cmake --build --preset ninja
```

#### Run

```bash
# Linux/macOS
./build/bin/main

# Windows (PowerShell 5.1+. 7+ Recommended)
.\build\bin\main.exe
```

Atau gunakan `.\run-windows.ps1` (Windows) / `bash run-linux.sh` (Linux/macOS) untuk menjalankan tanpa build ulang.

Untuk pengguna Windows, `Breach&Loot.bat` juga tersedia sebagai peluncur cepat.

### 2c. Build Presets Reference

| Preset | Deskripsi |
| --- | --- |
| `ninja` | Build release dengan optimasi (default) |
| `ninja-debug` | Build debug dengan simbol |

### 2d. Manual Build Without Presets (opsional)

```bash
cmake -B build -G Ninja
cmake --build build --parallel
```

---

## 3. Maintenance

### 3a. Clean Build

Hapus direktori build lalu build ulang dari awal.

```bash
# Linux/macOS
rm -rf build

# Windows (PowerShell)
Remove-Item -Recurse -Force build

# Kemudian build ulang
cmake --preset ninja
cmake --build --preset ninja
```

### 3b. Adding New Source Files

File `.cpp` baru di `src/` akan otomatis ditemukan pada saat CMake berjalan ulang. Tidak perlu perubahan manual.

---

## 4. Troubleshooting

- **Error "No such file or directory"**: Jalankan `.\setup.ps1` (Windows) atau `bash setup.sh` (Linux/macOS) untuk mengunduh dependensi
- **Build error setelah menambah file**: Jalankan `cmake --preset ninja` untuk mengkonfigurasi ulang
- **Game crash**: Pastikan untuk menjalankan file dari root directory, dan tidak mengklik file executable secara langsung karena program tidak bisa mencari file yang dibutuhkan. Apabila error masih terjadi, jalankan build process ulang atau lakukan `git fetch --origin && git pull --ff` untuk mendapatkan versi terbaru.
- **`run-windows.ps1` tidak bisa dijalankan**: Jalankan PowerShell sebagai administrator, lalu `Set-ExecutionPolicy RemoteSigned -Scope CurrentUser`
- **`run-linux.sh` tidak bisa dijalankan**: Jalankan `chmod +x run-linux.sh` lalu coba lagi

> **CATATAN**: Jika mengalami kendala pada sistem Linux/macOS, periksa apakah semua alat (gcc, cmake, ninja) telah terpasang dengan benar. Skrip `setup.sh` menangani sebagian besar dependensi secara otomatis.
