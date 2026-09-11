/**
 * Optional Docker runtime test: node remote/deploy/test-caddy-ports.mjs --docker
 * Requires local Docker, the existing caddy:2.10-alpine image, curl, and dev-net
 * with a ready yoauthorize-api:8080. Only GET /health/ready touches the app.
 * Creates two dedicated containers, loopback-only random high ports, and a
 * temporary production fixture with internal TLS (never public ACME).
 * No Compose operations, image pulls, secrets, socket mounts, or repo writes.
 * SIGINT/SIGTERM and failures clean up test containers and fixtures in finally.
 * HTTPS uses curl --insecure only for this disposable internal-CA fixture;
 * Alt-Svc is checked, not an actual HTTP/3 client connection.
 */
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { randomUUID } from 'node:crypto';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { createServer } from 'node:net';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { setTimeout as delay } from 'node:timers/promises';

assert.deepEqual(process.argv.slice(2), ['--docker'],
  'Opt in with --docker; see the module header for requirements and safety.');
const deploy = fileURLToPath(new URL('.', import.meta.url));
const token = randomUUID();
const label = `yoauthorize.caddy-port-test=${token}`;
const names = [];
let fixture;
let interrupted = false;
const interrupt = () => { interrupted = true; };
process.on('SIGINT', interrupt);
process.on('SIGTERM', interrupt);

function run(binary, args, check = true) {
  const result = spawnSync(binary, args, { encoding: 'utf8', timeout: 30000 });
  if (check && (result.error || result.status !== 0)) {
    throw new Error(`${binary} ${args.join(' ')}: ${result.error ?? result.stderr}`);
  }
  return result;
}

async function freePort() {
  const server = createServer();
  await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', resolve);
  });
  const { port } = server.address();
  await new Promise((resolve, reject) => server.close(error => error ? reject(error) : resolve()));
  assert.ok(port > 1024);
  return port;
}

function start(kind, args, command = []) {
  assert.ok(!interrupted, 'Interrupted');
  const name = `yoauthorize-caddy-ports-${kind}-${token}`;
  names.push(name);
  run('docker', ['create', '--pull=never', '--name', name, '--label', label,
    '--read-only', '--cap-drop=ALL', '--cap-add=NET_BIND_SERVICE',
    '--security-opt=no-new-privileges',
    // Docker client proxy defaults must not redirect local upstream requests.
    ...['HTTP_PROXY', 'HTTPS_PROXY', 'ALL_PROXY', 'http_proxy', 'https_proxy', 'all_proxy']
      .flatMap(key => ['-e', `${key}=`]), '-e', 'NO_PROXY=*', '-e', 'no_proxy=*',
    '--tmpfs', '/data', '--tmpfs', '/config', '--tmpfs', '/tmp',
    ...args, 'caddy:2.10-alpine', ...command]);
  run('docker', ['start', name]);
  const info = JSON.parse(run('docker', ['inspect', name]).stdout)[0];
  for (const bindings of Object.values(info.NetworkSettings.Ports)) {
    for (const binding of bindings ?? []) assert.equal(binding.HostIp, '127.0.0.1');
  }
  return name;
}

async function fetchResponse(url, insecure = false) {
  let last;
  for (let attempt = 0; attempt < 60; attempt++) {
    assert.ok(!interrupted, 'Interrupted');
    last = run('curl', ['--silent', '--show-error', '--noproxy', '*',
      '--connect-timeout', '1', '--max-time', '2', '--http1.1',
      '--resolve', `localhost:${new URL(url).port}:127.0.0.1`,
      ...(insecure ? ['--insecure'] : []), '--include', url], false);
    if (last.status === 0 && !/^HTTP\/1\.1 5\d\d/.test(last.stdout)) break;
    await delay(250);
  }
  assert.equal(last.status, 0, last.stderr);
  const split = last.stdout.indexOf('\r\n\r\n');
  assert.ok(split >= 0, 'Missing HTTP response headers');
  const [status, ...lines] = last.stdout.slice(0, split).split('\r\n');
  const headers = new Map(lines.map(line => {
    const colon = line.indexOf(':');
    return [line.slice(0, colon).toLowerCase(), line.slice(colon + 1).trim()];
  }));
  return { status, headers, body: last.stdout.slice(split + 4) };
}

