# Laporan aktivitas ke Discord

## Mode langsung tanpa relay

Launcher dapat mengirim laporan percobaan masuk langsung ke webhook Discord. **Mode ini memasukkan URL webhook lengkap ke dalam APK. Semua orang yang mendapatkan APK bisa mengekstrak token, mengirim pesan palsu/spam, atau menyalahgunakannya.** Untuk keamanan gunakan relay yang dijelaskan di bawah. Hapus/rotasi webhook yang sudah pernah terpapar dan gunakan webhook terpisah dengan izin terbatas.

Untuk build GitHub Actions, buat repository secret bernama `HOPEPRIDE_DISCORD_WEBHOOK_URL` melalui GitHub Settings → Secrets and variables → Actions. Isi dengan URL webhook baru di sana, bukan di source code atau chat. Workflow akan membacanya ketika build. Untuk build lokal, sediakan environment variable dengan nama yang sama melalui mekanisme rahasia build di perangkat pengembang. Jangan tulis URL webhook dalam command, log, Gradle property, atau file di repo.

Tanpa secret tersebut mode langsung tidak dikonfigurasi. Jika URL relay HTTPS juga tidak disetel, switch pelaporan akan dinonaktifkan. Jika kedua mode dikonfigurasi, webhook langsung dipakai. Pelaporan aktif secara default jika endpoint dikonfigurasi; pemain dapat mematikannya di Pengaturan. Pengaturan nonaktif yang sudah disimpan tetap dihormati; hanya alamat server dan port yang dikirim, tanpa nickname dan password. Discord dapat melihat alamat IP publik pengirim. Event menunjukkan percobaan masuk, bukan bukti berhasil. Pengiriman bersifat best-effort dan tidak boleh menghambat game.

## Alternatif: relay server (lebih aman untuk token)

Relay ini mengirim **percobaan masuk server**, bukan bukti bahwa koneksi berhasil. Launcher tidak mengirim nickname atau password. Fitur aktif secara default jika relay dikonfigurasi dan dapat dimatikan pemain di Pengaturan; pilihan nonaktif yang sudah disimpan tetap dihormati. Relay dan infrastrukturnya tetap dapat melihat alamat IP internet pengirim; beritahukan pengguna dan atur kebijakan privasi/retensi sesuai kebutuhan. Pengiriman bersifat best-effort; kejadian dapat hilang ketika aplikasi ditutup, offline, atau dibatasi sistem.

## Penyiapan

1. Hapus/rotasi webhook yang pernah terpapar. Buat webhook baru di Discord dan masukkan URL-nya sebagai environment secret `DISCORD_WEBHOOK_URL` di hosting server. Jangan tempel URL webhook di APK, source code, Gradle property, log, atau chat.
2. Jalankan `node server-side/discord-relay/relay.js` dengan Node.js 18 atau lebih baru. Default bind `127.0.0.1:8080`; `HOST` dan `PORT` dapat disesuaikan. Jalankan sebagai layanan yang dikelola dan jangan publikasikan port HTTP mentah.
3. Pasang reverse proxy HTTPS menuju `POST /events`, dengan pembatasan koneksi, ukuran request, WAF/rate limit dan logging yang sesuai privasi. Contoh URL publik **relay**, bukan webhook: `https://relay.example.com/events`.
4. Saat build launcher, berikan URL relay publik tersebut melalui Gradle property `-PhopeprideEventRelayUrl=https://relay.example.com/events`. URL relay publik akan terlihat di APK; jangan gunakan token atau kredensial di URL ini. Bila tidak dikonfigurasi, switch pelaporan tidak tersedia dan game tetap berjalan.
5. Di launcher, pemain dapat mematikan **Laporan percobaan masuk** di Pengaturan. Verifikasi dengan server uji dan channel Discord milik sendiri sebelum distribusi.

Endpoint ini **publik dan tidak memiliki autentikasi yang dapat dipercaya dari APK**. Siapa pun dapat mencoba mengirim event palsu, jadi jangan menganggap pesan Discord sebagai bukti aktivitas pemain. Pembatasan lokal 5 request/menit per alamat TCP dan 30/menit global hanya perlindungan dasar. Di belakang reverse proxy, alamat TCP yang terlihat biasanya alamat proxy, sehingga limit per-client dapat berlaku bersama; terapkan limit per-IP yang benar di proxy terpercaya. Tambahkan pengawasan dan kebijakan pembatasan untuk mencegah spam/biaya. Relay tidak mempercayai header `X-Forwarded-For`.

Tes relay lokal (tanpa menghubungi Discord): `node --test server-side/discord-relay/relay.test.js`.

## Mengirim daftar fitur tanpa build

Workflow **HopePride Debug APK** menyediakan pilihan manual `features` yang hanya mengirim daftar fitur menggunakan secret `HOPEPRIDE_DISCORD_WEBHOOK_URL`; pilihan ini tidak menjalankan job build. Pilihan pada antarmuka GitHub bisa bergantung pada versi workflow di branch default. Untuk branch ini, push dengan `[send-features]` di pesan commit juga menjalankan job pengiriman satu kali (push tersebut tetap memicu build). Push biasa tidak otomatis mengirim daftar fitur. Periksa hasil job `send-feature-list`; build sukses sendiri tidak berarti pesan diterima Discord. Status konfigurasi secret untuk build ditampilkan tanpa URL/token di ringkasan job, tetapi hanya pengiriman sukses membuktikan Discord menerimanya.
