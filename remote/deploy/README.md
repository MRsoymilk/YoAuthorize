# Deployment And Operations

The Compose stack supplies PostgreSQL, Redis, Mailpit, signer, explicit migration job, public API, Nginx frontend, Caddy, and a tools-profile bootstrap job. See [backend](../backend/README.md), [public API](../backend/API.md), and [frontend](../frontend/README.md). Production requires operator configuration and review; this is not a turnkey production-readiness guarantee.

## Fresh Development Install

Requirements: Bash, OpenSSL CLI, Docker Engine with Linux containers, and Docker Compose v2 **>= 2.24.4** (`!override` support is required for production). Allow network access for image pulls and Cargo/npm builds, and sufficient build memory/disk. Host Rust/Node are not required for Compose builds. Use a non-root invoking user with Docker access; Docker access itself is privileged.

From a fresh checkout:

```bash
cd remote/deploy
docker compose version
./init-secrets.sh
./start-development.sh -d --build
docker compose ps -a
docker compose logs --tail=100 migrate api signer
```

`init-secrets.sh` is **fresh-install only**: it refuses if `secrets/` already exists, even after a partial initialization. It creates a mode-0700 directory and six random secret files. If `.env` does not exist, it creates one containing **only `LOCAL_UID` and `LOCAL_GID`**. It does not copy `.env.example` or merge variables into an existing `.env`. If you created `.env` first, set those IDs yourself to the secret owner's numeric IDs. Do not overwrite generated `.env` with example IDs or delete existing keys to make initialization succeed. Review any partial installation manually.

The defaults serve the UI at <http://localhost:8088>, Swagger at <http://localhost:8088/docs>, readiness at <http://localhost:8088/health/ready>, and captured development mail at <http://localhost:8025>. API port 8080, signer 8090, PostgreSQL 5432, Redis 6379, and Nginx 80 are not published to the host. Caddy publishes only `127.0.0.1:8088`; Mailpit publishes only `127.0.0.1:8025`.

Set `BOOTSTRAP_ADMIN_EMAIL` in `.env` (and optionally `BOOTSTRAP_ADMIN_NAME`), then provision once:

```bash
docker compose --profile tools run --rm bootstrap-admin
```

Retrieve `secrets/bootstrap-admin-password` privately through your local secret-management workflow, not logs or shared terminals. It is mounted only into bootstrap, not put in its environment. The command refuses if any admin exists; run only one instance. Ordinary users register through the UI and follow verification links in Mailpit. The API does not migrate on startup: Compose waits for `migrate` to succeed and signer/Redis health before starting it. Bootstrap also needs the migrated database.

## Configuration And Layout

Use [.env.example](.env.example) as a reference and merge intended settings into the generated `.env`. Compose reads this for interpolation; host Rust binaries do not automatically read dotenv. Keep `.env` private, especially when it contains SMTP credentials, and never commit it or `secrets/`.

| Setting | Default/meaning |
| --- | --- |
| `PUBLIC_URL` | `http://localhost:8088`; Caddy site address and API-generated links |
| `YOAUTHORIZE_HTTP_PORT` | `8088`; development host port only |
| `COOKIE_SECURE` | `false` in Compose development (standalone API default is true) |
| `SMTP_URL` | `smtp://yoauthorize-mailpit:1025` |
| `MAIL_FROM` | `YoAuthorize <no-reply@localhost>` |
| `SIGNING_KEY_ID` | `development-1`; preserve with the signing seed/database |
| `LOCAL_UID`, `LOCAL_GID` | Secret owner's IDs; fallback 65532 if not configured |
| `DOCKER_BUILD_NETWORK` | `default`; optional Linux `host` for host-loopback build proxy |
| `YOAUTHORIZE_REMOTE_DIR` | Defaults to `..`; root for relocated backend/frontend/deploy paths |

Changing the host port alone does not change generated URLs or Caddy's configured site. Keep the external URL and listener/forwarding configuration consistent; default development container port mapping targets 8088.

`compose.yaml` is the entry point and declares shared secrets, networks, volumes, and all nine services. It extends same-directory fragments: [backend](compose.backend.yaml), [infrastructure](compose.infrastructure.yaml), and [web](compose.web.yaml). Fragments are not standalone stacks and need no extra `-f` flags. [Caddyfile](Caddyfile) routes public API, health, and docs requests to the API and other paths to the SPA.

Optional host service controls: run `node remote/manager/server.mjs` from the repository root and open <http://127.0.0.1:5002>. Starting the manager does not start containers. See [Host Manager](../manager/README.md) for prerequisites/security. The optional `node remote/deploy/test-paths.mjs` checks relocated/symlinked Compose paths without starting containers or copying secrets.

## Networks And Lifecycle

