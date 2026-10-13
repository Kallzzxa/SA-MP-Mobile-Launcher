# SA-MP Mobile Launcher & Server Sync Ecosystem

Arsitektur repositori terbagi menjadi 2 komponen utama:

---

## 1. `client-side/` (Android Launcher & Game Engine)
Launcher Android native untuk SA-MP & open.mp:
* **Clean UI:** Desain gelap minimalis tanpa styling AI-slop atau warna neon berlebih.
* **Pre-Join Model Sync:** Memeriksa manifest server dan mengunduh aset 0.3.DL (DFF & TXD) secara otomatis sebelum masuk ke game.
* **Game Data Manager:** Verifikasi file inti GTA SA secara lokal + resume download `files.zip` via HTTP range.
* **Android 16 Ready:** Kompatibel penuh dengan targetSdk 34, `extractNativeLibs="true"`, dan skema tanda tangan V1+V2+V3.

---

## 2. `server-side/` (Server Sync & 0.3.DL Content Pipeline)
Komponen integrasi open.mp / SA-MP:
* `models/`: Tempat menaruh file `.dff` dan `.txd`.
* `generate_manifest.py`: Script otomatis untuk menghitung sha256 checksum dan ukuran file menjadi `models_manifest.json`.
* `models_manifest.json`: Endpoint manifest HTTP yang dibaca oleh client-side launcher sebelum player join.
* `custom_models_loader.inc`: Modul Pawn untuk mendaftarkan model via `AddSimpleModel()` / `AddCharModel()`.

---

## 3. Upstream & Main Dev Source Code
* **Original Main Dev Project:** [JohnPeriaX/KotlinJava-DSL-SampMobile-2.10](https://github.com/JohnPeriaX/KotlinJava-DSL-SampMobile-2.10)
* **Dedicated Branch:** Source code original 100% dari main dev tersimpan dan dapat diakses langsung pada branch [`main-dev`](https://github.com/rezzylrn/samp-mobile-optimized/tree/main-dev).

