# Host Manager

Dependency-free, development-only web controls for this checkout's `yoauthorize`
Compose project. Requires Node.js 20+ and the host's Docker CLI with Compose v2.
No npm install, manager container, or Docker-in-Docker.

The header and favicon use the manager's own `public/logo.png`, copied from
the project's `res/logo.png`. Update this copy when changing the project logo;
the manager never serves files from the root resource directory.

From the repository root:

```sh
node remote/manager/server.mjs
# Open the loopback URL printed at startup (default port 5002)
# Optional process override (integer 1..65535):
MANAGER_PORT=5012 node remote/manager/server.mjs
```

## Manager Port

At startup, the manager selects its port from the process `MANAGER_PORT`, then
`MANAGER_PORT` in this checkout's `remote/deploy/.env`, then the default `5002`.
An explicitly empty value selects `5002`, including an empty process override
(which skips the file). A missing file or missing key also uses the default, so
the manager can start before initialization. Other file read errors fail clearly.
Paths are module-relative even when launched from another working directory.

The file accepts literal ports such as `MANAGER_PORT=5012`, optional `export`,
whitespace around the assignment, single or double quotes, and trailing comments
(`MANAGER_PORT=5012 # local manager`). Unquoted comments require whitespace;
spaces inside quotes are part of the value. Empty quotes also select the default.
Repeated assignments use the last value; every present assignment must be valid.
Invalid ports, interpolation (such as `${PORT:-5002}`), escapes, and multiline
port values fail with instructions to use a literal port or process override.
This is intentionally not a general Compose `.env` evaluator.

Only the manager port is extracted: no shell or Compose configuration command is
run for this lookup, no file values are logged, and nothing is loaded into
`process.env`. Unrelated quoted multiline values are skipped. The port is read
once, not on refresh; relaunch the manager to apply changes. Host validation uses
the selected port. Node.js 20+ remains supported without dependencies.

The server always binds `127.0.0.1`. Deployment and asset paths resolve from the
module location, not the shell's working directory. Startup only verifies
`docker compose version`; it does not contact the daemon to start services,
create networks, or generate secrets. Status errors (including daemon access
and missing `.env`) are shown, not converted to an empty/healthy stack.

Before the first service start, review `remote/deploy/README.md`. For a fresh
installation **only**, initialize secrets explicitly:

```sh
bash remote/deploy/init-secrets.sh
```

Never remove existing secrets to satisfy preflight. Restore missing files and
review `.env`, especially `LOCAL_UID`/`LOCAL_GID` and build proxy configuration.
The manager checks `.env` and all Compose fragments for every start because the
main Compose file references them. Secret checks follow the selected target and
its dependencies; required secrets must be readable and nonempty. Contents are
not validated or overwritten. The tools-only bootstrap password is never required.

| Start target | Required files beyond `.env` and Compose files |
| --- | --- |
| redis, mailpit, web | None; no secrets directory or Caddyfile needed |
| postgres, infrastructure | `secrets/postgres-password` |
| migrate | `secrets/postgres-password`, `secrets/database-url` |
| signer | `secrets/license-signing-key`, `secrets/signer-shared-secret` |
| api, backend | All five business secrets: postgres-password, database-url, activation-pepper, signer-shared-secret, license-signing-key |
| caddy, frontend, all | All five business secrets plus `Caddyfile` and `Caddy.routes` |

## Navigation

The brand stays on `/`. The adjacent navigation opens new tabs with
`rel="noopener noreferrer"`: Frontend (`/login`), Dashboard (`/app`), Admin
(`/admin/users`), and an API dropdown containing Swagger (`/docs`), OpenAPI
(`/api-docs/openapi.json`), and readiness Health (`/health/ready`). Swagger covers
all public HTTP operations and documents the native WebSocket handshake; its
Try It Out controls do not implement WebSocket sessions. See the
[API guide](../backend/API.md) for cookie login and CSRF instructions. Dashboard
and Admin retain the application's login and authorization requirements; manager
access does not sign you in. The native API disclosure supports click and keyboard
activation; Escape closes it and focuses its summary.

