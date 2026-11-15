'use strict';

const { validWebhook } = require('./relay');

const content = `**Fitur HopePride Launcher**
• Unduh dan ekstrak data game, termasuk lanjutkan unduhan.
• Daftar Hosted dan favorit; tambah, cari, urutkan, dan muat ulang server.
• Detail server, salin alamat, daftar pemain dan info server jika query didukung.
• Ukur ping query server; cari dan urutkan daftar pemain.
• Chat, command, riwayat input, /timestamp, scoreboard/TAB, dan dialog server.
• Pengaturan nickname, keyboard, FPS, jumlah pesan chat, voice chat, AML, dan CEF.
• Laporan percobaan masuk ke Discord jika dikonfigurasi; dapat dimatikan pemain.
• Dukungan awal MobileCEF dan custom model.

Fitur terbaru belum diuji di HP/server; kompatibilitas penuh dengan SA-MP PC belum terjamin. CLEO belum tersedia.`;

async function sendFeatureList(webhook, request = fetch) {
  if (!validWebhook(webhook)) throw new Error('Webhook tidak dikonfigurasi atau tidak valid');
  const response = await request(webhook, {
    method: 'POST', redirect: 'manual', signal: AbortSignal.timeout(10000),
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ content, allowed_mentions: { parse: [] } })
  });
  if (response.body) await response.body.cancel();
  if (!response.ok) throw new Error(`Discord menolak pesan (status ${response.status})`);
}

if (require.main === module) {
  sendFeatureList(process.env.HOPEPRIDE_DISCORD_WEBHOOK_URL || '')
    .then(() => console.log('Daftar fitur diterima Discord.'))
    .catch((error) => {
      // Do not print fetch errors, URLs, response bodies, or webhook tokens.
      console.error(error.message && /^Discord menolak pesan \(status \d+\)$/.test(error.message)
        ? error.message : 'Gagal mengirim: periksa secret dan koneksi Discord.');
      process.exitCode = 1;
    });
}
module.exports = { content, sendFeatureList };
