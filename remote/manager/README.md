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
# Open http://127.0.0.1:5002
# Optional port (integer 1..65535):
MANAGER_PORT=5012 node remote/manager/server.mjs
```

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
| caddy, frontend, all | All five business secrets plus `Caddyfile` |

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
live/manual log fetches with service/tail switching and stale-response rejection.
