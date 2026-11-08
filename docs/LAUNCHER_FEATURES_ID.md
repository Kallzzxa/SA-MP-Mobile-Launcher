# HopePride — Fitur Launcher (status nyata)

Status dibagi tiga: **Didukung** (berfungsi sekarang), **Sebagian** (berfungsi tetapi ada batas nyata),
dan **Belum didukung** (tidak ditampilkan sebagai aktif).

## Pengelola Mod — Sebagian

Layar: `Pengaturan → PENGELOLA MOD`

| Fitur | Status | Catatan |
|---|---|---|
| Impor dari penyimpanan (SAF, multi-berkas) | Didukung | `ActionResult` per berkas dilaporkan |
| Daftar mod + ukuran + kategori | Didukung | kategori dari ekstensi (dff/txd/col/ipl → Model dan peta, dst.) |
| Aktif / nonaktif | Didukung | memindahkan berkas antara `mods/` dan `mods_disabled/` |
| Hapus dengan konfirmasi | Didukung | konfirmasi menyebut nama + ukuran |
| Validasi berkas | Didukung | 0 byte, tanpa ekstensi, `.exe/.dll`, arsip `.zip/.rar/.7z` ditandai bermasalah |
| Deteksi duplikat | Didukung | SHA-256 untuk berkas ≤ 96 MB, selain itu dibandingkan ukuran |
| Deteksi bentrok nama | Didukung | akhiran ` (2)`, ` (3)`, … |
| Pencarian, filter kategori | Didukung | — |
| Riwayat pemasangan | Didukung | 120 entri terakhir di SharedPreferences |
| **Pemuatan mod ke dalam game** | **Belum didukung** | engine tidak punya jalur resmi untuk model/tekstur kustom dari launcher |

## Pusat Crash — Sebagian

Layar: `Pengaturan → PUSAT CRASH`

| Fitur | Status | Catatan |
|---|---|---|
| Membaca `crash_native.txt`, `samp_log.txt`, log diagnostik | Didukung | daftar berkas dengan ukuran + waktu |
| Parser sinyal | Didukung | SIGSEGV/SIGABRT/SIGBUS/SIGILL/SIGFPE |
| Alamat fault dan simbol PC/LR | Didukung | ditampilkan apa adanya |
| Pola `SortEntries`, `RLEDecompress`, `variant-fallback=1`, `load-failure=1` | Didukung | ditulis sebagai temuan, bukan kesimpulan |
| Daftar `UI-UNTRANSLATED:` | Didukung | sampai 40 entri unik |
| Salin / bagikan laporan | Didukung | teks biasa |
| Keluar dari Safe Mode | Didukung | menghapus penanda `safe_mode.on` |
| Membaca logcat | Belum didukung | Android 11+ membatasi akses logcat aplikasi lain tanpa izin khusus |

## Fitur lain yang diminta

| Fitur | Status | Alasan |
|---|---|---|
| Server manager (favorit, riwayat, quick connect) | Sudah ada di launcher | dipertahankan, tidak diubah |
| Download manager, profil, theme studio, file manager | Belum dikerjakan | belum ada di putaran ini |
| Keyboard/mouse, HUD editor, graphics settings | Belum didukung | butuh simbol `libGTASA`; berkas di repo masih LFS pointer |
| CLEO / MonetLoader | Belum didukung | library tidak tersedia; tidak dibuat tiruan |
