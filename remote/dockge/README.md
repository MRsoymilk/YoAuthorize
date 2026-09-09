# Dockge (Local Administration)

Run `bash remote/dockge/start.sh` from the repository root, then open
http://localhost:5001 and create the first administrator account. This starts
only Dockge, not the authorization services. Docker Engine must be local.

The independent `dockge` Compose project reuses `dev-net` and keeps its account
database in the `dockge_dockge_data` volume. Do not remove this volume unless
you intend to reset the management installation.

## Existing Project

The startup script registers `yoauthorize` using a symlink to `remote/deploy`.
The startup script locates `remote` from its own location, mounts it at the same
absolute path inside Dockge, and supplies `YOAUTHORIZE_REMOTE_DIR` to Compose.
Build contexts, secrets and Caddy mounts use this automatically resolved path
instead of resolving `..` against the stack symlink. No checkout path needs to
be entered in `.env` or committed. Ordinary CLI deployments keep relative-path
defaults. Do not move this checkout while Dockge is running; on a new machine,
rerun the startup script from the new checkout. Use the stack scan action if needed.

Before starting the business stack, initialize secrets using
`remote/deploy/init-secrets.sh` only if its secrets directory does not exist.
The Dockge startup script initializes the external network but never generates
or overwrites business secrets. Stack controls use the development Compose
configuration; production's explicit two-file invocation remains CLI-managed.

`remote/deploy/compose.yaml` remains the stack entry point, declaring all nine
services through `extends` (`file`/`service`) references to same-directory files:
`compose.backend.yaml` (api, signer, migrate, bootstrap-admin),
`compose.infrastructure.yaml` (postgres, redis, mailpit), and `compose.web.yaml`
(web, caddy). The main file retains the project name and all shared networks,
volumes, and secrets. Keep the fragments beside it; no extra stack registration
or `-f` arguments are needed for this split.

Editing Compose or environment settings in Dockge edits this checkout directly.
The UI's Compose editor edits the main file, not the referenced fragments. Edit
service definitions in their fragment using a filesystem editor; the UI does
not provide an editor for those files.
Do not delete this stack from the UI as a way to stop it; use Stop/Down instead.
The successful one-shot `migrate` container may make the stack appear exited
even while its long-running services are healthy. Check individual services.

## Proxy and Security

Only `config.json` from the host Docker client configuration directory
(`DOCKER_CONFIG` or `$HOME/.docker`) is mounted read-only to reuse proxy and
registry settings. If absent, the startup script supplies an empty local config.
The rest of `/root/.docker` uses the writable `dockge_docker_client_data` volume
so Buildx can create its state without modifying the host Docker directory.
Host-specific credential helper executables may not exist inside Dockge.
Linux loopback build proxies still require `DOCKER_BUILD_NETWORK=host` in the
business stack's local `.env`. Dockge itself does not use the inherited runtime
HTTP proxy. Its runtime network remains a bridge.

BuildKit is enabled by default. If Docker Hub token requests from Dockge time
out in a loopback-proxy environment, `DOCKGE_BUILDKIT=0` in this directory's
ignored `.env` selects the legacy builder on Docker versions that still support
it. Rerun `start.sh` to apply. This is a local workaround, not a replacement for
configuring reachable proxy access for BuildKit.

Dockge has root-equivalent host access through the Docker socket and can read
the mounted project's secrets and Docker credentials. Only trusted machine
administrators should use it. Port 5001 binds only to host loopback; use an SSH
tunnel or VPN for remote access, not a public port mapping. Other projects are
not automatically imported or modified.

Run `docker logs -f dockge-dockge-1` to inspect logs, or
`docker stop dockge-dockge-1` to stop the management container without stopping
business services. Rerun the startup script to start it again.

Run `node remote/dockge/test-paths.mjs` to check Compose path resolution in a
temporary relocated checkout with spaces, including the logical stack path and
production override. This test does not start containers or copy secrets.
