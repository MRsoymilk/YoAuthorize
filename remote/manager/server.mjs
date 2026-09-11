import http from 'node:http';
import { spawn } from 'node:child_process';
import { randomBytes } from 'node:crypto';
import { readFile, stat, access } from 'node:fs/promises';
import { constants } from 'node:fs';
import { fileURLToPath, pathToFileURL } from 'node:url';
import path from 'node:path';

export const DEPLOY = fileURLToPath(new URL('../deploy/', import.meta.url));
export const SERVICES = Object.freeze(['postgres', 'redis', 'mailpit', 'signer', 'api', 'web', 'caddy']);
export const VISIBLE = Object.freeze([...SERVICES, 'migrate']);
const BACKEND = ['postgres', 'redis', 'mailpit', 'signer', 'api', 'migrate'];
const FRONTEND = ['web', 'caddy'];
const INFRASTRUCTURE = ['postgres', 'redis', 'mailpit'];
export const OUTPUT_LIMIT = 256 * 1024;
const BODY_LIMIT = 4096;
const fail = (status, message) => Object.assign(new Error(message), { status });

export function parsePort(value = '5002') {
  if (!/^[1-9]\d{0,4}$/.test(String(value)) || Number(value) > 65535) {
    throw new Error('MANAGER_PORT must be an integer from 1 to 65535.');
  }
  return Number(value);
}

export function composeArgs(args, deploy = DEPLOY) {
  return ['compose', '--project-directory', deploy, '--env-file', path.join(deploy, '.env'),
    '-p', 'yoauthorize', '-f', path.join(deploy, 'compose.yaml'), ...args];
}

export function actionArgs(action, target) {
  if (!['start', 'stop', 'restart'].includes(action) ||
      ![...VISIBLE, 'backend', 'frontend', 'infrastructure', 'all'].includes(target) ||
      (target === 'migrate' && action !== 'start')) throw fail(400, 'Unsupported action or target.');
  if (action === 'start') {
    const names = target === 'backend' ? ['api', 'mailpit'] : target === 'frontend' ? FRONTEND :
      target === 'infrastructure' ? INFRASTRUCTURE : target === 'all' ? SERVICES : [target];
    return ['up', '-d', ...names];
  }
  const names = target === 'backend' ? BACKEND : target === 'frontend' ? FRONTEND :
    target === 'infrastructure' ? INFRASTRUCTURE : target === 'all' ? [...SERVICES, 'migrate'] : [target];
  return action === 'restart' ? ['restart', '--no-deps', ...names.filter(name => name !== 'migrate')]
    : ['stop', ...names];
}

// Keep only the last bytes, while draining both pipes so noisy children cannot block.
export function execute(args, { timeout = 20_000, color = false, onOutput = () => {}, spawnChild = spawn } = {}) {
  return new Promise(resolve => {
    let stdout = Buffer.alloc(0), stderr = Buffer.alloc(0), truncated = false, timedOut = false;
    const commandArgs = args[0] === 'compose' ? ['compose', '--ansi', color ? 'always' : 'never', ...args.slice(1)] : args;
    const child = spawnChild('docker', commandArgs, {
      cwd: DEPLOY, shell: false, stdio: ['ignore', 'pipe', 'pipe'],
      env: { ...process.env, YOAUTHORIZE_REMOTE_DIR: path.resolve(DEPLOY, '..'),
        COMPOSE_PROFILES: '', COMPOSE_ANSI: color ? 'always' : 'never', COMPOSE_PROGRESS: color ? 'auto' : 'plain' },
    });
    const append = (previous, chunk) => {
      const data = Buffer.concat([previous, chunk]);
      if (data.length > OUTPUT_LIMIT) truncated = true;
      return data.subarray(Math.max(0, data.length - OUTPUT_LIMIT));
    };
    child.stdout.on('data', chunk => { stdout = append(stdout, chunk); onOutput(chunk); });
    child.stderr.on('data', chunk => { stderr = append(stderr, chunk); onOutput(chunk); });
    let killTimer;
    const timer = setTimeout(() => {
      timedOut = true;
      killTimer = setTimeout(() => child.kill('SIGKILL'), 2000);
      child.kill('SIGTERM');
    }, timeout);
    let spawnError;
    child.on('error', error => { spawnError = error.message; });
    child.on('close', (code, signal) => {
      clearTimeout(timer);
      clearTimeout(killTimer);
      resolve({ code, signal, stdout: stdout.toString(), stderr: stderr.toString(), truncated,
        timedOut, error: spawnError });
    });
  });
}

