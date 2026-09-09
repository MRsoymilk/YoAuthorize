import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { runInNewContext } from 'node:vm';

const html = await readFile(new URL('./public/index.html', import.meta.url), 'utf8');
const script = await readFile(new URL('./public/app.js', import.meta.url), 'utf8');
const settle = () => new Promise(resolve => setImmediate(resolve));

async function page() {
  const elements = [], timers = new Map(), calls = [];
  let nextTimer = 0, active = 0, maxActive = 0;
  function element() {
    const node = { value: '', checked: false, disabled: false, textContent: '', dataset: {}, children: [], events: {},
      append(...children) { this.children.push(...children); },
      addEventListener(type, callback) { this.events[type] = callback; },
      emit(type) { return this.events[type](); },
    };
    elements.push(node); return node;
  }
  const ids = new Map([...html.matchAll(/id="([^"]+)"/g)].map(([, id]) => [id, element()]));
  const get = id => { assert.ok(ids.has(id), `known HTML id ${id}`); return ids.get(id); };
  get('log-tail').value = '200';
  runInNewContext(script, {
    document: { getElementById: get, createElement: element,
      querySelectorAll: () => elements.filter(node => node.dataset.action) },
    AbortSignal: { timeout: () => undefined },
    setTimeout(callback, delay) { const id = ++nextTimer; timers.set(id, { callback, delay }); return id; },
    clearTimeout: id => timers.delete(id), setInterval: () => {},
    fetch(url) {
      if (!url.startsWith('/api/logs?')) {
        const data = { '/api/session': { token: 'fixture' }, '/api/status': { services: [] }, '/api/job': { job: null } }[url];
        assert.ok(data, `unexpected non-log fetch ${url}`);
        return Promise.resolve({ ok: true, json: async () => data });
      }
      active++; maxActive = Math.max(maxActive, active);
      return new Promise((resolve, reject) => {
        calls.push({ url,
          respond(data) { active--; resolve({ ok: true, json: async () => data }); },
          reject(message) { active--; reject(new Error(message)); },
        });
      });
    },
  });
  await settle();
  const logTimers = () => [...timers.entries()].filter(([, timer]) => timer.delay === 3000);
  return { get, calls, logTimers, maxActive: () => maxActive,
    async tickLogs() {
      const [[id, timer]] = logTimers(); timers.delete(id); void timer.callback(); await settle();
    },
  };
}

test('live logs default off; manual and live snapshots serialize, stay bounded and stop polling', async () => {
  assert.doesNotMatch(html.match(/<input\b[^>]*id="logs-live"[^>]*>/)[0], /\bchecked\b/);
  const p = await page();
  assert.equal(p.get('logs-live').checked, false);
  assert.equal(p.calls.length, 0);
  assert.equal(p.logTimers().length, 0);
  void p.get('logs-refresh').emit('click');
  assert.equal(p.calls.length, 1);
  assert.equal(p.get('logs-refresh').disabled, true);
  p.get('logs-live').checked = true; p.get('logs-live').emit('change');
  assert.equal(p.calls.length, 1, 'enabling live during manual fetch does not overlap');
  assert.equal(p.logTimers().length, 0, 'no polling timer while fetching');
  const snapshot = 'x'.repeat(256 * 1024);
  p.calls[0].respond({ output: snapshot, truncated: true }); await settle();
  assert.equal(p.get('log-output').textContent, snapshot);
  assert.match(p.get('log-note').textContent, /output truncated/);
  assert.equal(p.logTimers().length, 1);
  await p.tickLogs();
  assert.equal(p.calls.length, 2);
  assert.equal(p.logTimers().length, 0);
  p.calls[1].respond({ output: 'replacement' }); await settle();
  assert.equal(p.get('log-output').textContent, 'replacement', 'snapshots replace, never append');
  void p.get('logs-refresh').emit('click');
  assert.equal(p.logTimers().length, 0, 'manual refresh cancels the pending poll');
  assert.equal(p.calls.length, 3);
  p.get('logs-live').checked = false; p.get('logs-live').emit('change');
  p.calls[2].respond({ output: 'last in-flight snapshot' }); await settle();
  assert.equal(p.logTimers().length, 0, 'disabling prevents rescheduling after completion');
  p.get('logs-live').checked = true; p.get('logs-live').emit('change');
  assert.equal(p.calls.length, 4, 'enabling while idle fetches immediately');
  p.calls[3].respond({ output: '' }); await settle();
  assert.match(p.get('log-output').textContent, /No logs returned/);
  assert.equal(p.logTimers().length, 1);
  p.get('logs-live').checked = false; p.get('logs-live').emit('change');
  assert.equal(p.logTimers().length, 0, 'disabling clears an existing timer');
  assert.equal(p.maxActive(), 1);
});

test('service and tail changes coalesce and discard stale success/error responses', async t => {
  for (const staleFailure of [false, true]) await t.test(staleFailure ? 'stale error' : 'stale success', async () => {
    const p = await page();
    void p.get('logs-refresh').emit('click');
    assert.match(p.calls[0].url, /service=api&tail=200$/);
    p.get('log-service').value = 'redis'; p.get('log-service').emit('change');
    p.get('log-service').value = 'api'; p.get('log-service').emit('change');
    assert.equal(p.calls.length, 1);
    assert.equal(p.get('log-output').textContent, 'No current snapshot.');
    if (staleFailure) p.calls[0].reject('obsolete API failure');
    else p.calls[0].respond({ output: 'obsolete API output' });
    await settle();
    assert.equal(p.calls.length, 2);
    assert.equal(p.get('log-output').textContent, 'No current snapshot.', 'even switching back must invalidate old responses');
    assert.doesNotMatch(p.get('log-note').textContent, /obsolete/);
    p.get('log-tail').value = '500'; p.get('log-tail').emit('change');
    // The per-service Logs button must use the same selection invalidation path.
    const postgres = p.get('services').children[0];
    postgres.children[2].children.at(-1).emit('click');
    assert.equal(p.get('log-service').value, 'postgres');
    assert.equal(p.calls.length, 2);
    p.calls[1].respond({ output: 'old tail/service' }); await settle();
    assert.equal(p.calls.length, 3);
    assert.match(p.calls[2].url, /service=postgres&tail=500$/);
    assert.equal(p.get('log-output').textContent, 'No current snapshot.');
    p.calls[2].respond({ output: 'current postgres logs' }); await settle();
    assert.equal(p.get('log-output').textContent, 'current postgres logs');
    assert.match(p.get('log-note').textContent, /^postgres \/ snapshot/);
    assert.equal(p.logTimers().length, 0, 'selection fetching does not enable live polling');
    assert.equal(p.maxActive(), 1);
  });
});

test('live polling reports current errors and recovers on the next bounded snapshot', async () => {
  const p = await page();
  p.get('logs-live').checked = true; p.get('logs-live').emit('change');
  p.calls[0].reject('Docker unavailable'); await settle();
  assert.equal(p.get('log-note').textContent, 'Logs unavailable: Docker unavailable');
  assert.equal(p.get('logs-refresh').disabled, false);
  assert.equal(p.logTimers().length, 1);
  await p.tickLogs();
  p.calls[1].respond({ output: 'recovered' }); await settle();
  assert.equal(p.get('log-output').textContent, 'recovered');
  assert.equal(p.logTimers().length, 1);
  assert.equal(p.maxActive(), 1);
});
