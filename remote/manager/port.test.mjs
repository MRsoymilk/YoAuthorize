import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, writeFile, rm } from 'node:fs/promises';
import path from 'node:path';
import { tmpdir } from 'node:os';
import { loadManagerPort } from './server.mjs';
import { fileURLToPath } from 'node:url';

const fromText = text => loadManagerPort({ env: {}, readEnvFile: async () => text });

test('port file accepts literal ports, whitespace, export, quotes, comments and CRLF', async () => {
  for (const text of ['MANAGER_PORT=5012', ' \texport MANAGER_PORT = 5012 \t# local',
    'MANAGER_PORT="5012" # local', "MANAGER_PORT='5012'# local", '\uFEFFMANAGER_PORT=5012\r\n',
    '# MANAGER_PORT=bad\nMANAGER_PORT=5012 # ${IGNORED}', 'MANAGER_PORT=1\nMANAGER_PORT=5012']) {
    assert.equal(await fromText(text), 5012);
  }
  assert.equal(await fromText('MANAGER_PORT=65535'), 65535);
});

test('missing and empty port values select the default', async () => {
  for (const text of ['', '# comment', 'OTHER_PORT=5012', 'MANAGER_PORT_EXTRA=5012',
    'MANAGER_PORT=', 'MANAGER_PORT= \t# default', 'MANAGER_PORT=""', "export MANAGER_PORT=''",
    'MANAGER_PORT=5012\nMANAGER_PORT=']) assert.equal(await fromText(text), 5002);
});

test('invalid file ports and unsupported syntax fail without disclosing values', async () => {
  for (const value of ['0', '65536', '-1', '1.5', '5e3', '05012', '5012abc', '5012#comment',
    '" 5012 "', '"5012', "'5012", '"5012" junk', '${RAW_SECRET:-5012}', '$RAW_SECRET',
    "'${RAW_SECRET}'", '$(RAW_SECRET)', '"50\\n12"', '"5012\n"', 'RAW_SECRET']) {
    await assert.rejects(fromText(`MANAGER_PORT=${value}`), error => {
      assert.match(error.message, /MANAGER_PORT.*literal integer.*interpolation.*override/);
      assert.doesNotMatch(error.message, /RAW_SECRET/);
      return true;
    });
  }
  await assert.rejects(fromText('export MANAGER_PORT'), /Invalid MANAGER_PORT/);
  await assert.rejects(fromText('MANAGER_PORT : 5012'), /Invalid MANAGER_PORT/);
  await assert.rejects(fromText('MANAGER_PORT=bad\nMANAGER_PORT=5012'), /Invalid MANAGER_PORT/);
});

test('process overrides skip all file access, including empty and invalid overrides', async () => {
  const readEnvFile = () => { assert.fail('override must not read the file'); };
  assert.equal(await loadManagerPort({ env: { MANAGER_PORT: '5013' }, readEnvFile }), 5013);
  assert.equal(await loadManagerPort({ env: { MANAGER_PORT: '' }, readEnvFile }), 5002);
  for (const value of ['bad', ' 5013', '0', '${PORT}']) {
    await assert.rejects(loadManagerPort({ env: { MANAGER_PORT: value }, readEnvFile }), /MANAGER_PORT must/);
  }
});

test('unrelated multiline secrets cannot inject port assignments or contaminate the environment', async () => {
  const before = { ...process.env };
  for (const quote of ["'", '"']) {
    assert.equal(await fromText(`SECRET=${quote}first \\${quote} still quoted\nMANAGER_PORT=6010\nlast${quote}\nMANAGER_PORT=5012`), 5012);
    assert.equal(await fromText(`SECRET=${quote}first\nMANAGER_PORT=6010\nlast${quote}`), 5002);
  }
  assert.equal(await fromText('SECRET="one line"\nOTHER=${SECRET}\nMANAGER_PORT=5012'), 5012);
  assert.deepEqual({ ...process.env }, before);
});

test('default file path is module-relative and read errors are sanitized', async () => {
  assert.equal(await loadManagerPort({ env: {}, readEnvFile: async (file, encoding) => {
    assert.equal(file, fileURLToPath(new URL('../deploy/.env', import.meta.url)));
    assert.equal(encoding, 'utf8');
    return 'MANAGER_PORT=5012';
  } }), 5012);
  for (const code of ['EACCES', 'EISDIR', 'EIO']) {
    await assert.rejects(loadManagerPort({ env: {}, readEnvFile: async () => {
      throw Object.assign(new Error('RAW_SECRET'), { code });
    } }), error => /Cannot read.*permissions.*MANAGER_PORT/.test(error.message) && !error.message.includes('RAW_SECRET'));
  }
});

test('real disposable port file supports paths with spaces and absence before initialization', async t => {
  const deploy = await mkdtemp(path.join(tmpdir(), 'manager port-'));
  t.after(() => rm(deploy, { recursive: true, force: true }));
  assert.equal(await loadManagerPort({ env: {}, deploy }), 5002);
  await writeFile(path.join(deploy, '.env'), 'MANAGER_PORT="5012" # fixture\nSECRET=private-fixture');
  assert.equal(await loadManagerPort({ env: {}, deploy }), 5012);
  assert.equal(await loadManagerPort({ env: { MANAGER_PORT: '5013' }, deploy }), 5013);
  await writeFile(path.join(deploy, '.env'), 'MANAGER_PORT=invalid-private-fixture');
  await assert.rejects(loadManagerPort({ env: {}, deploy }), /Invalid MANAGER_PORT/);
});
