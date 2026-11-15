'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const { createRelay, validWebhook, validEvent } = require('./relay');

test('webhook host and URL are constrained', () => {
  assert.equal(validWebhook('https://discord.com/api/webhooks/123/abc_def'), true);
  assert.equal(validWebhook('https://discord.com.evil.test/api/webhooks/123/abc'), false);
  assert.equal(validWebhook('http://discord.com/api/webhooks/123/abc'), false);
  assert.equal(validWebhook('https://discord.com/api/webhooks/123/abc?secret=x'), false);
});

test('event validation rejects extra fields, invalid hosts and ports', () => {
  assert.equal(validEvent({ event: 'join_attempt', host: 'test.example', port: 7777 }), true);
  assert.equal(validEvent({ event: 'join_attempt', host: 'test.example', port: 7777, nickname: 'x' }), false);
  assert.equal(validEvent({ event: 'join_attempt', host: '@everyone', port: 7777 }), false);
  assert.equal(validEvent({ event: 'join_attempt', host: 'test', port: 65536 }), false);
});

test('relay delivers accepted events, rejects invalid requests and rate limits', async (t) => {
  const delivered = [];
  const server = createRelay({ deliver: async (event) => delivered.push(event), perClientLimit: 4 });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  t.after(() => new Promise(resolve => server.close(resolve)));
  const url = `http://127.0.0.1:${server.address().port}/events`;
  const send = (body) => fetch(url, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
  const event = { event: 'join_attempt', host: 'play.example.com', port: 7777 };
  assert.equal((await send(event)).status, 202);
  assert.deepEqual(delivered, [event]);
  assert.equal((await send({ ...event, password: 'must-not-forward' })).status, 400);
  assert.equal((await send({ ...event, host: '@everyone' })).status, 400);
  assert.equal((await send(event)).status, 202);
  assert.equal((await send(event)).status, 429);
  assert.equal(delivered.length, 2);
});
