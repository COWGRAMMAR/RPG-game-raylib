# Instruksi Build

## Pengaturan Pengembangan

### Alat yang Diperlukan

Pasang alat-alat berikut untuk membangun proyek:

- **Windows**: Untuk kemudahan menggunakan dan mengunduh alat-alat, gunakan [scoop](https://scoop.sh/), lalu ikuti perintah setup yang ada pada halaman. Selebihnya, mohon untuk menggunakan PowerShell (5.1+) untuk memaksimalkan kemudahan.

| Alat | Windows (scoop) | macOS (brew) | Linux (apt) |
| ------------ | ----------------- | -------------- | ------------- |
| **Compiler (gcc)** | `scoop install gcc` atau `scoop install mingw-mstorsjo-llvm-ucrt` (Clang) | `brew install gcc` | `apt install gcc` |
| **CMake** | `scoop install cmake` | `brew install cmake` | `apt install cmake` |
| **Ninja** | `scoop install ninja` | `brew install ninja` | `apt install ninja-build` |
| **ccache** | `scoop install ccache` | `brew install ccache` | `apt install ccache` |

### Pengaturan Pertama

1. Pasang semua alat yang diperlukan (lihat tabel di atas)
2. Jalankan skrip setup untuk mengunduh dependensi:

   **Windows (PowerShell):**

   ```powershell
   .\setup.ps1
   ```

   **Linux/macOS (Bash):**

   ```bash
   bash setup.sh
   ```

> **PERHATIAN**: Dukungan untuk sistem Unix (Linux/macOS) bersifat eksperimental dan memerlukan pengujian lebih lanjut. Skrip `setup.sh` telah disediakan namun mungkin mengalami kendala pada beberapa distribusi.

## Membangun

### Mulai Cepat

```bash
# Configure (sekali saja, atau setelah menambah file baru)
cmake --preset ninja

# Build
cmake --build --preset ninja
```

File executable akan berada di `build/bin/main.exe`.

#### Jalankan Program

```bash
# Linux/macOS
./build/bin/main

# Windows (CMD)
.\build\bin\main.exe

# Windows (PowerShell)
.\build\bin\main.exe
```

### Preset Build

| Preset | Deskripsi |
| -------- | ----------- |
| `ninja` | Build release dengan optimasi (default) |
| `ninja-debug` | Build debug dengan simbol |

### Build Manual (tanpa preset)

```bash
cmake -B build -G Ninja
cmake --build build --parallel
```

### Build Bersih

```bash
# Linux/macOS
rm -rf build

# Windows (PowerShell)
Remove-Item -Recurse -Force build

# Windows (CMD)
rmdir /s /q build

# Kemudian build ulang
cmake --preset ninja
cmake --build --preset ninja
```

### Referensi Perintah

| Aksi | Linux/macOS | PowerShell | CMD |
| ------ | ----------- | ------------ | ----- |
| **Clean** | `rm -rf build` | `Remove-Item -Recurse -Force build` | `rmdir /s /q build` |

## Menambahkan File Sumber Baru

File `.cpp` baru di `src/` akan otomatis ditemukan pada saat CMake berjalan ulang. Tidak perlu perubahan manual.

## Pemecahan Masalah

- **Error "No such file or directory"**: Jalankan `.\setup.ps1` (Windows) atau `bash setup.sh` (Linux/macOS) untuk mengunduh dependensi
- **Build error setelah menambah file**: Jalankan `cmake --preset ninja` untuk mengkonfigurasi ulang
- **Game crash**: Pastikan untuk menjalankan file dari root directory, dan tidak mengklik file executable secara langsung karena program tidak bisa mencari file yang dibutuhkan. Apabila error masih terjadi, jalankan build process ulang atau lakukan `git fetch --origin && git pull --ff` untuk mendapatkan versi terbaru.

> **CATATAN**: Jika mengalami kendala pada sistem Linux/macOS, periksa apakah semua alat (gcc, cmake, ninja) telah terpasang dengan benar. Skrip `setup.sh` masih bersifat eksperimental.