function checked(result) {
  if (result.code !== 0 || result.timedOut || result.error) {
    throw fail(502, result.timedOut
      ? 'Docker command timed out. The daemon may still be working; inspect status before retrying.'
      : result.error || result.stderr.trim() || `Docker exited with code ${result.code} (${result.signal || 'no signal'}).`);
  }
  return result;
}

export function parseStatus(text) {
  let rows;
  try {
    const trimmed = text.trim();
    rows = !trimmed ? [] : trimmed.startsWith('[') ? JSON.parse(trimmed)
      : trimmed.split(/\r?\n/).filter(Boolean).map(line => JSON.parse(line));
    if (!Array.isArray(rows) || rows.some(row => !row || typeof row.Service !== 'string' || typeof row.State !== 'string')) {
      throw new Error('Unexpected fields');
    }
  } catch { throw fail(502, 'Docker Compose returned invalid status JSON. Upgrade/check Docker Compose v2.'); }
  return VISIBLE.map(service => ({ service, containers: rows.filter(row => row.Service === service).map(row => ({
    name: row.Name || service, state: row.State, health: row.Health || '',
    exitCode: row.ExitCode ?? null,
    completed: service === 'migrate' && row.State === 'exited' && String(row.ExitCode) === '0',
  })) }));
}

export async function checkStartFiles(target, deploy = DEPLOY) {
  actionArgs('start', target);
  // Include secrets mounted by dependencies, matching the development Compose fragments.
  const business = ['postgres-password', 'database-url', 'activation-pepper', 'signer-shared-secret', 'license-signing-key'];
  const secrets = {
    redis: [], mailpit: [], web: [], postgres: ['postgres-password'],
    migrate: ['postgres-password', 'database-url'], signer: ['license-signing-key', 'signer-shared-secret'],
    api: business, backend: business, frontend: business, infrastructure: ['postgres-password'], all: business, caddy: business,
  };
  const files = ['.env', 'compose.yaml', 'compose.backend.yaml', 'compose.infrastructure.yaml', 'compose.web.yaml',
    ...secrets[target].map(name => `secrets/${name}`)];
  if (['caddy', 'frontend', 'all'].includes(target)) files.push('Caddyfile');
  const missing = [];
  for (const file of files) {
    try {
      const location = path.join(deploy, file);
      await access(location, constants.R_OK);
      const info = await stat(location);
      if (!info.isFile() || (file !== '.env' && info.size === 0)) missing.push(file);
    } catch { missing.push(file); }
  }
  if (missing.length) throw fail(422, `Missing, empty or unreadable deployment files: ${missing.join(', ')}. ` +
    'See remote/deploy/README.md. Run bash remote/deploy/init-secrets.sh only for a fresh secrets directory; ' +
    'otherwise restore the missing files and check permissions. Review .env and LOCAL_UID/LOCAL_GID. Nothing was initialized.');
}

