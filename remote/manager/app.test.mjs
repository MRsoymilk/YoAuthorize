import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { runInNewContext } from 'node:vm';

const html = await readFile(new URL('./public/index.html', import.meta.url), 'utf8');
const script = await readFile(new URL('./public/app.js', import.meta.url), 'utf8');
const ansi = await readFile(new URL('./public/ansi.js', import.meta.url), 'utf8');
const settle = () => new Promise(resolve => setImmediate(resolve));

async function page({ confirmed = true, job = null, services = [], links = { publicUrl: 'https://auth.example:8088', mailboxUrl: 'http://127.0.0.1:18025' } } = {}) {
  const elements = [], timers = new Map(), calls = [], confirmations = [], actions = [];
  let nextTimer = 0, active = 0, maxActive = 0;
  function element() {
    let text = '';
    const node = { value: '', checked: false, disabled: false, dataset: {}, children: [], events: {}, style: {},
      scrollTop: 0, scrollLeft: 0, scrollHeight: 100, clientHeight: 100,
      get textContent() { return text + this.children.map(child => child.textContent).join(''); },
      set textContent(value) { text = value; this.children = []; },
      append(...children) { this.children.push(...children.flatMap(child => child.fragment ? child.children : [child])); },
      replaceChildren(...children) { this.textContent = ''; this.append(...children); },
      addEventListener(type, callback) { this.events[type] = callback; },
      emit(type, event) { return this.events[type](event); },
      setAttribute(key, value) { this[key] = value; },
      removeAttribute(key) { delete this[key]; },
      focus() { this.focused = true; },
    };
    elements.push(node); return node;
  }
  const ids = new Map([...html.matchAll(/id="([^"]+)"/g)].map(([, id]) => [id, element()]));
  const groupButtons = [...html.matchAll(/<button data-action="([^"]+)" data-target="([^"]+)"[^>]*>([^<]+)<\/button>/g)].map(([, action, target, label]) => {
    const button = element(); button.dataset = { action, target }; button.textContent = label; return button;
  });
  const get = id => {
    const node = ids.get(id) || elements.find(node => node.id === id);
    assert.ok(node, `known HTML id ${id}`); return node;
  };
  get('log-tail').value = '200';
  runInNewContext(ansi + '\n' + script, {
    document: { getElementById: get, createElement: element,
      createDocumentFragment() { return Object.assign(element(), { fragment: true }); },
      querySelectorAll: () => elements.filter(node => node.dataset.action) },
    AbortSignal: { timeout: () => undefined },
    setTimeout(callback, delay) { const id = ++nextTimer; timers.set(id, { callback, delay }); return id; },
    clearTimeout: id => timers.delete(id), setInterval: () => {},
    confirm(message) { confirmations.push(message); return confirmed; },
    fetch(url, options) {
      if (url === '/api/actions') {
        actions.push({ ...JSON.parse(options.body), headers: { ...options.headers } });
        return Promise.resolve({ ok: true, json: async () => ({ job: null }) });
      }
      if (!url.startsWith('/api/logs?')) {
        if (url === '/api/links' && links instanceof Error) return Promise.reject(links);
        const data = { '/api/session': { token: 'fixture' }, '/api/status': { services }, '/api/job': { job }, '/api/links': links }[url];
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
  return { get, calls, groupButtons, confirmations, actions, logTimers, maxActive: () => maxActive,
    async tickLogs() {
      const [[id, timer]] = logTimers(); timers.delete(id); void timer.callback(); await settle();
    },
  };
}

test('all twelve group controls submit selected actions with accurate confirmations', async () => {
  const p = await page();
  assert.equal(p.groupButtons.length, 12);
  assert.match(html, /Start also starts backend dependencies via Caddy/);
  for (const target of ['backend', 'frontend', 'infrastructure', 'all']) {
    assert.match(html, new RegExp(`id="group-${target}">`));
    for (const action of ['start', 'stop', 'restart']) {
      const buttons = p.groupButtons.filter(button => button.dataset.target === target && button.dataset.action === action);
      assert.equal(buttons.length, 1);
      assert.equal(buttons[0].textContent, action[0].toUpperCase() + action.slice(1));
      assert.equal(buttons[0].disabled, false);
      const before = p.confirmations.length;
      await buttons[0].emit('click');
      assert.deepEqual(p.actions.at(-1), { action, target, headers: { 'Content-Type': 'application/json', 'X-CSRF-Token': 'fixture' } });
      if (action === 'start' && target !== 'frontend') {
        assert.equal(p.confirmations.length, before); continue;
      }
      assert.equal(p.confirmations.length, before + 1);
      const message = p.confirmations.at(-1);
      assert.ok(message.startsWith(`${action} ${target} (`));
      if (target === 'frontend') {
        assert.doesNotMatch(message, /PostgreSQL will be interrupted/);
        if (action === 'start') assert.match(message, /WARNING:.*backend dependencies.*api, postgres, redis, signer and migrate/);
        else assert.match(message, /Only web and caddy are affected; backend services will not be stopped or restarted/);
      } else {
        assert.match(message, /WARNING: PostgreSQL will be interrupted/);
        if (target === 'infrastructure') assert.match(message, /\(postgres, redis, mailpit\)/);
        if (action === 'restart') assert.doesNotMatch(message, /migrate/);
        else if (target !== 'infrastructure') assert.match(message, /, migrate\)/);
      }
    }
  }
  assert.equal(p.actions.length, 12);
});

test('navigation maps external links, warns without disabling stopped services, and closes on Escape', async () => {
  const p = await page();
  assert.match(html, /<a href="\/" class="brand">/);
  assert.match(html, /<details id="nav-api"><summary id="nav-api-summary">API<\/summary>/);
  assert.match(html, /Swagger \(partial: activation \+ health\)/);
  for (const [name, path] of Object.entries({ frontend: '/login', dashboard: '/app', admin: '/admin/users', swagger: '/docs', openapi: '/api-docs/openapi.json', health: '/health/ready' })) {
    const node = p.get(`nav-${name}`);
    assert.equal(node.href, `https://auth.example:8088${path}`);
    assert.equal(node.target, '_blank'); assert.equal(node.rel, 'noopener noreferrer');
    assert.equal(node['aria-disabled'], 'false');
  }
  assert.equal(p.get('nav-mailbox').href, 'http://127.0.0.1:18025');
  assert.equal(p.get('nav-mailbox').rel, 'noopener noreferrer');
  assert.match(p.get('nav-warning').textContent, /caddy not running; mailpit not running/);
  p.get('nav-api').open = true;
  p.get('nav-api').emit('keydown', { key: 'Enter' });
  assert.equal(p.get('nav-api').open, true);
  p.get('nav-api').emit('keydown', { key: 'Escape' });
  assert.equal(p.get('nav-api').open, false);
  assert.equal(p.get('nav-api-summary').focused, true);
  assert.equal(p.actions.length, 0);
});

test('navigation failures remove stale hrefs; Refresh status retries links', async () => {
  const links = { publicUrl: null, mailboxUrl: null };
  const p = await page({ links, services: ['caddy', 'mailpit'].map(service => ({ service, containers: [{ state: 'running', health: 'healthy' }] })) });
  assert.equal(p.get('nav-warning').textContent, '');
  assert.match(p.get('nav-note').textContent, /Application links unavailable; Mailbox link unavailable/);
  links.publicUrl = 'http://localhost:8088';
  await p.get('refresh').emit('click'); await settle();
  assert.equal(p.get('nav-frontend').href, 'http://localhost:8088/login');
  links.publicUrl = null;
  await p.get('refresh').emit('click'); await settle();
  assert.equal(p.get('nav-frontend').href, undefined);
  const failed = await page({ links: new Error('config failure') });
  assert.match(failed.get('nav-note').textContent, /Links unavailable.*Refresh status/);
  for (const name of ['frontend', 'dashboard', 'admin', 'swagger', 'openapi', 'health', 'mailbox']) {
    assert.equal(failed.get(`nav-${name}`).href, undefined);
    assert.equal(failed.get(`nav-${name}`)['aria-disabled'], 'true');
  }
});

test('declining group confirmations sends no mutations', async () => {
  const p = await page({ confirmed: false });
  for (const button of p.groupButtons.filter(button => button.dataset.action !== 'start' || button.dataset.target === 'frontend')) {
    await button.emit('click');
  }
  assert.equal(p.confirmations.length, 9);
  assert.equal(p.actions.length, 0);
});

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

test('both output panes render ANSI snapshots safely and logs skip unchanged snapshots', async () => {
  const p = await page({ job: { state: 'succeeded', action: 'stop', target: 'api', startedAt: '2026-01-01',
    output: '\x1b[31m<img src=x>' } });
  assert.equal(p.get('job-output').children[0].style.color, '#aa0000');
  assert.equal(p.get('job-output').children[0].textContent, '<img src=x>');
  assert.deepEqual(p.get('job-output').children.at(-1).style, {});
  for (let i = 0; i < 2; i++) {
    void p.get('logs-refresh').emit('click');
    p.calls[i].respond({ output: '\x1b[32mservice\x1b[0m' }); await settle();
    if (i === 0) p.get('log-output').children[0].marker = true;
  }
  assert.equal(p.get('log-output').textContent, 'service');
  assert.equal(p.get('log-output').children[0].style.color, '#00aa00');
  assert.equal(p.get('log-output').children[0].marker, true);
});
