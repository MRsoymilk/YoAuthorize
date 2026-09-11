const $ = id => document.getElementById(id);
const names = ['postgres', 'redis', 'mailpit', 'signer', 'api', 'web', 'caddy', 'migrate'];
const descriptions = { postgres: 'Persistent database', redis: 'Cache', mailpit: 'Development mail', signer: 'License signing', api: 'Authorization backend', web: 'Frontend', caddy: 'HTTP gateway', migrate: 'One-shot / manual rerun' };
let token, busy = false, submitting = false, statusLoading = false;
let logsLoading = false, logsPending = false, logRevision = 0, logTimer;
let linksLoading = false;
const destinations = { frontend: '/login', dashboard: '/app', admin: '/admin/users', swagger: '/docs', openapi: '/api-docs/openapi.json', health: '/health/ready', mailbox: '' };

async function links() {
  if (linksLoading) return;
  linksLoading = true;
  let data = {};
  try {
    data = await api('/api/links');
    $('nav-note').textContent = [!data.publicUrl && 'Application links unavailable', !data.mailboxUrl && 'Mailbox link unavailable'].filter(Boolean).join('; ');
  } catch {
    $('nav-note').textContent = 'Links unavailable. Retry with Refresh status.';
  } finally {
    for (const [name, path] of Object.entries(destinations)) {
      const node = $(`nav-${name}`), base = name === 'mailbox' ? data.mailboxUrl : data.publicUrl;
      node.target = '_blank'; node.rel = 'noopener noreferrer';
      node.setAttribute('aria-disabled', String(!base));
      if (base) node.href = base + path;
      else node.removeAttribute('href');
    }
    linksLoading = false;
  }
}
$('nav-api').addEventListener('keydown', event => {
  if (event.key === 'Escape') {
    $('nav-api').open = false;
    $('nav-api-summary').focus();
  }
});

async function api(url, options) {
  const response = await fetch(url, { ...options, signal: AbortSignal.timeout(35_000) });
  const data = await response.json();
  if (!response.ok) throw new Error(data.error || `HTTP ${response.status}`);
  return data;
}
function error(message = '') { $('error').textContent = message; $('error').hidden = !message; }
function controls() {
  document.querySelectorAll('[data-action]').forEach(button => { button.disabled = busy || submitting || !token; });
}
for (const name of names) {
  const row = document.createElement('article');
  row.className = 'service';
  const title = document.createElement('div');
  const heading = document.createElement('h3'); heading.textContent = name;
  const description = document.createElement('small'); description.textContent = descriptions[name];
  title.append(heading, description);
  const state = document.createElement('div'); state.id = `state-${name}`; state.className = 'state'; state.textContent = 'Unknown';
  const actions = document.createElement('div'); actions.className = 'actions';
  for (const action of name === 'migrate' ? ['start', 'logs'] : ['start', 'stop', 'restart', 'logs']) {
    const button = document.createElement('button');
    button.textContent = name === 'migrate' && action === 'start' ? 'Run migration' : action[0].toUpperCase() + action.slice(1);
    if (action === 'logs') button.addEventListener('click', () => { $('log-service').value = name; selectLogs(); });
    else { button.dataset.action = action; button.dataset.target = name; if (action === 'stop') button.className = 'danger'; }
    actions.append(button);
  }
  row.append(title, state, actions); $('services').append(row);
  const option = document.createElement('option'); option.value = name; option.textContent = name; $('log-service').append(option);
}
$('log-service').value = 'api';

