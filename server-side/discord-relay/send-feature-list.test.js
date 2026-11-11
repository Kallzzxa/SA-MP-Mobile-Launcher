'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const { content, sendFeatureList } = require('./send-feature-list');

const example = 'https://discord.com/api/webhooks/123/example-token';
test('feature list is sent with mentions disabled', async () => {
  let options;
  await sendFeatureList(example, async (url, init) => {
    assert.equal(url, example); options = init;
    return { ok: true, status: 204, body: null };
  });
  assert.equal(options.method, 'POST');
  assert.equal(options.redirect, 'manual');
  assert.deepEqual(JSON.parse(options.body), { content, allowed_mentions: { parse: [] } });
  assert.ok(content.length <= 2000);
});
test('rejects invalid destinations and unsuccessful responses', async () => {
  let called = false;
  await assert.rejects(sendFeatureList('https://not-discord.example/', async () => { called = true; }));
  assert.equal(called, false);
  await assert.rejects(sendFeatureList(example, async () => ({ ok: false, status: 429, body: null })), /429/);
});
