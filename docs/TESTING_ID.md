# HopePride — Status Pengujian

Dokumen ini memisahkan dua hal yang sering dicampur: **pengujian otomatis yang benar-benar
dijalankan** dan **pengujian perangkat yang belum dijalankan**. Tidak ada status "FIXED" di sini
untuk sesuatu yang belum diuji di perangkat.

## 1. Pengujian otomatis (dijalankan di CI, `tools/hosttest/run.sh`)

Skrip menyalin `Localization.cpp`, `Localization.h`, `SafeMode.h` dari source saat ini, menyusunnya
dengan stub kecil (`stubs/main.h`, `stubs/vendor/shadowhook/patch.h`), lalu menjalankan asersi.

| # | Yang diuji | Expected | Hasil |
|---|---|---|---|
| A1 | Label menu Rusia `Возобновить` | diterjemahkan | PASS |
| A2 | Hasil terjemahan | `Lanjutkan` | PASS |
| A3 | Pencocokan huruf kecil | tetap cocok | PASS |
| A4 | Pencocokan huruf besar | tetap cocok | PASS |
| A5 | `Назад` | `Kembali` | PASS |
| A6 | Baris chat pemain | tidak diubah | PASS |
| A7 | Pesan server | tidak diubah | PASS |
| A8 | String kosong | tidak diubah | PASS |
| A9 | Teks Rusia tak dikenal | dihitung sekali | PASS |
| A10 | Teks yang sama diulang | tidak dihitung dua kali | PASS |
| A11 | Penghitung terjemahan | bertambah | PASS |
| A12 | Penanda safe mode ada | `SafeMode::IsEnabled()` benar | PASS |
| A13 | Path penanda dilaporkan | sama dengan file yang dibuat | PASS |

Hasil: `ALL PASS (0 failure(s))`.

Setelah tes ini berjalan pertama kali, ditemukan satu cacat nyata: `UnmatchedCyrillicCount`
menghitung **setiap frame** (satu label yang sama digambar 60×/detik), sehingga angkanya tidak
berguna. Sekarang yang dihitung adalah jumlah string berbeda.

## 2. Pengujian perangkat (belum dijalankan)

Sandbox pengembangan tidak memiliki perangkat Android atau GPU, jadi 17 pengujian berikut
**belum** dieksekusi. Kolom "Bukti yang dicari" adalah baris `samp_log.txt` yang menentukan
lulus/tidak.

| # | Pengujian | Expected | Bukti yang dicari | Status |
|---|---|---|---|---|
| 1 | Startup | sampai menu, tanpa crash | log berakhir normal | belum diuji |
| 2 | RenderWare init | diinisialisasi sekali | `InitialiseRenderWare .. (call #1)` lalu `not loading again` | belum diuji |
| 3 | Database tekstur | hanya database yang ada yang dimuat | `texdb '<x>': loaded` / `no files in storage` | belum diuji |
| 4 | Pemuatan tekstur | tidak ada entry tanpa nama | tidak ada baris `SortEntries: ...` | belum diuji |
| 5 | Karakter | tidak hitam | visual | belum diuji |
| 6 | Map/bangunan | bertekstur | visual | belum diuji |
| 7 | Model kustom | termuat | visual | belum diuji |
| 8 | Koneksi server | masuk server | `Connecting to ...` / `Connected` | belum diuji |
| 9 | Spawn pemain | karakter muncul | visual | belum diuji |
| 10 | Kontrol sentuh | tombol tampil | `Touch controls: CTouchInterface::DrawAll(true) called` | belum diuji |
| 11 | Keyboard/mouse | belum diimplementasikan | — | tidak berlaku |
| 12 | Menu jeda | terbuka, teks Indonesia | log `UI-UNTRANSLATED:` kosong | belum diuji |
| 13 | Peta | terbuka | visual | belum diuji |
| 14 | CLEO/plugin | belum diimplementasikan | — | tidak berlaku |
| 15 | Sesi panjang | tidak crash | log tanpa `SortEntries`/SIGSEGV | belum diuji |
| 16 | Reconnect | tersambung ulang | log | belum diuji |
| 17 | Pemulihan crash | start ke-3 masuk safe mode | `SafeMode: active (...)` | belum diuji |

## 3. Cara menjalankan pengujian otomatis secara lokal

```
./tools/hosttest/run.sh
```

Keluar dengan kode 0 bila semua asersi lulus. Langkah ini juga dipasang di
`.github/workflows/hopepride-apk.yml`, jadi build APK gagal bila tabel bahasa atau safe mode rusak.