`GET /api/links` runs the same pinned `docker compose config --format json` as
other manager reads. It returns only a validated HTTP(S) origin from Caddy's
resolved `environment.PUBLIC_URL` and a Mailbox URL from Mailpit's published TCP
port targeting 8025. It does not guess from the manager environment or expose
the full configuration. Credentials, non-root paths, queries and fragments are
rejected. Public hostnames and configured ports are retained. Mailbox uses only
local published bindings, translating wildcard IPv4/IPv6 to loopback, never
container IPs. The default deployment publishes Mailpit at `127.0.0.1:8025`;
the application normally uses `localhost:8088`, separately from manager port 5002.

Links load once per page and retry/reload on **Refresh status**, not on status/job
polls. Invalid or unavailable configuration shows explicit unavailable labels and
entries without hrefs, with no guessed fallback. Truncated config capture (256 KiB)
is refused; config failures never return Compose stderr or raw parse errors.
Status warnings indicate Caddy/Mailpit not running or not ready, but valid links
remain clickable. Navigation does not probe remote URLs or start anything. Use
the existing explicit Start controls when needed; Frontend Start also starts
backend dependencies, while Mailpit can be started individually.

## Controls

| Target | Start | Stop / Restart |
| --- | --- | --- |
| Individual persistent service | `up -d SERVICE`, including dependencies | `stop SERVICE` / `restart --no-deps SERVICE` |
| Backend | `up -d api mailpit` (Compose supplies postgres, migrate, redis, signer) | Explicit backend service names; restart excludes migrate |
| Frontend | `up -d web caddy` (Caddy also starts backend dependencies: api, postgres, migrate, redis, signer) | `stop web caddy` / `restart --no-deps web caddy`; does not stop/restart backend |
| Infrastructure | `up -d postgres redis mailpit` (only these three) | `stop postgres redis mailpit` / `restart --no-deps postgres redis mailpit`; no other services targeted |
| All | `up -d postgres redis mailpit signer api web caddy` | Explicit seven services plus migrate for stop; restart excludes migrate |
| Migration | Manual `up -d migrate`, including postgres; confirm schema changes | No individual stop/restart |

The page exposes Start, Stop and Restart for all four groups (12 buttons), plus
individual starts/stops/restarts/logs. Frontend Start requires confirmation because
it also starts backend dependencies and may apply migrations; Frontend Stop/Restart
affect only web and caddy, without a database-interruption warning. Backend,
Infrastructure and All Stop/Restart warn about PostgreSQL interruption.
A start may build missing local images;
there is no forced rebuild/pull control. Restart does not create missing
containers or apply configuration changes; use Start for reconciliation.
Successful exited migration containers display **Completed**, not a stack failure.

Only a start checks/creates `dev-net`, with the existing helper's bridge driver
and `com.docker.network.bridge.name=br-docker` option. An existing network must
use the bridge driver and is never altered. Network name is fixed to `dev-net`
because the development compose file requires it; helper environment overrides
are intentionally not used. A network may remain after a failed start.

Every Compose call pins the project, main compose file, `.env`, and absolute
`YOAUTHORIZE_REMOTE_DIR` to this checkout. Inherited profiles are disabled.
Docker client credentials, context, and proxy settings are those of the host
user. Verify your Docker context points to the intended local engine before use.
Production overrides and `bootstrap-admin` remain CLI-managed.

## Jobs And Limits

- One mutation at a time, including preflight/network setup. Concurrent requests
  get HTTP 409; there is no waiting queue. Only the latest in-memory job is kept.
- Polling shows checking/running/succeeded/failed/timed-out states and the last
  256 KiB of job output. Command success is not a readiness guarantee.
- Status polls every 8 seconds; jobs every 1.5 seconds. Logs are bounded snapshots,
  tail 1..1000 lines (default 200), capped at 256 KiB. Docker capture bounds each
  stdout/stderr pipe to 256 KiB. Truncation is reported; truncated status is an error.
