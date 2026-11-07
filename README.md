<div align="center">
  <img src="app/src/main/res/drawable-nodpi/hopepride_banner.png" alt="Banner HopePride Roleplay" width="800" />
  <br />
  <img src="app/src/main/res/drawable-nodpi/hopepride_logo.png" alt="Logo HopePride" width="128" />

# HopePride

**SA-MP Mobile Launcher untuk HopePride Roleplay**<br />
**Dev Launcher: Pall &amp; Fann**
</div>

## Tentang proyek

HopePride adalah launcher Android untuk SA-MP/open.mp dengan pengelolaan data game, daftar server favorit dan Hosted, serta pengaturan client. Kode Android dan native berada di `app/`. Integrasi Pawn dan contoh UI WebView tersedia di `server/`; contoh pipeline custom model ada di `server-side/`.

Nama aplikasi yang tampil adalah **HopePride**. Build uji dari GitHub Actions disimpan sebagai **`HopePride-debug.apk`**. Package ID tetap `com.sampmobile.xyvern` demi kompatibilitas dengan konfigurasi dan data aplikasi yang sudah ada.

> Paket data game dan URL konfigurasi/server yang ada di kode masih mengarah ke layanan proyek sebelumnya. Sesuaikan sebelum digunakan untuk server HopePride. Contoh custom model pada `server-side/` bukan jaminan sinkronisasi model end-to-end sudah berfungsi.

## Build APK uji dengan GitHub Actions

Workflow **HopePride Debug APK** menjalankan tes unit dan membangun APK debug untuk ARM64 dan ARMv7. Workflow berjalan pada push ke cabang `arena/**`, dan juga dapat dijalankan secara manual melalui tab **Actions** setelah workflow tersedia di cabang default. Buka run yang berhasil dan unduh artifact **HopePride-debug**; ekstrak untuk mendapatkan `HopePride-debug.apk`. Workflow ini tidak membuat GitHub Release.

APK debug ditandatangani dengan kunci debug, bukan kunci produksi. Biasanya APK ini tidak dapat memperbarui instalasi yang ditandatangani dengan kunci berbeda; mencopot aplikasi lama dapat menghapus data lokalnya, jadi cadangkan data terlebih dahulu. Jangan distribusikan APK debug sebagai rilis produksi.

## Build lokal

Persyaratan: Git LFS, JDK 17, Android SDK Platform 34, build-tools 34.0.0, NDK 26.2.11394342, dan CMake 3.22.1. Unduh file LFS sebelum build dan konfigurasikan Android SDK di Android Studio atau melalui `ANDROID_HOME`/`local.properties`.

```bash
git lfs pull
./gradlew testDebugUnitTest assembleDebug
```

Pada Windows gunakan `gradlew.bat` sebagai pengganti `./gradlew`. Output Gradle berada di `app/build/outputs/apk/debug/app-debug.apk`. Pengujian di perangkat Android tetap diperlukan untuk memastikan game dan aset berfungsi.

## Struktur

- `app/` — launcher Android, resource UI, dan kode game native.
- `server/` — integrasi MobileCEF dan contoh Pawn; lihat [panduan server](server/README.md).
- `server-side/` — contoh manifest dan loader model 0.3.DL.
- `preview/` — screenshot yang ada di repositori.

Proyek ini menggunakan kode upstream [JohnPeriaX/KotlinJava-DSL-SampMobile-2.10](https://github.com/JohnPeriaX/KotlinJava-DSL-SampMobile-2.10). Pastikan izin penggunaan aset game dan keamanan signing key sebelum membuat rilis produksi.
