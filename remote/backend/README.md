# YoAuthorize remote backend

Rust workspace containing the public API, YALC v1 FlatBuffers implementation, and an isolated Ed25519 signer. The API stores only public signing-key metadata. It sends structured license definitions to the signer over its authenticated internal endpoint.

## Development

```sh
cargo fmt --all --check
cargo clippy --workspace --all-targets -- -D warnings
cargo test --workspace
```

Set the variables documented in `.env.example`. Migrations are embedded and run when the API starts. Create the first administrator exactly once with `cargo run -p authorization-api --bin bootstrap-admin`; the command refuses to run after any administrator exists.

Generate a signer seed with a secret-management tool as 32 random bytes (raw or standard Base64), mount it read-only with mode `0400`, and register the corresponding 32-byte Ed25519 public key in `signing_keys`. Never provide `SIGNING_KEY_FILE` to the API container.

The signer image is non-root and the container filesystem is read-only. In Compose it runs as `LOCAL_UID:LOCAL_GID` because local secrets are bind-mounted with host ownership; the signing key must be owned by that UID and have mode `0400` or `0600`. Shared secrets needed by the fixed-UID API must be readable by that UID but remain non-writable. `BOOTSTRAP_ADMIN_PASSWORD_FILE` is supported by the one-shot bootstrap command so its password does not need to be exposed in an environment variable.

The WebSocket endpoint accepts `Authorization: Bearer <device token>` and emits `license.updated`, `license.revoked`, `device.unbound`, `features.changed`, and `server.ping` JSON events. Durable events are read from `outbox_events`; an external publisher may also consume that table using `published_at` and `attempts`.