export function createManager({ run = execute, preflight = checkStartFiles } = {}) {
  let job = null, active = false, readers = 0;
  const docker = async (args, options) => checked(await run(args, options));
  async function read(args, options) {
    if (readers >= 4) throw fail(429, 'Too many Docker reads; retry shortly.');
    readers++;
    try { return await docker(composeArgs(args), options); } finally { readers--; }
  }
  return {
    get job() { return job; },
    async status() {
      const result = await read(['ps', '-a', '--format', 'json']);
      if (result.truncated) throw fail(502, 'Status output exceeded the capture limit.');
      return parseStatus(result.stdout);
    },
    async logs(service, tail) {
      if (!VISIBLE.includes(service) || !/^\d{1,4}$/.test(tail) || Number(tail) < 1 || Number(tail) > 1000) {
        throw fail(400, 'Choose an allowed service and a tail from 1 to 1000.');
      }
      const result = await read(['logs', '--timestamps', '--tail', String(Number(tail)), service], { color: true });
      const output = Buffer.from(result.stdout + result.stderr);
      return { output: output.subarray(Math.max(0, output.length - OUTPUT_LIMIT)).toString(),
        truncated: result.truncated || output.length > OUTPUT_LIMIT };
    },
    async submit(action, target) {
      const args = actionArgs(action, target);
      if (active) throw fail(409, 'Another mutation is in progress. Wait for the current job.');
      active = true;
      job = { id: randomBytes(12).toString('hex'), action, target, state: 'checking',
        startedAt: new Date().toISOString(), output: '', truncated: false };
      try { if (action === 'start') await preflight(target); }
      catch (error) {
        job.state = 'failed'; job.error = error.message; job.finishedAt = new Date().toISOString();
        active = false; throw error;
      }
      job.state = 'running';
      let captured = Buffer.alloc(0);
      const append = chunk => {
        const output = Buffer.concat([captured, Buffer.from(chunk)]);
        if (output.length > OUTPUT_LIMIT) job.truncated = true;
        captured = output.subarray(Math.max(0, output.length - OUTPUT_LIMIT));
        job.output = captured.toString();
      };
      const options = { timeout: 30 * 60_000, color: true, onOutput: append };
      // Reserve the slot before any await; one accepted mutation includes its network preparation.
      void (async () => {
        try {
          if (action === 'start') {
            const networks = await docker(['network', 'ls', '--format', '{{.Name}}']);
            if (networks.truncated) throw fail(502, 'Network list exceeded the capture limit.');
            if (networks.stdout.trim().split(/\r?\n/).includes('dev-net')) {
              const network = await docker(['network', 'inspect', '-f', '{{.Driver}}', 'dev-net']);
              if (network.stdout.trim() !== 'bridge') throw fail(422, 'dev-net must use the bridge driver. No network was changed.');
              append('[Network] Reusing dev-net (bridge).\n');
            } else {
              append('[Network] Creating dev-net (bridge, br-docker).\n');
              await docker(['network', 'create', '--driver', 'bridge', '--opt',
                'com.docker.network.bridge.name=br-docker', 'dev-net'], { onOutput: append });
            }
          }
          append(`$ docker compose ${args.join(' ')}\n`);
          const result = await run(composeArgs(args), options);
          job.exitCode = result.code;
          if (result.timedOut) job.state = 'timed_out';
          checked(result);
          job.state = 'succeeded';
        } catch (error) {
          if (job.state !== 'timed_out') job.state = 'failed';
          job.error = error.message;
        } finally { job.finishedAt = new Date().toISOString(); active = false; }
      })();
      return job;
    },
  };
}

export function validateRequest(req, port, token) {
  const host = req.headers.host;
  const hosts = [`127.0.0.1:${port}`, `localhost:${port}`];
  if (port === 80) hosts.push('127.0.0.1', 'localhost');
  if (!hosts.includes(host)) throw fail(403, 'Untrusted Host.');
  const origin = new URL(`http://${host}`).origin;
  if (req.headers.origin !== undefined && req.headers.origin !== origin) throw fail(403, 'Untrusted Origin.');
  if (req.headers['sec-fetch-site'] && !['same-origin', 'none'].includes(req.headers['sec-fetch-site'])) {
    throw fail(403, 'Cross-site requests are forbidden.');
  }
  if (req.method === 'POST') {
    if (req.headers.origin !== origin || req.headers['x-csrf-token'] !== token) throw fail(403, 'Invalid CSRF token or Origin. Reload the page.');
    if (!/^application\/json(?:\s*;\s*charset=utf-8)?$/i.test(req.headers['content-type'] || '')) {
      throw fail(415, 'Content-Type must be application/json.');
    }
  }
}

