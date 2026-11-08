# HopePride — Safe Mode & Pemulihan

Dokumen ini menjelaskan mekanisme pemulihan yang **sudah ada di source**, bukan rencana.

## Cara kerja

1. `HopeApplication.onCreate()` memanggil `Diagnostics.install()` lalu `BootGuard.install()`.
2. `BootGuard` menyimpan penanda `boot_in_progress` di SharedPreferences `hopepride_recovery`
   setiap kali proses lahir.
3. Kalau proses mati sebelum watchdog 25 detik, penanda masih `true` saat start berikutnya →
   dihitung sebagai satu start-up gagal (`BOOT_INTERRUPTED`).
4. Dua start-up gagal berturut-turut → **safe mode** aktif, lalu file penanda
   `safe_mode.on` ditulis ke:
   - `/data/data/com.hopepride.samp/files/safe_mode.on`
   - `/storage/emulated/0/Android/data/com.hopepride.samp/files/safe_mode.on`
5. Lapisan native membaca penanda itu (`SafeMode::IsEnabled()` di `game/SafeMode.h`) sebelum
   memasang patch opsional. Dalam safe mode `TextureVariantFallback::Initialise()` **tidak**
   memasang hook `LoadThumbs`, `Load`, maupun `SortEntries` — engine berjalan seperti aslinya.
6. Sesi yang berjalan 25 detik tanpa crash menghapus penanda `boot_in_progress` dan
   mengosongkan penghitung kegagalan (`BOOT_HEALTHY`).

Data pemain, konfigurasi, dan file game tidak pernah dihapus. Keluar dari safe mode hanya
menghapus `safe_mode.on` (`BootGuard.clearSafeMode`).

## Mencari bukti di log

```
SafeMode: inactive
SafeMode: active (/data/data/com.hopepride.samp/files/safe_mode.on), texture database hooks not installed
BootHealthy / BOOT_INTERRUPTED        ← di log diagnostics Java (files/diagnostics)
```

## Keluar dari safe mode

Belum ada tombol di UI. Untuk saat ini: hapus file `safe_mode.on` di salah satu lokasi di atas,
lalu jalankan ulang aplikasi. Tombol "Matikan Safe Mode" akan menyusul bersama layar Settings.