- **Live logs (3s)** is off by default. Enabling it fetches immediately, then
  polls 3 seconds after each completed request, including failed requests. Manual
  fetches and service/tail changes share the same single in-flight log request;
  changes are coalesced into a fetch for the latest selection. Stale responses
  cannot overwrite the selected service's output. Each snapshot replaces rather
  than appends output. Disabling live logs clears the timer; an already-running
  request may finish, but does not schedule another poll.
- Reads/network commands time out after 20 seconds; mutations after 30 minutes.
  Timed-out CLI processes receive TERM then KILL after 2 seconds. Docker daemon
  work may continue: inspect status before retrying. No rollback is attempted.
- Four simultaneous Docker reads maximum; JSON bodies capped at 4096 bytes.
- Stop uses explicit names, never `down`; it keeps containers, networks and
  volumes. Confirmations warn about downtime, especially PostgreSQL. Stopping
  the manager with Ctrl-C does not stop services. Avoid exiting during a job;
  its in-memory record is lost and daemon work may continue.

## Output Colors

Service logs and Compose start/stop/restart output request color with the global
`docker compose --ansi always` option and `COMPOSE_ANSI=always`. Logs no longer
use `--no-color`. Status JSON, startup version checks and network commands use
`COMPOSE_ANSI=never` (and `--ansi never` for Compose). Colored commands use
automatic progress mode because Compose rejects forced ANSI with `plain` progress;
all output is still captured through pipes, not an interactive terminal.
No business-container environment, TTY, logging,
or Compose configuration is changed. Applications must emit their own ANSI;
plain text and plain JSON logs remain plain, without JSON syntax highlighting.

Both panes safely render SGR base/bright foreground and background, 256-color
and semicolon-form RGB truecolor, bold, dim, italic, underline and their resets.
Output becomes DOM spans via `textContent`, never HTML or clickable OSC links.
Only fixed style properties and validated numeric colors are assigned through
the CSSOM; the existing `style-src 'self'` CSP is not relaxed.

These are bounded snapshots, not a terminal emulator: CR/CRLF become newlines,
cursor/clear commands and OSC/control strings are ignored. Trailing incomplete
escapes are hidden until a later snapshot completes them. A cap may discard a
style reset or the start of an escape; each snapshot starts with default styles,
and recognizable leading SGR fragments are removed best-effort when truncated.
Arbitrary mid-OSC fragments cannot be reliably identified and may appear as
literal text. Unsupported SGR modes are ignored. Unchanged snapshots retain
their DOM; replacements preserve scroll offsets or follow the bottom if already
there. All existing capture, tail, polling and concurrency limits still apply.

## Security

This is a trusted-local-administrator tool, not a multi-user authentication
service. Docker access is effectively host-root access. Do not expose it via a
public proxy or port mapping. Local processes/users with loopback access can
retrieve the session token. Logs may contain sensitive application output.

Exact loopback Host validation, same-origin checks, Fetch Metadata checks,
per-process CSRF tokens, and JSON-only mutations reject cross-origin browser
requests and localhost DNS rebinding. Use `http://127.0.0.1:PORT` or
`http://localhost:PORT`; no aliases. There is no CORS, arbitrary command,
file-serving, secret-editing, delete or prune API. Static paths are explicit.
All Docker executions use argument arrays and no shell. Trusted local compose
and `.env` files still control container behavior; do not use untrusted checkouts.
Do not operate the same stack concurrently through the CLI or other tools: the mutation lock
only covers this manager process.

## Tests

```sh
node --test remote/manager/*.test.mjs
```

Tests inject fake child execution, use disposable file fixtures, and start/close
an ephemeral loopback HTTP server. They never invoke Docker or mutate business
containers. Frontend tests run the page script with fake DOM, fetch and timers.
Coverage includes commands/groups, paths with spaces, target-aware preflight,
network behavior, status formats, bounded output/timeouts, origin/CSRF, body
limits, explicit assets, concurrent mutations, progress polling, and serialized
live/manual log fetches with service/tail switching and stale-response rejection,
ANSI command opt-in, split output capture, safe color/reset rendering, ignored
OSC/control sequences, truncated snapshots and scroll preservation.