async function status() {
  if (statusLoading) return;
  statusLoading = true; $('refresh').disabled = true;
  try {
    const data = await api('/api/status');
    $('nav-warning').textContent = ['caddy', 'mailpit'].map(name => {
      const containers = data.services.find(row => row.service === name)?.containers;
      return !containers?.length || containers.some(c => c.state !== 'running') ? `${name} not running` :
        containers.some(c => c.health && c.health !== 'healthy') ? `${name} not ready` : '';
    }).filter(Boolean).join('; ');
    for (const { service, containers } of data.services) {
      const node = $(`state-${service}`);
      node.textContent = containers.length ? containers.map(c => c.completed ? 'Completed / exit 0' :
        `${c.state}${c.health ? ` / ${c.health}` : ''}${c.state === 'exited' ? ` / exit ${c.exitCode ?? '?'}` : ''}`).join('\n') : 'Not created';
      node.className = 'state' + (containers.length && containers.every(c => c.completed || (c.state === 'running' && (!c.health || c.health === 'healthy'))) ? ' good' :
        containers.some(c => c.health === 'unhealthy' || (c.state === 'exited' && c.exitCode !== null && Number(c.exitCode) !== 0)) ? ' bad' : '');
    }
    $('status-note').textContent = `Last checked ${new Date().toLocaleTimeString()}. Container health is separate from operation progress.`;
  } catch (e) {
    $('nav-warning').textContent = 'Service status unavailable';
    $('status-note').textContent = `Status unavailable: ${e.message}`;
    for (const name of names) { $(`state-${name}`).textContent = 'Unknown / status unavailable'; $(`state-${name}`).className = 'state'; }
  } finally { statusLoading = false; $('refresh').disabled = false; }
}
function showJob(job) {
  busy = job && ['checking', 'running'].includes(job.state);
  controls();
  if (!job) return;
  $('job-state').textContent = job.state.replace('_', ' ').toUpperCase();
  $('job-meta').textContent = `${job.action} ${job.target} / ${new Date(job.startedAt).toLocaleString()}${job.finishedAt ? ' / finished' : ' / in progress'}${job.truncated ? ' / older output discarded' : ''}`;
  renderAnsi($('job-output'), job.output || (busy ? 'Waiting for Docker...' : 'No command output.'), job.truncated,
    (job.error ? `\nERROR: ${job.error}` : '') + (job.state === 'succeeded' ? '\nCommand completed. Check service status for readiness.' : ''));
}
document.querySelectorAll('[data-action]').forEach(button => button.addEventListener('click', async () => {
  const { action, target } = button.dataset;
  if (action === 'stop' || action === 'restart' || target === 'migrate' || target === 'frontend') {
    const database = action !== 'start' && ['postgres', 'backend', 'infrastructure', 'all'].includes(target);
    const groups = { backend: 'postgres, redis, mailpit, signer, api', frontend: 'web, caddy',
      infrastructure: 'postgres, redis, mailpit', all: 'postgres, redis, mailpit, signer, api, web, caddy' };
    const selected = groups[target] ? ` (${groups[target]}${action === 'stop' && ['backend', 'all'].includes(target) ? ', migrate' : ''})` : '';
    const detail = target === 'migrate' ? 'This applies database schema changes and starts PostgreSQL if needed.' :
      target === 'frontend' && action === 'start' ? 'WARNING: Starting web + caddy also starts backend dependencies via Caddy: api, postgres, redis, signer and migrate. This may apply database schema changes.' :
      `${target === 'frontend' ? 'Only web and caddy are affected; backend services will not be stopped or restarted. ' : ''}This can interrupt requests. Containers and volumes will not be deleted.`;
    if (!confirm(`${action === 'start' && target === 'migrate' ? 'Run migration for' : action} ${target}${selected}?\n${database ? 'WARNING: PostgreSQL will be interrupted; dependent services may fail.\n' : ''}${detail}`)) return;
  }
  submitting = true; controls(); error();
  try { showJob((await api('/api/actions', { method: 'POST', headers: { 'Content-Type': 'application/json', 'X-CSRF-Token': token }, body: JSON.stringify({ action, target }) })).job); }
  catch (e) { error(`${e.message} If the response was lost, check Latest operation before retrying.`); }
  finally { submitting = false; controls(); }
}));
async function loadLogs() {
  clearTimeout(logTimer);
  if (logsLoading) { logsPending = true; return; }
  logsLoading = true;
  const service = $('log-service').value, tail = $('log-tail').value, revision = logRevision;
  $('logs-refresh').disabled = true; $('log-note').textContent = `Fetching ${service}...`;
  try {
    const result = await api(`/api/logs?service=${service}&tail=${tail}`);
    if (revision !== logRevision) return;
    renderAnsi($('log-output'), result.output || 'No logs returned (the container may not exist yet).', result.truncated);
    $('log-note').textContent = `${service} / snapshot at ${new Date().toLocaleTimeString()}${result.truncated ? ' / output truncated' : ''}`;
  } catch (e) {
    if (revision !== logRevision) return;
    $('log-note').textContent = `Logs unavailable: ${e.message}`; renderAnsi($('log-output'), 'No current snapshot.');
  } finally {
    logsLoading = false; $('logs-refresh').disabled = false;
    // Coalesce selection changes without overlapping requests or displaying stale results.
    if (logsPending) { logsPending = false; void loadLogs(); }
    else if ($('logs-live').checked) logTimer = setTimeout(loadLogs, 3000);
  }
}
function selectLogs() {
  logRevision++;
  renderAnsi($('log-output'), 'No current snapshot.');
  $('log-note').textContent = `Waiting for ${$('log-service').value} logs...`;
  void loadLogs();
}
$('refresh').addEventListener('click', () => { void links(); return status(); });
$('logs-refresh').addEventListener('click', loadLogs);
$('log-service').addEventListener('change', selectLogs);
$('log-tail').addEventListener('change', selectLogs);
$('logs-live').addEventListener('change', () => {
  clearTimeout(logTimer);
  if ($('logs-live').checked && !logsLoading) void loadLogs();
});
async function poll() {
  try {
    if (!token) token = (await api('/api/session')).token;
    const wasBusy = busy;
    showJob((await api('/api/job')).job);
    if (wasBusy && !busy) status();
    controls();
  } catch (e) { busy = true; controls(); $('job-state').textContent = `Connection unavailable: ${e.message}`; }
  setTimeout(poll, 1500);
}
controls(); poll(); status(); links();
setInterval(status, 8000);
