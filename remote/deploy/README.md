# Deployment

Initialize development secrets and start the stack:

```sh
./init-secrets.sh
./start-development.sh --build
```

The development startup script creates or reuses the external `dev-net` bridge and validates its driver before starting Compose. Development publishes Caddy only on `127.0.0.1:8088` by default. Mailpit is the default SMTP server, with its UI on `127.0.0.1:8025`. PostgreSQL/Redis and the signer remain on dedicated internal networks.

Create the initial administrator by setting `BOOTSTRAP_ADMIN_EMAIL` and running the tools profile. The generated password is stored in `secrets/bootstrap-admin-password` and is mounted only into this one-shot container; it is never passed through the environment.

```sh
docker compose --profile tools run --rm bootstrap-admin
```

## Build proxy on Linux

Docker automatically supplies build proxy arguments from `~/.docker/config.json`.
If that proxy listens on host loopback, set `DOCKER_BUILD_NETWORK=host` in the
local `.env` before running `docker compose build` or the development startup
script. This allows build steps to reach the host's proxy without duplicating
its address in Compose or embedding proxy environment variables in images.
The default build network remains `default` on other machines. Runtime services
still use bridge networks; this option does not enable host networking for them.

## Production

Set these values in `.env` before starting production:

```dotenv
PUBLIC_URL=https://licenses.example.com
COOKIE_SECURE=true
SMTP_URL=smtps://username:password@smtp.example.com:465
MAIL_FROM=YoAuthorize <licenses@example.com>
```

An HTTPS `PUBLIC_URL` requires `COOKIE_SECURE=true`. `SMTP_URL` defaults to the Mailpit alias `yoauthorize-mailpit` for development and must identify the production SMTP service in production; set `MAIL_FROM` to a sender accepted by that service.

Start production with the override, which replaces the development listener with public HTTP, HTTPS, and HTTP/3 ports:

```sh
docker compose -f compose.yaml -f compose.production.yaml up -d --build
```

The production override replaces external `dev-net` with a project-owned bridge so independent deployments remain isolated and do not require development network initialization.

## Secret ownership

Containers remain non-root and read-only. Compose bind-mounts local secret files without remapping their ownership. The signer runs as `LOCAL_UID:LOCAL_GID` so it can read the owner-only (`0600`) signing key while the signer rejects keys accessible by group or others. Set `LOCAL_UID` and `LOCAL_GID` to the owner of `secrets/license-signing-key`; `init-secrets.sh` records the invoking user's IDs. The bootstrap tool uses the same runtime identity so it can read its owner-only password secret. API-consumed shared secrets are non-writable and readable by its fixed unprivileged UID.
