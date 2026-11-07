'use strict';

// Minimal standalone relay. Keep the real webhook in the server environment only.
const http = require('node:http');
const MAX_BODY = 1024;
const WINDOW_MS = 60_000;

function validWebhook(value) {
  try {
    const u = new URL(value);
    return u.protocol === 'https:' && u.hostname === 'discord.com' && !u.port &&
      !u.username && !u.password && !u.search && !u.hash &&
      /^\/api\/webhooks\/\d+\/[A-Za-z0-9._-]+$/.test(u.pathname);
  } catch { return false; }
}

function validEvent(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value) &&
    Object.keys(value).every((key) => ['event', 'host', 'port'].includes(key)) &&
    value.event === 'join_attempt' && typeof value.host === 'string' &&
    /^[A-Za-z0-9._-]{1,127}$/.test(value.host) && Number.isInteger(value.port) &&
    value.port >= 1 && value.port <= 65535;
}

function createRelay({ deliver, perClientLimit = 5, globalLimit = 30, now = Date.now }) {
  if (typeof deliver !== 'function') throw new Error('Delivery function required');
  let globalWindow = { start: now(), count: 0 };
  const clients = new Map();
  function allowed(client) {
    const time = now();
    if (time - globalWindow.start >= WINDOW_MS) globalWindow = { start: time, count: 0 };
    for (const [key, entry] of clients) if (time - entry.start >= WINDOW_MS) clients.delete(key);
    if (globalWindow.count >= globalLimit) return false;
    if (!clients.has(client)) {
      if (clients.size >= 1024) return false;
      clients.set(client, { start: time, count: 0 });
    }
    const entry = clients.get(client);
    if (entry.count >= perClientLimit) return false;
    entry.count++;
    globalWindow.count++;
    return true;
  }
  function reply(res, status) {
    if (res.destroyed || res.writableEnded) return;
    res.writeHead(status, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff' });
    res.end(JSON.stringify({ ok: status === 202 }));
  }
  const server = http.createServer((req, res) => {
    if (req.method !== 'POST' || req.url !== '/events') { reply(res, 404); req.resume(); return; }
    if (!allowed(req.socket.remoteAddress || 'unknown')) { reply(res, 429); req.resume(); return; }
    if (!/^application\/json(?:\s*;|$)/i.test(req.headers['content-type'] || '')) { reply(res, 415); req.resume(); return; }
    if (Number(req.headers['content-length'] || 0) > MAX_BODY) { reply(res, 413); req.resume(); return; }
    let size = 0;
    const chunks = [];
    req.on('data', (chunk) => {
      size += chunk.length;
      if (size > MAX_BODY) { reply(res, 413); req.destroy(); return; }
      chunks.push(chunk);
    });
    req.on('end', async () => {
      if (size > MAX_BODY || res.writableEnded) return;
      let event;
      try { event = JSON.parse(Buffer.concat(chunks).toString('utf8')); }
      catch { reply(res, 400); return; }
      if (!validEvent(event)) { reply(res, 400); return; }
      try {
        await deliver({ event: event.event, host: event.host, port: event.port });
        reply(res, 202);
      } catch { reply(res, 502); }
    });
    req.on('error', () => { if (!res.writableEnded) reply(res, 400); });
  });
  server.requestTimeout = 10_000;
  server.headersTimeout = 10_000;
  return server;
}

async function sendDiscord(webhook, event) {
  const response = await fetch(webhook, {
    method: 'POST', redirect: 'manual', signal: AbortSignal.timeout(5000),
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      content: `HopePride: percobaan masuk ke ${event.host}:${event.port} (belum tentu berhasil).`,
      allowed_mentions: { parse: [] }
    })
  });
  if (response.body) await response.body.cancel();
  if (!response.ok) throw new Error('Discord delivery failed');
}

if (require.main === module) {
  const webhook = process.env.DISCORD_WEBHOOK_URL || '';
  if (!validWebhook(webhook)) {
    console.error('DISCORD_WEBHOOK_URL belum valid. Atur melalui environment server.');
    process.exit(1);
  }
  const port = Number(process.env.PORT || 8080);
  if (!Number.isInteger(port) || port < 1 || port > 65535) throw new Error('PORT tidak valid');
  const server = createRelay({ deliver: (event) => sendDiscord(webhook, event) });
  // Bind locally by default; expose through a TLS reverse proxy with additional abuse controls.
  server.listen(port, process.env.HOST || '127.0.0.1', () => console.log('Relay aktif'));
}

module.exports = { createRelay, validWebhook, validEvent };