async function bodyJSON(req) {
  if (Number(req.headers['content-length']) > BODY_LIMIT) throw fail(413, 'Request body exceeds 4096 bytes.');
  const chunks = [];
  let size = 0;
  for await (const chunk of req.iterator({ destroyOnReturn: false })) {
    size += chunk.length;
    if (size > BODY_LIMIT) throw fail(413, 'Request body exceeds 4096 bytes.');
    chunks.push(chunk);
  }
  try { return JSON.parse(Buffer.concat(chunks).toString()); }
  catch { throw fail(400, 'Invalid JSON body.'); }
}

const STATIC = new Map([
  ['/', ['index.html', 'text/html; charset=utf-8']],
  ['/app.js', ['app.js', 'text/javascript; charset=utf-8']],
  ['/ansi.js', ['ansi.js', 'text/javascript; charset=utf-8']],
  ['/styles.css', ['styles.css', 'text/css; charset=utf-8']],
  ['/logo.png', ['logo.png', 'image/png']],
]);

export function createServer({ manager = createManager(), port = 5002 } = {}) {
  const token = randomBytes(32).toString('hex');
  const server = http.createServer(async (req, res) => {
    res.setHeader('Cache-Control', 'no-store');
    res.setHeader('X-Content-Type-Options', 'nosniff');
    res.setHeader('X-Frame-Options', 'DENY');
    res.setHeader('Referrer-Policy', 'no-referrer');
    res.setHeader('Content-Security-Policy', "default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self'; connect-src 'self'; base-uri 'none'; frame-ancestors 'none'; form-action 'none'");
    const json = (status, data) => {
      res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8' });
      res.end(JSON.stringify(data));
    };
    try {
      validateRequest(req, typeof port === 'function' ? port() : port, token);
      if (!req.url.startsWith('/') || req.url.startsWith('//')) throw fail(400, 'Invalid request path.');
      const url = new URL(req.url, 'http://127.0.0.1');
      if (req.method === 'GET' && STATIC.has(req.url)) {
        const [file, type] = STATIC.get(req.url);
        const data = await readFile(new URL(`./public/${file}`, import.meta.url));
        res.writeHead(200, { 'Content-Type': type }); res.end(data); return;
      }
      if (req.method === 'GET' && req.url === '/api/session') return json(200, { token });
      if (req.method === 'GET' && req.url === '/api/status') return json(200, { services: await manager.status() });
      if (req.method === 'GET' && req.url === '/api/job') return json(200, { job: manager.job });
      if (req.method === 'GET' && url.pathname === '/api/logs') {
        if ([...url.searchParams.keys()].some(key => !['service', 'tail'].includes(key)) ||
            url.searchParams.getAll('service').length !== 1 || url.searchParams.getAll('tail').length > 1) throw fail(400, 'Invalid log query.');
        return json(200, await manager.logs(url.searchParams.get('service'), url.searchParams.get('tail') ?? '200'));
      }
      if (req.method === 'POST' && req.url === '/api/actions') {
        const body = await bodyJSON(req);
        if (!body || Array.isArray(body) || typeof body !== 'object' ||
            Object.keys(body).sort().join(',') !== 'action,target') throw fail(400, 'Expected only action and target.');
        return json(202, { job: await manager.submit(body.action, body.target) });
      }
      throw fail(404, 'Not found.');
    } catch (error) {
      if (!req.readableEnded) { res.setHeader('Connection', 'close'); req.resume(); }
      json(error.status || 500, { error: error.message || 'Internal server error.' });
    }
  });
  server.requestTimeout = 10_000;
  server.headersTimeout = 10_000;
  server.timeout = 35_000;
  server.keepAliveTimeout = 5000;
  server.maxRequestsPerSocket = 100;
  return server;
}

export async function main() {
  const port = parsePort(process.env.MANAGER_PORT);
  checked(await execute(['compose', 'version']));
  const server = createServer({ port });
  await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(port, '127.0.0.1', resolve);
  });
  console.log(`YoAuthorize manager: http://127.0.0.1:${port}\nDeployment: ${DEPLOY}\nNo services or networks changed at startup.`);
  return server;
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  main().catch(error => { console.error(`Manager startup failed: ${error.message}`); process.exitCode = 1; });
}
