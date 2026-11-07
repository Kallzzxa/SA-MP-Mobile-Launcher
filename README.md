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
> Package ID `com.hopepride.samp` adalah aplikasi baru bagi Android, bukan pembaruan dari `com.sampmobile.xyvern`. Cadangkan data lama sebelum menghapus aplikasi lama; file game perlu disalin atau diunduh ke direktori aplikasi baru. Favorit dan pengaturan tidak berpindah otomatis. Namespace Java/JNI tetap menggunakan nama lama untuk menjaga kompatibilitas library native.

## 🔧 Konfigurasi sebelum rilis

- **Konfigurasi dan paket data:** periksa URL di [`GameDataManager.java`](app/src/main/java/com/sampmobile/xyvern/game/GameDataManager.java) dan [`HomeFragment.java`](app/src/main/java/com/sampmobile/xyvern/launcher/fragments/HomeFragment.java).
- **Daftar server Hosted:** periksa `HOSTED_URL` di [`PlayFragment.java`](app/src/main/java/com/sampmobile/xyvern/launcher/fragments/PlayFragment.java).
- **MobileCEF:** ikuti [panduan integrasi server](server/README.md).
- **Custom model:** `server-side/` berisi contoh generator manifest dan loader Pawn. Contohnya belum menjamin sinkronisasi model end-to-end; sesuaikan endpoint dan uji di server serta perangkat.
- **Firebase:** konfigurasi bawaan adalah milik aplikasi lama dan tidak digunakan untuk ID baru. Untuk mengaktifkan Firebase/FCM dan Crashlytics, daftarkan `com.hopepride.samp` di proyek Firebase milikmu lalu tempatkan `google-services.json` yang sesuai di `app/`. Tanpa itu Firebase dinonaktifkan.
- **Signing produksi:** amankan dan kelola kunci signing sebelum distribusi. Keystore dan kredensial release yang ada di repositori harus dianggap terekspos; rencanakan rotasi sesuai mekanisme distribusi aplikasi.

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

---

<div align="center">
  <img src="app/src/main/res/drawable-nodpi/hopepride_logo.png" alt="HopePride" width="56" />
  <br />
  <strong>HOPEPRIDE ROLEPLAY</strong><br />
  <sub>Launcher by Pall &amp; Fann</sub>
</div>
