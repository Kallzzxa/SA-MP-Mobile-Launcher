<div align="center">
  <img src="app/src/main/res/drawable-nodpi/hopepride_banner.png" alt="HopePride Roleplay banner" width="100%" />
  <br /><br />
  <img src="app/src/main/res/drawable-nodpi/hopepride_logo.png" alt="HopePride logo" width="112" />

  # HOPEPRIDE

  **SA-MP MOBILE LAUNCHER**<br />
  Satu launcher untuk perjalananmu di HopePride Roleplay.

  <a href="#download-apk">Download APK</a> &nbsp; • &nbsp;
  <a href="#fitur">Fitur</a> &nbsp; • &nbsp;
  <a href="#build-lokal">Build Lokal</a> &nbsp; • &nbsp;
  <a href="#struktur-proyek">Struktur Proyek</a>

  <br />

  ![Platform](https://img.shields.io/badge/PLATFORM-ANDROID-2496ED?style=for-the-badge&logo=android&logoColor=white)
  ![Support](https://img.shields.io/badge/SUPPORT-SA--MP%20%2F%20open.mp-0866B8?style=for-the-badge)
  ![Version](https://img.shields.io/badge/VERSION-1.7.0-12263A?style=for-the-badge)

  **Launcher Developer**<br />
  `PALL` &nbsp; × &nbsp; `FANN`
</div>

---

## 👋 Selamat datang

**HopePride** adalah launcher Android untuk SA-MP dan open.mp. Kelola data game, simpan server favorit, dan atur pengalaman bermain dalam satu aplikasi.

| | Detail |
|:---|:---|
| **Nama aplikasi** | HopePride |
| **Developer launcher** | Pall & Fann |
| **Versi aplikasi** | 1.7.0 |
| **Android minimum** | Android 8.0 / API 26 |
| **Arsitektur** | ARM64 & ARMv7 |
| **Package ID** | `com.hopepride.samp` |

> [!IMPORTANT]
> URL konfigurasi, unduhan data, dan daftar Hosted di kode masih mengarah ke layanan proyek sebelumnya. Sesuaikan sebelum launcher digunakan untuk server HopePride.

## Fitur

<table>
  <tr>
    <td width="50%"><strong>🎮 Siap bermain</strong><br />Periksa data game dan buka client dari launcher.</td>
    <td width="50%"><strong>🌐 Daftar server</strong><br />Lihat Hosted dan simpan server favorit secara lokal.</td>
  </tr>
  <tr>
    <td><strong>📦 Pengelola data</strong><br />Unduh, lanjutkan unduhan, dan ekstrak paket data game.</td>
    <td><strong>⚙️ Pengaturan client</strong><br />Atur nickname, versi client, FPS, keyboard, dan opsi lainnya.</td>
  </tr>
  <tr>
    <td><strong>🖥️ MobileCEF</strong><br />Integrasi UI WebView dan contoh Pawn untuk server.</td>
    <td><strong>🛠️ Native Android</strong><br />Launcher Java dengan komponen game C++ untuk ARM.</td>
  </tr>
</table>

## Download APK

APK uji dibuat melalui **GitHub Actions** dan disimpan sebagai artifact, bukan otomatis sebagai GitHub Release.

1. Buka **[HopePride Debug APK → Actions](https://github.com/HOOOOOOQ/SA-MP-HOPEPRIDE-LAUNCER/actions/workflows/hopepride-apk.yml)**.
2. Pilih build yang berstatus **success**.
3. Pada bagian **Artifacts**, unduh **HopePride-debug**.
4. Ekstrak file ZIP untuk mendapatkan **`HopePride-debug.apk`**.

Artifact tersedia selama **14 hari** setelah build. Jika belum ada build yang berhasil, jalankan workflow secara manual dari tab Actions. Workflow juga berjalan saat ada push ke cabang `arena/**`.

> [!CAUTION]
> Ini adalah APK **debug** dengan kunci debug, bukan APK rilis produksi. APK mungkin tidak bisa memperbarui instalasi yang menggunakan tanda tangan berbeda. Cadangkan data sebelum mencopot aplikasi lama; penghapusan aplikasi dapat menghapus data lokalnya.

## 🚀 Mulai menggunakan

1. Instal APK uji di perangkat Android yang kompatibel.
2. Buka HopePride dan siapkan paket data game yang sesuai, atau letakkan data secara manual di direktori yang ditampilkan pada halaman Home.
3. Buka tab **Play**, tambahkan atau pilih server, lalu masukkan nickname.
4. Sesuaikan opsi di **Settings** dan sambungkan ke server.

Pengujian di perangkat Android tetap diperlukan. Pastikan kamu memiliki izin untuk menggunakan data dan aset game yang dipasang.

## Build lokal

### Kebutuhan

| Komponen | Versi |
|:---|:---|
| JDK | 17 |
| Android SDK Platform | 34 |
| Android Build Tools | 34.0.0 |
| Android NDK | 26.2.11394342 |
| CMake | 3.22.1 |
| Git LFS | Diperlukan untuk library native |

Konfigurasikan Android SDK melalui Android Studio, `ANDROID_HOME`, atau `local.properties`, lalu unduh file LFS dan jalankan build:

**Linux / macOS**

```bash
git lfs pull
./gradlew testDebugUnitTest assembleDebug
```

**Windows**

```bat
git lfs pull
gradlew.bat testDebugUnitTest assembleDebug
```

Output lokal: `app/build/outputs/apk/debug/app-debug.apk`<br />
Artifact GitHub Actions: `HopePride-debug.apk`

> [!IMPORTANT]
> Package ID `com.hopepride.samp` adalah aplikasi baru bagi Android, bukan pembaruan dari `com.hopepride.samp`. Cadangkan data lama sebelum menghapus aplikasi lama; file game perlu disalin atau diunduh ke direktori aplikasi baru. Favorit dan pengaturan tidak berpindah otomatis. Namespace Java/JNI tetap menggunakan nama lama untuk menjaga kompatibilitas library native.

## 🔧 Konfigurasi sebelum rilis

- **Konfigurasi dan paket data:** periksa URL di [`GameDataManager.java`](app/src/main/java/com/sampmobile/xyvern/game/GameDataManager.java) dan [`HomeFragment.java`](app/src/main/java/com/sampmobile/xyvern/launcher/fragments/HomeFragment.java).
- **Daftar server Hosted:** periksa `HOSTED_URL` di [`PlayFragment.java`](app/src/main/java/com/sampmobile/xyvern/launcher/fragments/PlayFragment.java).
- **Discord (opsional):** laporan percobaan masuk aktif secara default jika webhook dikonfigurasi dan dapat dimatikan pemain di Pengaturan; laporan dapat dikirim langsung ke webhook. Lihat [panduan Discord](server-side/discord-relay/README.md). URL webhook belum dikonfigurasi di APK secara default.
- **MobileCEF:** ikuti [panduan integrasi server](server/README.md).
- **Custom model:** `server-side/` berisi contoh generator manifest dan loader Pawn. Contohnya belum menjamin sinkronisasi model end-to-end; sesuaikan endpoint dan uji di server serta perangkat.
- **Firebase:** konfigurasi bawaan adalah milik aplikasi lama dan tidak digunakan untuk ID baru. Untuk mengaktifkan Firebase/FCM dan Crashlytics, daftarkan `com.hopepride.samp` di proyek Firebase milikmu lalu tempatkan `google-services.json` yang sesuai di `app/`. Tanpa itu Firebase dinonaktifkan.
- **Signing produksi:** amankan dan kelola kunci signing sebelum distribusi. Keystore dan kredensial release yang ada di repositori harus dianggap terekspos; rencanakan rotasi sesuai mekanisme distribusi aplikasi.

### Apa itu Firebase?

Firebase adalah layanan Google yang dapat menyediakan notifikasi push (FCM), laporan crash (Crashlytics), dan analitik aplikasi. Firebase bukan server SA-MP dan tidak wajib untuk bermain. Untuk package baru, konfigurasi Firebase lama tidak cocok sehingga integrasinya dinonaktifkan sampai pemilik menyediakan konfigurasi Firebase untuk `com.hopepride.samp`. Mengaktifkannya perlu pengujian, pemberitahuan privasi yang sesuai, serta pengecekan biaya/kuota sesuai paket layanan yang dipilih.

## Struktur proyek

```text
HopePride/
├── app/             # Launcher Android, resource, dan kode native
├── server/          # MobileCEF, contoh UI, dan integrasi Pawn
├── server-side/     # Contoh manifest dan loader custom model 0.3.DL
├── preview/         # Screenshot lama dari proyek sebelumnya
└── .github/         # Workflow GitHub Actions
```

## 🤝 Kredit

**HopePride Launcher** dikembangkan oleh **Pall & Fann** dan menggunakan kode upstream dari [JohnPeriaX/KotlinJava-DSL-SampMobile-2.10](https://github.com/JohnPeriaX/KotlinJava-DSL-SampMobile-2.10).

Kontributor yang dicantumkan: [HOOOOOOQ](https://github.com/HOOOOOOQ) dan [Xaeraa](https://github.com/Xaeraa).

Lihat juga [daftar kontributor kode di GitHub](https://github.com/HOOOOOOQ/SA-MP-HOPEPRIDE-LAUNCER/graphs/contributors).

---

<div align="center">
  <img src="app/src/main/res/drawable-nodpi/hopepride_logo.png" alt="HopePride" width="56" />
  <br />
  <strong>HOPEPRIDE ROLEPLAY</strong><br />
  <sub>Launcher by Pall &amp; Fann</sub>
</div>

## Kompatibilitas fitur SA-MP PC (bertahap)

Launcher ini memiliki implementasi chat, command, scoreboard, dialog server, textdraw, serta dukungan awal custom model. Ini **bukan jaminan kesetaraan penuh dengan SA-MP PC**. Perubahan terbaru menambahkan toggle `/timestamp` untuk pesan chat berikutnya, memperbaiki riwayat input, pembaruan scoreboard, dan penanganan dialog tablist / penutupan oleh server. Beberapa perintah lokal (termasuk `/timestamp`) ditangani client, bukan server.

Pengujian berikut masih diperlukan pada Android dan server SA-MP/open.mp sebelum rilis:

- Buka launcher dari awal dan dengarkan suara sambutan; pastikan berhenti saat launcher ditutup atau game dibuka, serta tidak berulang saat activity dibuat ulang.
- Sebelum masuk game, uji instalasi font `Textures/Fonts/RussianFont.met` dan `.png` yang hilang; pastikan file font pengguna yang sudah ada tidak ditimpa. Lalu uji loading di HP dengan data game yang lengkap.

- Cari server berdasarkan nama tersimpan atau alamat di tab Favorit/Hosted, coba urutan asli, nama tersimpan, dan alamat, lalu uji **Muat ulang** dan pastikan pilihan serta favorit tetap berfungsi. Pengurutan hanya mengubah tampilan, bukan favorit yang tersimpan; nama yang dipakai untuk urutan adalah nama tersimpan atau nama dari daftar Hosted, bukan nama query terbaru.
- Di detail server, gunakan **Ukur ping server** untuk melihat waktu respons query UDP; nilai ini tidak sama dengan ping dalam game dan dapat tidak tersedia.
- Di detail server, buka **Lihat daftar pemain** untuk query ID, nama, skor dan ping; cari nama dan coba pengurutan nama, skor, ping, ID serta urutan server. Pencarian dan pengurutan hanya berlaku pada respons yang diterima; hasil hanya tersedia bila server mendukung query detail.
- Gunakan **Lihat info server** untuk melihat rules yang diberikan server, **Salin alamat server** untuk menyalin IP:port, dan **Simpan ke favorit** untuk menyimpan server Hosted. Query info mungkin dinonaktifkan oleh pemilik server.
- Buka/tutup TAB beberapa kali; periksa nama, skor, ping dan pembaruan setelah pemain bergabung/keluar.
- Kirim chat dan command, navigasikan riwayat pada kedua keyboard, serta uji `/timestamp`.
- Coba dialog MSGBOX, INPUT, PASSWORD, LIST, TABLIST dan TABLIST_HEADERS, termasuk kolom kosong, tag warna, teks dengan karakter `<` / `&`, dan penutupan oleh server.
- Uji mapping padat dan material objek (termasuk beberapa tekstur/teks pada satu objek), streaming masuk/keluar dan kondisi memori rendah di perangkat.
- Uji trailer pada dua pemain (attach, lepas, ganti trailer, bergerak dan berhenti), termasuk tow truck; periksa paket terpotong dan streaming kendaraan.
- Uji penumpang dua pemain di kursi 1–7, kontrol analog/tombol dan drive-by, termasuk masuk/keluar kendaraan serta pergantian senjata. Uji tembakan ke pemain/kendaraan dan objek bergerak/berhenti pada mapping.
- Uji bidikan dan efek tembakan tank, air fire truck, kendaraan lain, textdraw yang dapat dipilih, custom model 0.3.DL, reconnect, serta koneksi ke server berbeda; kompatibilitasnya belum diverifikasi menyeluruh. Dukungan unduh 0.3.DL saat ini belum mendukung HTTPS dan belum terbukti end-to-end di Android.