try {
  run('docker', ['image', 'inspect', 'caddy:2.10-alpine']);
  run('docker', ['network', 'inspect', 'dev-net']);
  run('curl', ['--version']);
  const ports = new Set();
  while (ports.size < 3) ports.add(await freePort());
  const [devPort, httpPort, httpsPort] = ports;
  const mount = (source, target) => ['--mount', `type=bind,src=${source},dst=${target},readonly`];
  start('dev', ['--network', 'dev-net', '-p', `127.0.0.1:${devPort}:8088`,
    ...mount(join(deploy, 'Caddyfile'), '/etc/caddy/Caddyfile'),
    ...mount(join(deploy, 'Caddy.routes'), '/etc/caddy/Caddy.routes')]);
  const ready = await fetchResponse(`http://localhost:${devPort}/health/ready`);
  assert.equal(ready.status, 'HTTP/1.1 200 OK');
  assert.ok(ready.body.length > 0, 'Empty readiness response');
  console.log(`PASS dev localhost:${devPort} -> :8088 GET /health/ready: ${ready.status}; body=${ready.body.trim()}`);

  fixture = await mkdtemp(join(tmpdir(), 'yoauthorize-caddy-ports-'));
  const production = await readFile(join(deploy, 'Caddyfile.production'), 'utf8');
  const site = '{$CADDY_SITE_ADDRESS} {';
  assert.equal(production.split(site).length, 2, 'Expected exactly one production TLS site');
  await writeFile(join(fixture, 'Caddyfile'), production.replace(site, `${site}\n  tls internal`));
  await writeFile(join(fixture, 'Caddy.routes'), 'respond "custom-port fixture okay" 200\n');
  const publicUrl = `https://localhost:${httpsPort}`;
  const prodName = start('prod', ['--network', 'bridge',
    '-p', `127.0.0.1:${httpPort}:80`, '-p', `127.0.0.1:${httpsPort}:443`,
    '-p', `127.0.0.1:${httpsPort}:443/udp`, '-e', `PUBLIC_URL=${publicUrl}`,
    ...mount(join(fixture, 'Caddyfile'), '/etc/caddy/Caddyfile'),
    ...mount(join(fixture, 'Caddy.routes'), '/etc/caddy/Caddy.routes'),
    ...mount(join(deploy, 'start-caddy-production.sh'), '/etc/caddy/start-production.sh')],
  ['sh', '/etc/caddy/start-production.sh']);
  const uri = '/custom/path%20segment?one=1&next=%2Fdocs%3Fx%3D2';
  const redirect = await fetchResponse(`http://localhost:${httpPort}${uri}`);
  assert.equal(redirect.status, 'HTTP/1.1 301 Moved Permanently');
  assert.equal(redirect.headers.get('location'), `${publicUrl}${uri}`);
  console.log(`PASS prod HTTP localhost:${httpPort} -> :80: ${redirect.status}; Location=${redirect.headers.get('location')}`);
  const secure = await fetchResponse(`${publicUrl}${uri}`, true);
  assert.equal(secure.status, 'HTTP/1.1 200 OK');
  assert.equal(secure.headers.get('alt-svc'), `h3=":${httpsPort}"; ma=2592000`);
  assert.equal(secure.body, 'custom-port fixture okay');
  // Read only this test container's PID 1 environment to verify launcher derivation.
  const environment = run('docker', ['exec', prodName, 'cat', '/proc/1/environ']).stdout.split('\0');
  assert.ok(environment.includes('CADDY_SITE_ADDRESS=https://localhost:443'));
  assert.ok(environment.includes(`CADDY_PUBLIC_HTTPS_PORT=${httpsPort}`));
  console.log(`PASS launcher derived CADDY_SITE_ADDRESS=https://localhost:443 CADDY_PUBLIC_HTTPS_PORT=${httpsPort}`);
  console.log(`PASS prod HTTPS localhost:${httpsPort} -> :443: ${secure.status}; Alt-Svc=${secure.headers.get('alt-svc')}; body=${secure.body}`);
} catch (error) {
  for (const name of names) {
    const logs = run('docker', ['logs', '--tail', '40', name], false);
    console.error(`Test container ${name}:\n${logs.stdout}${logs.stderr}`);
  }
  throw error;
} finally {
  const errors = [];
  for (const name of names.reverse()) {
    const inspected = run('docker', ['inspect', name], false);
    if (inspected.status !== 0 && inspected.stderr?.includes('No such object')) continue;
    try {
      assert.equal(inspected.status, 0, inspected.stderr);
      const info = JSON.parse(inspected.stdout)[0];
      assert.equal(info.Config.Labels['yoauthorize.caddy-port-test'], token);
      run('docker', ['rm', '--force', '--volumes', name]);
      console.log(`CLEANUP removed ${name}`);
    } catch (error) { errors.push(error); }
  }
  if (fixture) {
    await rm(fixture, { recursive: true, force: true });
    console.log('CLEANUP removed temporary production fixture');
  }
  process.off('SIGINT', interrupt);
  process.off('SIGTERM', interrupt);
  assert.equal(errors.length, 0, errors.map(String).join('\n'));
}