`start-development.sh` creates/reuses the external `dev-net` bridge via `docker-network.sh`, rejecting an existing non-bridge network. New bridges default to host interface name `br-docker`. Use the startup script before plain development `docker compose up`; Compose itself does not create that external network. Helper `DOCKER_NETWORK` overrides do not automatically change the fixed network name in Compose.

API, web, Caddy, and Mailpit share `dev-net`; PostgreSQL/Redis use project-owned internal `data`, and signer uses internal `signing`. Development `dev-net` is shared across projects and is **not deployment isolation**: repeated service aliases may collide. Production replaces it with a project-owned bridge. Independent installations also need unique Compose project names (`-p`), nonconflicting ports, separate secrets/config, and deliberate image tags; local build tags are shared by default.

From `remote/deploy`, ordinary development lifecycle commands:

```bash
docker compose stop
docker compose start
docker compose down
```

`stop` retains containers; `start` resumes existing ones, not new configuration. `down` removes project containers/networks but preserves named volumes and external `dev-net`. Remove `dev-net` manually only after checking all its consumers and stopping them; do not remove a shared bridge as routine cleanup. Never use `down -v`, volume deletion, or volume pruning as an upgrade/repair/restore step.

For Linux builds with Docker's configured proxy on host loopback, set `DOCKER_BUILD_NETWORK=host` in local `.env`. Docker supplies build proxy arguments from `~/.docker/config.json`; the option changes build networking only, not runtime bridges. Do not bake proxy credentials into images.

## Secret Ownership

Compose local secrets are bind mounts and retain host ownership. The signer and bootstrap run as `LOCAL_UID:LOCAL_GID`; use a nonzero UID owning the mode-0600 signing key/bootstrap password. On Unix the signer rejects a seed readable by group/others. Shared secrets generated as 0644 remain readable by the fixed API UID 65532 inside mounts; the host directory is 0700. Preserve this access model or replace it with reviewed secret delivery, not broad permissions on the key.

API and signer services have read-only root filesystems and `/tmp` tmpfs. The API image runs as UID 65532; signer identity is overridden as above. Web is read-only with Nginx tmpfs mounts but is not explicitly non-root. PostgreSQL, Redis, Mailpit, Caddy, and one-shot tools do not all have the same read-only/non-root settings. Do not infer universal hardening from the API/signer configuration.

## Production

Use a real hostname pointing to this server, reachable TCP 80/443 for Caddy HTTPS/ACME, and UDP 443 if using HTTP/3. Persist Caddy data/config volumes. Set values in private `.env`, retaining correct UID/GID and existing key identity:

```dotenv
PUBLIC_URL=https://licenses.example.com
COOKIE_SECURE=true
SMTP_URL=smtps://SMTP_USER:SMTP_PASSWORD@smtp.example.com:465
MAIL_FROM=YoAuthorize <licenses@example.com>
```

These SMTP credentials are placeholders; use provider-approved credentials and URL-encode reserved characters. Configure sender authorization and verify delivery. HTTPS `PUBLIC_URL` requires secure cookies; do not disable them to bypass login problems. Review TLS trust/CA availability for the actual SMTP provider and runtime image. There is no backend CORS configuration: serve UI/API same-origin through Caddy.

Always use both files for production lifecycle/tool commands:

```bash
docker compose -f compose.yaml -f compose.production.yaml up -d --build
docker compose -f compose.yaml -f compose.production.yaml --profile tools run --rm bootstrap-admin
docker compose -f compose.yaml -f compose.production.yaml ps -a
```

The override replaces development Caddy ports with public TCP 80/443 and UDP 443 and replaces external `dev-net` with an isolated project bridge. It does not require development network initialization. It **still includes Mailpit**, including its loopback 8025 UI, and does not itself replace SMTP defaults. Configure real SMTP and review whether to remove Mailpit via an operator-maintained deployment configuration. Do not expose its UI publicly. API/database/Redis/signer ports remain unpublished. Review firewall, admin access, logs, resource limits, monitoring, image provenance/pinning, and recovery before serving real users.

## Backup And Restore

PostgreSQL holds accounts, sessions, licenses, activations, public key metadata, audit, and outbox. Redis is ephemeral rate-limit state (`--save ""`, AOF disabled); restarting it resets counters. Mailpit is not a production mail archive. Back up the **signing seed, activation pepper, shared secret, database credentials, signing key ID, private `.env`, Compose/Caddy configuration, and exact release/image identities** alongside the database. Lost pepper makes existing codes unusable; a new seed does not match active public metadata. Keep encrypted, access-controlled, off-host copies and test restoration in isolation. Store already-distributed activation codes securely; database backups contain hashes/hints, not their original plaintext.

Concrete production procedure below uses Bash from `remote/deploy`, PostgreSQL 17 tools in the container, and a new backup directory. Use the same `-p` project flag in every command if your installation has a custom name. For development omit the production file and ensure `dev-net` exists. Commands are manual maintenance operations, not a scheduled live-backup script.

