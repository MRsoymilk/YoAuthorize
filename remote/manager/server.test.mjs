import { test } from 'node:test';
import assert from 'node:assert/strict';
import http from 'node:http';
import { EventEmitter } from 'node:events';
import { PassThrough } from 'node:stream';
import { mkdtemp, mkdir, writeFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { DEPLOY, SERVICES, VISIBLE, OUTPUT_LIMIT, actionArgs, composeArgs, parsePort,
  parseStatus, validateRequest, checkStartFiles, createManager, createServer, execute } from './server.mjs';

const ok = (stdout = '') => ({ code: 0, stdout, stderr: '', truncated: false });
const deferred = () => { let resolve; const promise = new Promise(r => { resolve = r; }); return { promise, resolve }; };
async function finished(manager) {
  for (let i = 0; i < 100 && ['running', 'checking'].includes(manager.job?.state); i++) {
    await new Promise(resolve => setTimeout(resolve, 1));
  }
  assert.ok(!['running', 'checking'].includes(manager.job?.state), 'job finished');
  return manager.job;
}

test('whitelist, dependency-preserving starts and explicit non-destructive groups', () => {
  assert.equal(SERVICES.length, 7);
  for (const name of SERVICES) {
    assert.deepEqual(actionArgs('start', name), ['up', '-d', name]);
    assert.deepEqual(actionArgs('stop', name), ['stop', name]);
    assert.deepEqual(actionArgs('restart', name), ['restart', '--no-deps', name]);
  }
  assert.deepEqual(actionArgs('start', 'backend'), ['up', '-d', 'api', 'mailpit']);
  assert.deepEqual(actionArgs('start', 'all'), ['up', '-d', ...SERVICES]);
  assert.deepEqual(actionArgs('stop', 'all'), ['stop', ...SERVICES, 'migrate']);
  assert.deepEqual(actionArgs('stop', 'backend'), ['stop', 'postgres', 'redis', 'mailpit', 'signer', 'api', 'migrate']);
  assert.deepEqual(actionArgs('restart', 'backend'), ['restart', '--no-deps', 'postgres', 'redis', 'mailpit', 'signer', 'api']);
  assert.deepEqual(actionArgs('restart', 'all'), ['restart', '--no-deps', ...SERVICES]);
  for (const [group, names] of [['frontend', ['web', 'caddy']], ['infrastructure', ['postgres', 'redis', 'mailpit']]]) {
    assert.deepEqual(actionArgs('start', group), ['up', '-d', ...names]);
    assert.deepEqual(actionArgs('stop', group), ['stop', ...names]);
    assert.deepEqual(actionArgs('restart', group), ['restart', '--no-deps', ...names]);
  }
  assert.deepEqual(actionArgs('start', 'migrate'), ['up', '-d', 'migrate']);
  assert.ok(!actionArgs('restart', 'all').includes('migrate'));
  for (const name of ['bootstrap-admin', '--help', 'api; touch /tmp/pwn', '../api', 'toString', {}, null]) {
    assert.throws(() => actionArgs('start', name), { status: 400 });
  }
  for (const action of ['down', 'prune', 'exec', 'build', {}, null]) assert.throws(() => actionArgs(action, 'all'));
  assert.throws(() => actionArgs('restart', 'migrate'));
  assert.throws(() => actionArgs('stop', 'migrate'));
});

test('paths are module-relative and safe for a checkout with spaces; port validation', () => {
  assert.equal(DEPLOY, fileURLToPath(new URL('../deploy/', import.meta.url)));
  const deploy = '/tmp/a checkout/remote/deploy';
  assert.deepEqual(composeArgs(['up', '-d', 'api'], deploy), ['compose', '--project-directory', deploy,
    '--env-file', `${deploy}/.env`, '-p', 'yoauthorize', '-f', `${deploy}/compose.yaml`, 'up', '-d', 'api']);
  assert.equal(parsePort(), 5002);
  assert.equal(parsePort('65535'), 65535);
  for (const value of ['', '0', '65536', '-1', '1.5', ' 5002', '5e3', '5002abc']) assert.throws(() => parsePort(value));
});

test('status accepts arrays, JSON lines and empty stacks; completed migration is not a failure', () => {
  const rows = [{ Service: 'api', Name: 'api-1', State: 'running', Health: 'starting' },
    { Service: 'migrate', State: 'exited', ExitCode: 0 }];
  const array = parseStatus(JSON.stringify(rows));
  assert.deepEqual(parseStatus(rows.map(row => JSON.stringify(row)).join('\r\n')), array);
  assert.equal(array.length, 8);
  assert.equal(array.find(s => s.service === 'postgres').containers.length, 0);
  assert.equal(array.find(s => s.service === 'migrate').containers[0].completed, true);
  assert.equal(parseStatus('{"Service":"migrate","State":"exited","ExitCode":1}').at(-1).containers[0].completed, false);
  assert.ok(parseStatus('').every(s => s.containers.length === 0));
  for (const value of ['broken', '[null]', '{}', '{"Service":"api"}', '[{}]']) assert.throws(() => parseStatus(value), { status: 502 });
});

test('Host, Origin, fetch metadata, token and JSON restrictions', () => {
  const request = (headers = {}, method = 'GET') => ({ method, headers: { host: '127.0.0.1:5002', ...headers } });
  validateRequest(request(), 5002, 'secret');
  validateRequest(request({ host: 'localhost:5002', origin: 'http://localhost:5002' }), 5002, 'secret');
  validateRequest(request({ host: 'localhost', origin: 'http://localhost' }), 80, 'secret');
  validateRequest(request({ host: 'localhost:80', origin: 'http://localhost' }), 80, 'secret');
  for (const headers of [{ host: 'evil.test:5002' }, { host: '127.0.0.1:5003' }, { host: 'localhost.evil:5002' },
    { host: '127.1:5002' }, { origin: 'null' }, { origin: 'https://127.0.0.1:5002' },
    { origin: 'http://evil.test' }, { 'sec-fetch-site': 'cross-site' }, { 'sec-fetch-site': 'same-site' }]) {
    assert.throws(() => validateRequest(request(headers), 5002, 'secret'), { status: 403 });
  }
  const headers = { origin: 'http://127.0.0.1:5002', 'x-csrf-token': 'secret', 'content-type': 'application/json' };
  validateRequest(request(headers, 'POST'), 5002, 'secret');
  for (const key of ['origin', 'x-csrf-token']) {
    const copy = { ...headers }; delete copy[key];
    assert.throws(() => validateRequest(request(copy, 'POST'), 5002, 'secret'), { status: 403 });
  }
  assert.throws(() => validateRequest(request({ ...headers, 'content-type': 'text/plain' }, 'POST'), 5002, 'secret'), { status: 415 });
});

test('start preflight reports files without generating secrets', async t => {
  const dir = await mkdtemp(path.join(tmpdir(), 'yoauthorize-manager-'));
  t.after(() => rm(dir, { recursive: true, force: true }));
  await assert.rejects(checkStartFiles('all', dir), error => error.status === 422 && error.message.includes('Nothing was initialized'));
  await mkdir(path.join(dir, 'secrets'));
  for (const name of ['.env', 'compose.yaml', 'compose.backend.yaml', 'compose.infrastructure.yaml', 'compose.web.yaml', 'Caddyfile',
    'secrets/postgres-password', 'secrets/database-url', 'secrets/activation-pepper', 'secrets/signer-shared-secret', 'secrets/license-signing-key']) {
    await writeFile(path.join(dir, name), name === '.env' ? '' : 'test fixture');
  }
  await checkStartFiles('all', dir);
  await writeFile(path.join(dir, 'secrets/database-url'), '');
  await assert.rejects(checkStartFiles('all', dir), /database-url/);
});

test('preflight requires only target/dependency secrets and Caddyfile only when needed', async t => {
  const business = ['postgres-password', 'database-url', 'activation-pepper', 'signer-shared-secret', 'license-signing-key'];
  const targets = {
    redis: [], mailpit: [], web: [], postgres: ['postgres-password'],
    migrate: ['postgres-password', 'database-url'], signer: ['license-signing-key', 'signer-shared-secret'],
    api: business, backend: business, frontend: business, infrastructure: ['postgres-password'], all: business, caddy: business,
  };
  for (const [target, secrets] of Object.entries(targets)) await t.test(target, async t => {
    const dir = await mkdtemp(path.join(tmpdir(), 'yoauthorize-preflight-'));
    t.after(() => rm(dir, { recursive: true, force: true }));
    const common = ['.env', 'compose.yaml', 'compose.backend.yaml', 'compose.infrastructure.yaml', 'compose.web.yaml'];
    for (const file of common) await writeFile(path.join(dir, file), file === '.env' ? '' : 'fixture');
    const required = secrets.map(name => `secrets/${name}`);
    if (['caddy', 'frontend', 'all'].includes(target)) required.push('Caddyfile');
    if (secrets.length) await mkdir(path.join(dir, 'secrets'));
    for (const file of required) await writeFile(path.join(dir, file), 'fixture');
    // No unrelated secrets or Caddyfile exist; secret-free targets have no secrets directory.
    await checkStartFiles(target, dir);
    for (const file of [...common, ...required]) {
      await rm(path.join(dir, file));
      await assert.rejects(checkStartFiles(target, dir), error => {
        assert.equal(error.status, 422);
        assert.equal(error.message.split('. See ')[0], `Missing, empty or unreadable deployment files: ${file}`);
        return true;
      });
      await writeFile(path.join(dir, file), 'fixture');
    }
    await assert.rejects(checkStartFiles('bootstrap-admin', dir), { status: 400 });
  });
});

test('single mutation slot covers preflight and command; progress is bounded and failure releases slot', async () => {
  const gate = deferred(), command = deferred();
  const calls = [];
  const manager = createManager({ preflight: target => { assert.equal(target, 'backend'); return gate.promise; }, run: async (args, options) => {
    calls.push(args);
    if (args[0] === 'network') return ok(args[1] === 'ls' ? 'dev-net\n' : 'bridge\n');
    options.onOutput(Buffer.alloc(OUTPUT_LIMIT + 100, 'x'));
    return command.promise;
  } });
  const pending = manager.submit('start', 'backend');
  assert.equal(manager.job.state, 'checking');
  await assert.rejects(manager.submit('stop', 'all'), { status: 409 });
  gate.resolve(); await pending;
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(manager.job.state, 'running');
  assert.equal(manager.job.output.length, OUTPUT_LIMIT);
  assert.equal(manager.job.truncated, true);
  await assert.rejects(manager.submit('restart', 'api'), { status: 409 });
  command.resolve({ ...ok(), code: 1, stderr: 'build failed' });
  assert.equal((await finished(manager)).state, 'failed');
  assert.match(manager.job.error, /build failed/);
  await manager.submit('stop', 'api'); await finished(manager);
  assert.deepEqual(calls.at(-1), composeArgs(['stop', 'api']));
});

test('preflight failure performs no Docker calls; wrong network driver prevents up', async () => {
  let calls = 0;
  const manager = createManager({ preflight: async () => { throw Object.assign(new Error('Restore secrets'), { status: 422 }); },
    run: async () => { calls++; return ok(); } });
  await assert.rejects(manager.submit('start', 'api'), { status: 422 });
  assert.equal(calls, 0);
  await manager.submit('stop', 'api'); await finished(manager);
  assert.equal(calls, 1);
  const argsSeen = [];
  const wrong = createManager({ preflight: async () => {}, run: async args => {
    argsSeen.push(args); return ok(args[1] === 'ls' ? 'dev-net\n' : 'overlay\n');
  } });
  await wrong.submit('start', 'api');
  assert.equal((await finished(wrong)).state, 'failed');
  assert.match(wrong.job.error, /bridge driver/);
  assert.equal(argsSeen.length, 2);
});

test('network creation mirrors helper only for start; timeout remains distinct from success', async () => {
  const calls = [];
  const manager = createManager({ preflight: async () => {}, run: async args => { calls.push(args); return ok(); } });
  assert.equal(calls.length, 0);
  await manager.submit('start', 'all'); await finished(manager);
  assert.deepEqual(calls[1], ['network', 'create', '--driver', 'bridge', '--opt', 'com.docker.network.bridge.name=br-docker', 'dev-net']);
  assert.deepEqual(calls[2], composeArgs(['up', '-d', ...SERVICES]));
  const timeout = createManager({ run: async () => ({ ...ok(), code: null, timedOut: true }) });
  await timeout.submit('stop', 'api');
  assert.equal((await finished(timeout)).state, 'timed_out');
  assert.match(timeout.job.error, /daemon may still be working/);
});

test('logs/status errors are honest; tail and output are bounded; reads have a concurrency cap', async () => {
  const calls = [];
  const manager = createManager({ run: async args => { calls.push(args); return ok('hello'); } });
  assert.equal((await manager.logs('api', '200')).output, 'hello');
  assert.deepEqual(calls[0], composeArgs(['logs', '--no-color', '--timestamps', '--tail', '200', 'api']));
  for (const [service, tail] of [['bootstrap-admin', '200'], ['api', '0'], ['api', '1001'], ['api', '-1'], ['api', '2;id']]) {
    await assert.rejects(manager.logs(service, tail), { status: 400 });
  }
  const huge = createManager({ run: async () => ({ ...ok('x'.repeat(OUTPUT_LIMIT + 1)), truncated: true }) });
  assert.equal((await huge.logs('api', '1000')).output.length, OUTPUT_LIMIT);
  await assert.rejects(huge.status(), { status: 502 });
  const broken = createManager({ run: async () => ({ ...ok(), code: 1, stderr: 'Cannot connect to Docker' }) });
  await assert.rejects(broken.status(), /Cannot connect/);
  await assert.rejects(broken.logs('api', '1'), /Cannot connect/);
  const gate = deferred();
  const limited = createManager({ run: () => gate.promise });
  const reads = Array.from({ length: 4 }, () => limited.status());
  await assert.rejects(limited.status(), { status: 429 });
  gate.resolve(ok('[]')); await Promise.all(reads);
});

test('child execution uses no shell, drains bounded output, reports spawn errors and kills on timeout', async () => {
  function fakeSpawn(onSpawn) {
    return (command, args, options) => {
      assert.equal(command, 'docker'); assert.equal(options.shell, false); assert.equal(options.cwd, DEPLOY);
      assert.equal(options.env.COMPOSE_PROFILES, '');
      const child = new EventEmitter(); child.stdout = new PassThrough(); child.stderr = new PassThrough();
      child.kill = signal => { child.emit('close', null, signal); return true; };
      setImmediate(() => onSpawn(child, args)); return child;
    };
  }
  const captured = await execute(['test'], { spawnChild: fakeSpawn(child => {
    child.stdout.write(Buffer.alloc(OUTPUT_LIMIT * 2, 'a')); child.stderr.write('diagnostic'); child.emit('close', 0, null);
  }) });
  assert.equal(captured.stdout.length, OUTPUT_LIMIT); assert.equal(captured.truncated, true);
  assert.equal(captured.stderr, 'diagnostic');
  const error = await execute([], { spawnChild: fakeSpawn(child => {
    child.emit('error', new Error('ENOENT')); child.emit('close', -2, null);
  }) });
  assert.equal(error.error, 'ENOENT');
  const timed = await execute([], { timeout: 5, spawnChild: fakeSpawn(() => {}) });
  assert.equal(timed.timedOut, true); assert.equal(timed.signal, 'SIGTERM');
});

test('HTTP smoke: explicit assets, security, body limits, logs and concurrent action polling', async t => {
  const gate = deferred();
  const manager = createManager({ run: args => args.includes('stop') ? gate.promise : Promise.resolve(ok('[]')) });
  const server = createServer({ manager, port: () => server.address().port });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  t.after(() => { gate.resolve(ok()); server.closeAllConnections(); return new Promise(resolve => server.close(resolve)); });
  const port = server.address().port;
  const request = (url, { method = 'GET', headers = {}, body } = {}) => new Promise((resolve, reject) => {
    const req = http.request({ hostname: '127.0.0.1', port, path: url, method, headers }, res => {
      let text = ''; res.setEncoding('utf8'); res.on('data', chunk => { text += chunk; });
      res.on('end', () => resolve({ status: res.statusCode, text, headers: res.headers }));
    });
    req.on('error', reject); req.end(body);
  });
  for (const asset of ['/', '/app.js', '/styles.css']) assert.equal((await request(asset)).status, 200);
  const logo = await fetch(`http://127.0.0.1:${port}/logo.png`);
  assert.equal(logo.status, 200);
  assert.equal(logo.headers.get('content-type'), 'image/png');
  assert.match(logo.headers.get('content-security-policy'), /img-src 'self'/);
  const bytes = Buffer.from(await logo.arrayBuffer());
  assert.deepEqual([...bytes.subarray(0, 8)], [137, 80, 78, 71, 13, 10, 26, 10]);
  for (const url of ['/server.mjs', '/.env', '/../deploy/.env', '/%2e%2e/deploy/secrets/database-url', '//evil.test', '/public/index.html', '/res/logo.png', '/public/logo.png']) {
    assert.ok([400, 404].includes((await request(url)).status));
  }
  assert.equal((await request('/api/session', { headers: { Host: 'evil.test' } })).status, 403);
  assert.equal((await request('/api/status', { headers: { Origin: 'http://evil.test' } })).status, 403);
  const session = await request('/api/session');
  assert.equal(session.headers['access-control-allow-origin'], undefined);
  assert.match(session.headers['content-security-policy'], /frame-ancestors 'none'/);
  const token = JSON.parse(session.text).token;
  const headers = { Origin: `http://127.0.0.1:${port}`, 'Content-Type': 'application/json', 'X-CSRF-Token': token };
  const post = body => request('/api/actions', { method: 'POST', headers, body });
  assert.equal((await request('/api/actions', { method: 'POST', body: '{}' })).status, 403);
  assert.equal((await post('{')).status, 400);
  assert.equal((await post(JSON.stringify({ action: 'stop', target: 'api', command: 'id' }))).status, 400);
  assert.equal((await post('x'.repeat(5000))).status, 413);
  assert.equal((await request('/api/actions', { method: 'POST', headers: { ...headers, 'Transfer-Encoding': 'chunked' }, body: 'x'.repeat(5000) })).status, 413);
  assert.equal((await request('/api/status')).status, 200);
  assert.equal(JSON.parse((await request('/api/status')).text).services.length, VISIBLE.length);
  assert.equal((await request('/api/logs?service=api&tail=1')).status, 200);
  assert.equal((await request('/api/logs?service=api&tail=2000')).status, 400);
  assert.equal((await request('/api/logs?service=api&service=postgres')).status, 400);
  assert.equal((await post('{"action":"stop","target":"api"}')).status, 202);
  assert.equal((await post('{"action":"stop","target":"all"}')).status, 409);
  assert.equal(JSON.parse((await request('/api/job')).text).job.state, 'running');
  gate.resolve(ok()); await finished(manager);
  assert.equal(JSON.parse((await request('/api/job')).text).job.state, 'succeeded');
});
