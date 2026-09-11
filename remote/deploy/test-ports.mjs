import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { copyFileSync, existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { test } from 'node:test';
import { fileURLToPath } from 'node:url';

const source = path.dirname(fileURLToPath(import.meta.url));
const defaults = {
  YOAUTHORIZE_HTTP_PORT: '8088', MAILPIT_HTTP_PORT: '8025', MANAGER_PORT: '5002',
  FRONTEND_DEV_PORT: '5173', FRONTEND_API_TARGET: 'http://localhost:8088',
  YOAUTHORIZE_PRODUCTION_HTTP_PORT: '80', YOAUTHORIZE_PRODUCTION_HTTPS_PORT: '443',
};
const custom = {
  YOAUTHORIZE_HTTP_PORT: '18088', MAILPIT_HTTP_PORT: '18025', MANAGER_PORT: '15002',
  FRONTEND_DEV_PORT: '15173', YOAUTHORIZE_PRODUCTION_HTTP_PORT: '10080',
  YOAUTHORIZE_PRODUCTION_HTTPS_PORT: '10443',
};

function fixture(t, files) {
  const root = mkdtempSync(path.join(tmpdir(), 'yoauthorize ports '));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const deploy = path.join(root, 'checkout with spaces', 'remote', 'deploy');
  mkdirSync(deploy, { recursive: true });
  // Never copy the checkout's .env or secrets.
  for (const file of files) copyFileSync(path.join(source, file), path.join(deploy, file));
  return deploy;
}

function run(command, args, deploy, extraEnv = {}) {
  const result = spawnSync(command, args, {
    cwd: deploy,
    // Do not let inherited Compose or application settings override the fixture.
    env: { PATH: process.env.PATH, HOME: path.dirname(deploy), ...extraEnv },
    encoding: 'utf8', timeout: 30_000,
  });
  assert.ifError(result.error);
  return result;
}

function compose(deploy, production) {
  const args = ['compose', '--project-directory', deploy, '--env-file', path.join(deploy, '.env'),
    '-f', path.join(deploy, 'compose.yaml')];
  if (production) args.push('-f', path.join(deploy, 'compose.production.yaml'));
  args.push('--profile', 'tools', 'config', '--format', 'json');
  const result = run('docker', args, deploy);
  assert.equal(result.status, 0, result.stderr);
  return JSON.parse(result.stdout);
}

test('Compose custom host ports preserve internal targets and authoritative public origins', async t => {
  const deploy = fixture(t, ['compose.yaml', 'compose.backend.yaml', 'compose.infrastructure.yaml',
    'compose.web.yaml', 'compose.production.yaml', 'Caddyfile', 'Caddyfile.production',
    'Caddy.routes', 'start-caddy-production.sh']);
  for (const publicUrl of [undefined, 'https://licenses.example.test:19443']) {
    writeFileSync(path.join(deploy, '.env'), Object.entries({ ...custom, ...(publicUrl ? { PUBLIC_URL: publicUrl } : {}) })
      .map(([key, value]) => `${key}=${value}\n`).join(''));
    for (const production of [false, true]) await t.test(`${production ? 'production' : 'development'} ${publicUrl ?? 'derived origin'}`, () => {
      const { services } = compose(deploy, production);
      const origin = publicUrl ?? 'http://localhost:18088';
      assert.equal(services.api.environment.PUBLIC_URL, origin);
      assert.equal(services.caddy.environment.PUBLIC_URL, origin);
      const ports = service => service.ports.map(({ host_ip, published, target, protocol }) =>
        ({ host_ip, published, target, protocol }));
      assert.deepEqual(ports(services.mailpit), [
        { host_ip: '127.0.0.1', published: '18025', target: 8025, protocol: 'tcp' },
      ]);
      assert.deepEqual(ports(services.caddy), production ? [
        { host_ip: undefined, published: '10080', target: 80, protocol: 'tcp' },
        { host_ip: undefined, published: '10443', target: 443, protocol: 'tcp' },
        { host_ip: undefined, published: '10443', target: 443, protocol: 'udp' },
      ] : [{ host_ip: '127.0.0.1', published: '18088', target: 8088, protocol: 'tcp' }]);
      assert.equal(services.api.environment.LISTEN_ADDR, '0.0.0.0:8080');
      assert.equal(services.api.environment.SIGNER_URL, 'http://signer:8090');
      assert.equal(services.api.environment.REDIS_URL, 'redis://redis:6379/');
      assert.equal(services.api.environment.SMTP_URL, 'smtp://yoauthorize-mailpit:1025');
      assert.equal(services.signer.environment.SIGNER_LISTEN_ADDR, '0.0.0.0:8090');
      for (const name of ['api', 'web', 'signer', 'postgres', 'redis']) assert.equal(services[name].ports, undefined);
      const binds = services.caddy.volumes.filter(volume => volume.type === 'bind');
      assert.deepEqual(binds.map(({ source, target, read_only }) => ({ source, target, read_only })), [
        { source: path.join(deploy, production ? 'Caddyfile.production' : 'Caddyfile'), target: '/etc/caddy/Caddyfile', read_only: true },
        { source: path.join(deploy, 'Caddy.routes'), target: '/etc/caddy/Caddy.routes', read_only: true },
        ...(production ? [{ source: path.join(deploy, 'start-caddy-production.sh'), target: '/etc/caddy/start-production.sh', read_only: true }] : []),
      ]);
      if (production) assert.deepEqual(services.caddy.command, ['sh', '/etc/caddy/start-production.sh']);
    });
  }
  assert.match(readFileSync(path.join(deploy, 'Caddyfile'), 'utf8'), /http:\/\/:8088\s*\{\s*import Caddy\.routes/);
  const production = readFileSync(path.join(deploy, 'Caddyfile.production'), 'utf8');
  assert.match(production, /auto_https disable_redirects/);
  assert.match(production, /http:\/\/:80\s*\{\s*redir \{\$PUBLIC_URL\}\{uri\} permanent/);
  assert.match(production, /\{\$CADDY_SITE_ADDRESS\}\s*\{[\s\S]*?import Caddy\.routes/);
  assert.match(production, /header Alt-Svc.*CADDY_PUBLIC_HTTPS_PORT:443/);
  const routes = readFileSync(path.join(deploy, 'Caddy.routes'), 'utf8');
  assert.deepEqual([...routes.matchAll(/reverse_proxy (\S+)/g)].map(match => match[1]),
    ['yoauthorize-api:8080', 'yoauthorize-api:8080', 'yoauthorize-api:8080', 'yoauthorize-api:8080', 'yoauthorize-web:80']);
  assert.equal(existsSync(path.join(deploy, 'secrets')), false);
});

for (const existing of [false, true]) test(`init-secrets ${existing ? 'preserves existing .env' : 'writes centralized defaults'} and refuses rerun`, t => {
  const deploy = fixture(t, ['init-secrets.sh']);
  const envFile = path.join(deploy, '.env');
  const previous = '# keep exactly\nYOAUTHORIZE_HTTP_PORT=18088\nPUBLIC_URL=https://licenses.example.test:19443\n';
  if (existing) writeFileSync(envFile, previous);
  const result = run('bash', [path.join(deploy, 'init-secrets.sh')], deploy);
  assert.equal(result.status, 0, 'fresh initialization succeeds');
  const env = readFileSync(envFile, 'utf8');
  if (existing) assert.equal(env, previous);
  else {
    const values = Object.fromEntries(env.trim().split('\n').map(line => line.split('=')));
    assert.deepEqual(values, { LOCAL_UID: String(process.getuid()), LOCAL_GID: String(process.getgid()), ...defaults });
    assert.equal(Object.hasOwn(values, 'PUBLIC_URL'), false);
  }
  const secretDir = path.join(deploy, 'secrets');
  const names = ['activation-pepper', 'bootstrap-admin-password', 'database-url', 'license-signing-key', 'postgres-password', 'signer-shared-secret'];
  assert.deepEqual(readdirSync(secretDir).sort(), names);
  assert.equal(statSync(secretDir).mode & 0o777, 0o700);
  const secrets = names.map(name => readFileSync(path.join(secretDir, name)));
  const output = Buffer.from(result.stdout + result.stderr);
  for (const secret of secrets) {
    assert.ok(secret.length >= 32, 'fresh secret has expected minimum length');
    assert.ok(!output.includes(secret), 'initialization output must not reveal secrets');
  }
  assert.equal(secrets[3].length, 32, 'signing key contains 32 raw bytes');
  assert.ok(secrets[2].equals(Buffer.from(`postgres://yoauthorize:${secrets[4].toString()}@postgres:5432/yoauthorize`)), 'database URL uses generated password');
  const rerun = run('bash', [path.join(deploy, 'init-secrets.sh')], deploy);
  assert.equal(rerun.status, 1);
  assert.match(rerun.stderr, /Refusing to overwrite existing secrets directory/);
  assert.equal(readFileSync(envFile, 'utf8'), env);
  assert.deepEqual(readdirSync(secretDir).sort(), names);
  for (const [index, name] of names.entries()) {
    assert.ok(readFileSync(path.join(secretDir, name)).equals(secrets[index]), 'rerun preserves secret bytes');
    assert.ok(!Buffer.from(rerun.stdout + rerun.stderr).includes(secrets[index]), 'refusal output must not reveal secrets');
  }
});

test('production startup validates external HTTPS origins and fixes the Caddy listener at 443', async t => {
  const deploy = fixture(t, ['start-caddy-production.sh']);
  const bin = path.join(deploy, 'mock bin');
  mkdirSync(bin);
  const capture = path.join(deploy, 'caddy capture');
  writeFileSync(path.join(bin, 'caddy'), '#!/bin/sh\nprintf "%s\\n" "$CADDY_SITE_ADDRESS" "$PUBLIC_URL" "$CADDY_PUBLIC_HTTPS_PORT" "$@" > "$CAPTURE"\n', { mode: 0o755 });
  for (const publicUrl of ['https://licenses.example.test', 'https://licenses.example.test:19443/']) {
    const result = run('sh', [path.join(deploy, 'start-caddy-production.sh')], deploy,
      { PATH: `${bin}:${process.env.PATH}`, PUBLIC_URL: publicUrl, CAPTURE: capture });
    assert.equal(result.status, 0, result.stderr);
    assert.equal(readFileSync(capture, 'utf8'),
      `https://licenses.example.test:443\n${publicUrl.replace(/\/$/, '')}\n${new URL(publicUrl).port || '443'}\nrun\n--config\n/etc/caddy/Caddyfile\n--adapter\ncaddyfile\n`);
    rmSync(capture);
  }
  for (const publicUrl of ['', 'http://licenses.example.test', 'https://user:password@licenses.example.test',
    'https://user@licenses.example.test', 'https://licenses.example.test/path', 'https://licenses.example.test?query',
    'https://licenses.example.test/#fragment', 'https://[::1]:19443', 'https://::1',
    'https://licenses.example.test:', 'https://licenses.example.test:0', 'https://licenses.example.test:65536',
    'https://licenses.example.test:abc']) await t.test(`rejects ${publicUrl || 'empty origin'}`, () => {
    const result = run('sh', [path.join(deploy, 'start-caddy-production.sh')], deploy,
      { PATH: `${bin}:${process.env.PATH}`, PUBLIC_URL: publicUrl, CAPTURE: capture });
    assert.equal(result.status, 1);
    assert.match(result.stderr, /PUBLIC_URL/);
    assert.equal(existsSync(capture), false, 'invalid origin must not execute Caddy');
  });
});