1. Announce maintenance and block incoming traffic. Stop **all writers**, including API replicas, bootstrap jobs, external publishers, manager-triggered starts, and direct SQL clients. The commands stop the shipped entry point/API; operators must stop any additional writers.
2. Take and verify the database dump and private configuration archive. `set -e` prevents continuing after a failed command; do not reopen traffic after a failed backup without review.

```bash
set -e
umask 077
dc=(docker compose -f compose.yaml -f compose.production.yaml)
backup_dir="$HOME/yoauthorize-backup-$(date -u +%Y%m%dT%H%M%SZ)"
mkdir "$backup_dir"
"${dc[@]}" stop caddy api
"${dc[@]}" exec -T postgres pg_dump -U yoauthorize -d yoauthorize -Fc > "$backup_dir/database.dump"
test -s "$backup_dir/database.dump"
"${dc[@]}" exec -T postgres pg_restore --list < "$backup_dir/database.dump" > "$backup_dir/database.contents"
tar -czf "$backup_dir/private-config.tgz" .env secrets compose.yaml compose.backend.yaml compose.infrastructure.yaml compose.web.yaml compose.production.yaml Caddyfile
"${dc[@]}" images > "$backup_dir/images.txt"
git rev-parse HEAD > "$backup_dir/revision.txt"
"${dc[@]}" start api caddy
```

This logical backup does not archive named volumes. Separately preserve Caddy TLS/account data using your volume-backup tooling while Caddy is stopped, or plan deliberate certificate reissuance (subject to CA limits). The private archive is plaintext until encrypted; transfer it through approved encrypted backup tooling and restrict retention/access. A successful dump listing is not a restore test.

Restore first into a **separate, empty, isolated installation**, with matching release/config/secrets and PostgreSQL major version 17. Restore the private archive only into that empty deployment directory after inspecting it, preserve owner-only key permissions, and set UID/GID to the restored file owner. Do not run `init-secrets.sh`. Do not extract over a working installation. Use an isolated hostname/network and prevent real email/device traffic during verification.

After the matching files/secrets/images are in place, from the isolated deployment directory:

```bash
set -e
dc=(docker compose -f compose.yaml -f compose.production.yaml)
backup_dir="/absolute/path/to/verified-backup"
"${dc[@]}" up -d postgres
"${dc[@]}" exec -T postgres pg_isready -U yoauthorize -d yoauthorize
# Only into an empty database; fail rather than overwrite existing objects.
"${dc[@]}" exec -T postgres pg_restore --exit-on-error --single-transaction --no-owner -U yoauthorize -d yoauthorize < "$backup_dir/database.dump"
# Use the migration binary built from the release selected for this restore.
"${dc[@]}" run --rm --no-deps migrate
"${dc[@]}" up -d redis signer api web
"${dc[@]}" ps -a
"${dc[@]}" logs --tail=100 migrate api signer
```

Wait for PostgreSQL readiness before restore; if `pg_isready` fails, wait and repeat it, not the restore. Do not start API or run migrations before loading the dump. Confirm signer/public metadata match, API health, expected records, and controlled SMTP behavior before starting Caddy and directing traffic to the restored environment. Existing sessions/device tokens from the backup may still be valid; account for that in a security recovery. No database drop, volume removal, or prune is needed.

## Upgrades And Rollback

Record source revision and immutable image digests/tags before upgrade; `:local` tags alone are not recoverable release identities. Prepare and retain old/new images, enter maintenance, stop writes, and take a verified pre-upgrade backup including keys/config. Run the **new release's** explicit migration binary before its API, then verify health, signer metadata, and controlled application workflows before reopening traffic.

SQLx migrations are embedded in the binary and tracked in the database. The repository provides forward migrations, not a general down-migration/rollback tool. Do not edit previously applied migration files or assume an old API can read a newer schema. If schema compatibility is not established, rollback means restoring the pre-upgrade database **and matching application release/config/keys** into an isolated replacement and switching traffic after verification. Writes made after that backup require a separate reconciliation plan and may be lost. Never regenerate keys, prune volumes, or run bootstrap as a rollback workaround.

## Troubleshooting

- Missing `dev-net`: use `start-development.sh` for development, not production; inspect existing driver/consumers before changing networks.
- Signer permission/startup failure: verify owner UID/GID and 0400/0600 seed permissions. Do not loosen key access.
- API missing-table error: inspect `migrate` exit/logs; startup does not migrate automatically.
- API key mismatch: recover matching seed/key ID and database metadata from trusted backups; do not overwrite metadata to silence the error.
- Login/CSRF 403: verify user status, origin, HTTPS/secure-cookie settings, and current JSON token from `/auth/me`; concurrent refreshes can invalidate tokens.
- Vite cannot reach API: Compose does not publish 8080. Use the [frontend development options](../frontend/README.md).
- Generic 500: inspect private API logs and dependency health. See [known API limitations](../backend/API.md#known-limitations) before assuming every invalid input returns 400.
