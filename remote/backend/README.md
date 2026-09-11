# Remote Backend

Rust workspace for the public authorization API, YALC v1 FlatBuffers license format, and isolated Ed25519 signer. See [deployment](../deploy/README.md), [public API](API.md), and [frontend](../frontend/README.md).

## Components

| Component | Responsibility |
| --- | --- |
| `authorization-api` | Accounts, cookie sessions, admin operations, activation, device events; PostgreSQL persistence and Redis rate limits |
| `migrate` binary | Explicit embedded SQLx migrations; must finish before API/bootstrap startup |
| `bootstrap-admin` binary | Creates an active administrator; refuses if any administrator already exists |
| `license-signer` | Holds the private signing seed; signs structured definitions, never arbitrary bytes |
| `license-format` | Generates/verifies Ed25519-signed YALC v1 packages; [schema](schema/license.fbs) |

The API does **not** run migrations on startup. It contacts the signer and automatically inserts its public key metadata when no eligible active key exists. If active metadata exists, the signer key ID and public bytes must match; startup fails rather than silently replacing them. Treat active metadata as immutable. There is no public key-management/rotation endpoint. Never mount `SIGNING_KEY_FILE` into the API.

## Host Development

Use Rust/Cargo supporting edition 2024 and the locked dependencies; the container build uses Rust 1.97 on Debian Bookworm. Host builds also need a native compiler/linker. Supply reachable PostgreSQL (Compose uses 17), Redis (8), SMTP, and signer. Compose service DNS names and unpublished ports are not directly reachable from the host.

Run from `remote/backend`, supplying the appropriate environment separately for each process:

```bash
cargo run --locked -p authorization-api --bin migrate
# Separate terminal, signer-only environment:
cargo run --locked -p license-signer
# After migrations and signer startup, API environment:
cargo run --locked -p authorization-api --bin authorization-api
# Once, bootstrap environment:
cargo run --locked -p authorization-api --bin bootstrap-admin
```

Binaries read process environment, **not automatic dotenv files**. [.env.example](.env.example) is a reference, not a ready-to-source shell script (`MAIL_FROM` contains spaces). Use exported, quoted variables or a trusted environment loader; keep secrets out of shell history. For host-only use set API `LISTEN_ADDR=127.0.0.1:8080` and signer `SIGNER_LISTEN_ADDR=127.0.0.1:8090` explicitly.

## Configuration

| API variable | Meaning/default |
| --- | --- |
| `LISTEN_ADDR` | `0.0.0.0:8080` |
| `DATABASE_URL` | Required PostgreSQL connection string |
| `REDIS_URL` | Required Redis URL |
| `PUBLIC_URL` | Required external origin; trailing slashes removed; used for mail and WebSocket URLs |
| `COOKIE_SECURE` | Boolean `true` by default; HTTPS `PUBLIC_URL` rejects `false` |
| `ACTIVATION_PEPPER` | Required, at least 32 bytes; preserve across restores |
| `SIGNER_URL` | Required internal HTTP URL |
| `SIGNER_SHARED_SECRET` | Required, at least 32 bytes, matching signer |
| `SMTP_URL` | Required SMTP transport URL |
| `MAIL_FROM` | Required sender accepted by SMTP server |

Required API values support `NAME_FILE`: trimmed file contents take precedence over `NAME`. `LISTEN_ADDR` and `COOKIE_SECURE` do not use that mechanism. `RUST_LOG` controls tracing.

Signer variables: `SIGNER_LISTEN_ADDR` defaults to **`0.0.0.0:8090`**; `SIGNING_KEY_ID` is required (1-255 bytes); `SIGNING_KEY_FILE` is required and contains exactly 32 raw seed bytes or their standard Base64 encoding. On Unix the signer rejects group/other-accessible key files: use owner-only `0400` or `0600`. `SIGNER_SHARED_SECRET_FILE` takes precedence over `SIGNER_SHARED_SECRET` and trims contents.

Bootstrap needs `DATABASE_URL` or `_FILE`, `BOOTSTRAP_ADMIN_EMAIL`, optional `BOOTSTRAP_ADMIN_NAME` (default `Administrator`), and `BOOTSTRAP_ADMIN_PASSWORD` or `_FILE` (at least 14 bytes). Run one bootstrap process only, after migration. This is provisioning, not an HTTP endpoint.

## Internal Signer

This is a separate trust boundary, **not part of the public API**. Keep its listener private; never publish its container port or proxy `/internal/*` through Caddy. The API authenticates with a shared-secret bearer credential, not a user session or device token.

`GET /internal/v1/signing-key` returns `{key_id, public_key}` (standard Base64 public bytes). `POST /internal/v1/licenses/sign` accepts `{definition: ...}` and returns `{license, key_id}` (standard Base64 package). The definition uses snake_case fields from [license-format](crates/license-format/src/lib.rs): identifiers, Unix-second times, `license_type` (`trial`, `subscription`, `permanent`), feature-code array, positive `max_sessions`, and optional/nullable `machine_id`. Permanent expiration is zero. Limits include 256 features, 1024-byte identifier/feature/machine strings, a 128 KiB signer request body, and a 1 MiB generated package. Internal failures use their own `{code, message?}` shape: 401 for failed authentication, 400 for invalid definitions. `/health/live` needs no credential.

The API verifies returned signatures against stored public metadata before persisting new activations. Discovery is automatic at startup, not a manual SQL registration step. Coordinate key changes with client trust distribution and stored metadata; regenerating the seed breaks startup against existing active metadata.

## Developer Checks

```bash
cargo fmt --all --check
cargo clippy --workspace --all-targets -- -D warnings
cargo test --workspace
```

Public health routes are `/health/live` and `/health/ready`; readiness checks PostgreSQL and Redis, not ongoing signer/SMTP health. Swagger UI is `/docs`, with JSON at `/api-docs/openapi.json`. Use [API.md](API.md) for integration semantics and known limitations. Swagger's normal request runner does not implement device WebSocket sessions.
