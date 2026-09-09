import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { copyFileSync, mkdirSync, mkdtempSync, rmSync, symlinkSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const source = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../deploy');
const temporary = mkdtempSync(path.join(tmpdir(), 'yoauthorize-paths-'));
const env = { ...process.env };
delete env.YOAUTHORIZE_REMOTE_DIR;
delete env.COMPOSE_FILE;
delete env.COMPOSE_PROJECT_NAME;

try {
  const remote = path.join(temporary, 'checkout with spaces', 'remote');
  const deploy = path.join(remote, 'deploy');
  const stacks = path.join(remote, 'dockge', '.state', 'stacks');
  mkdirSync(deploy, { recursive: true });
  mkdirSync(stacks, { recursive: true });
  for (const file of [
    'compose.yaml', 'compose.backend.yaml', 'compose.infrastructure.yaml',
    'compose.web.yaml', 'compose.production.yaml',
  ]) {
    copyFileSync(path.join(source, file), path.join(deploy, file));
  }
  const logical = path.join(stacks, 'yoauthorize');
  symlinkSync(deploy, logical, 'dir');

  for (const [label, directory, extraEnv, production] of [
    ['CLI relative defaults', deploy, {}, false],
    ['Dockge logical directory', logical, { YOAUTHORIZE_REMOTE_DIR: remote }, false],
    ['production override', deploy, {}, true],
  ]) {
    const args = ['compose', '--project-directory', directory, '-f', path.join(directory, 'compose.yaml')];
    if (production) args.push('-f', path.join(directory, 'compose.production.yaml'));
    args.push('--profile', 'tools', 'config', '--format', 'json');
    const config = JSON.parse(execFileSync('docker', args, {
      cwd: temporary,
      env: { ...env, ...extraEnv, PWD: directory },
      encoding: 'utf8',
    }));
    assert.equal(config.name, 'yoauthorize');
    for (const service of ['api', 'signer', 'migrate', 'bootstrap-admin']) {
      assert.equal(config.services[service].build.context, path.join(remote, 'backend'));
    }
    assert.equal(config.services.web.build.context, path.join(remote, 'frontend'));
    const bind = config.services.caddy.volumes.find(volume => volume.type === 'bind');
    assert.equal(bind.source, path.join(deploy, 'Caddyfile'));
    const files = {
      postgres_password: 'postgres-password', database_url: 'database-url',
      activation_pepper: 'activation-pepper', signer_shared_secret: 'signer-shared-secret',
      license_signing_key: 'license-signing-key', bootstrap_admin_password: 'bootstrap-admin-password',
    };
    for (const [name, file] of Object.entries(files)) {
      assert.equal(config.secrets[name].file, path.join(deploy, 'secrets', file));
    }
    assert.equal(config.volumes.postgres_data.name, 'yoauthorize_postgres_data');
    console.log(`PASS: ${label} in relocated checkout with spaces`);
  }
} finally {
  rmSync(temporary, { recursive: true, force: true });
}
