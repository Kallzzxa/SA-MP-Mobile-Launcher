# omp-cef untuk Android (port client PC ke launcher ini)

Satu gamemode, satu `.inc` (upstream `cef.inc` dari aurora-mp/omp-cef),
jalan di PC **dan** Android.

- PC: pemain pakai client plugin omp-cef asli (Chromium, DX9).
- Android: pemain pakai launcher ini. Client omp-cef-nya sudah di-port
  ke Android di `app/src/main/java/com/sampmobile/xyvern/cef/omp/` +
  `OmpCefClient.java` + WebView di `CefJavaManager.java`. Rendering-nya
  WebView, bukan Chromium CEF, tapi protokolnya sama persis dengan PC:
  UDP ke port game + 2, handshake libsodium (X25519 + BLAKE2b), KCP,
  secretbox, manifest, `.pak` resource, `http://cef/<resource>/<file>`,
  dan event `CreateBrowser` / `EmitEvent` / `ClientEmitEvent`.

## Server: yang harus dipasang (tidak berubah dari PC)

1. Download server plugin omp-cef upstream (`.dll` Windows / `.so`
   Linux) dari rilis aurora-mp/omp-cef, taruh di `plugins/`.
2. `server.cfg`: `plugins omp-cef` (tambah plugin lain yang sudah ada).
3. Copy `cef.inc` upstream ke `pawno/include/`.
4. Resource UI ditaruh di `scriptfiles/cef/<nama_resource>/`, misal
   `scriptfiles/cef/cef_demo/` (contoh di folder ini, tinggal copy).
   Server plugin akan mem-pak folder itu jadi `<nama>.pak` sendiri.
5. Di `OnGameModeInit`: `CEF_AddResource("cef_demo");`
6. Setelah `OnCefReady(playerid)`, baru `CEF_CreateBrowser(...)`.

Contoh gamemode minimal: `example_omp_demo.pwn` di folder ini.

## Yang didukung Android vs PC

Didukung Android (Overlay2D):
- `CEF_PlayerHasPlugin`, `CEF_AddResource`, `CEF_CreateBrowser`
  (satu atau banyak browser, layer via elevation),
  `CEF_SetBrowserVisible`, `CEF_SetBrowserLayer`, `CEF_DestroyBrowser`,
  `CEF_ReloadBrowser`, `CEF_FocusBrowser`, `CEF_LoadUrl`,
  `CEF_RegisterEvent`, `CEF_EmitEvent`,
  `OnCefInitialize`, `OnCefDownloadStart/Progress/Finish` (progress
  dihitung di client; callback Pawn-nya tetap dipicu server plugin),
  `OnCefReady`, `OnCefBrowserCreated`, `CEF_ExitGame`.

Belum didukung Android (sengaja ditolak, bukan diam-diam salah):
- `CEF_CreateWorldBrowser`, `CEF_CreateWorld2DBrowser` -> client balas
  `BrowserCreateResult` ERROR_GENERIC, alasan "World browsers are not
  supported on Android". WebView tidak bisa render ke tekstur objek /
  proyeksi world-to-screen seperti DX9 di PC.
- Audio spasial (`AUDIO_MODE_WORLD`, `SetBrowserAudioSettings`),
  HUD toggle, spawn screen toggle, key capture, ESC/player-list custom,
  DevTools: diterima server, diabaikan client Android (audio WebView
  selalu 2D UI).

## Kalau server tidak pakai omp-cef

Client Android mencoba handshake omp-cef 5x @ 2 detik. Kalau tidak ada
balasan, client menganggap server non-CEF dan berhenti, lalu CEF lama
(MobileCEF, RakNet paket 252) tetap jalan seperti biasa. Jadi satu APK
bisa masuk ke server omp-cef maupun server MobileCEF lama.

## Tes yang sudah dilakukan (VM, tanpa server asli)

- KCP Java: loopback in-memory, pesan 10 / 1200 / 5000 / 20000 byte,
  drop ~10% + out-of-order, semua utuh dan urut. Zero-window probe lolos.
- Crypto Java: X25519 RFC 7748, BLAKE2b vs Python hashlib, secretbox
  round-trip + tamper ditolak, kx client/server cocok, dan output
  secretbox + kx byte-identik dengan libsodium sistem (cek via ctypes).
- Mock server omp-cef (Java, in-memory transport karena sandbox VM
  memblokir UDP send): handshake, manifest, ServerConfig, download
  `.pak`, SHA-256, unzip + AES-CBC dekripsi entry, VFS, CreateBrowser,
  event user, BrowserCreateResult — semua PASS.
- Compile: `javac` Android (android.jar 34) untuk semua file CEF Java
  bersih; `clang++` NDK untuk `SAMPMobileCef.cpp` bersih (1 warning
  lama dari RakNet). `netgame.cpp` tidak bisa dicek penuh di luar CMake
  (butuh define build `VER_x32` dkk), jadi build APK penuh tetap harus
  lewat CI.

Belum dites live: lawan server plugin omp-cef asli (butuh server +
  plugin upstream + APK dari CI). Itu langkah tes berikutnya sebelum
  rilis.
