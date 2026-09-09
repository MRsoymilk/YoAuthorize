# Dockge (Local Administration)

Run `bash remote/dockge/start.sh` from the repository root, then open
http://localhost:5001 and create the first administrator account. This starts
only Dockge, not the authorization services. Docker Engine must be local.

The independent `dockge` Compose project reuses `dev-net` and keeps its account
database in the `dockge_dockge_data` volume. Do not remove this volume unless
you intend to reset the management installation.

## Existing Project

The startup script registers `yoauthorize` using a symlink to `remote/deploy`.
The full `remote` directory is mounted at the same absolute path inside Dockge,
preserving relative build contexts, secrets and Caddy mounts. Do not move this
checkout while Dockge is running. Use the stack scan action if needed.

Before starting the business stack, initialize secrets using
`remote/deploy/init-secrets.sh` only if its secrets directory does not exist.
The Dockge startup script initializes the external network but never generates
or overwrites business secrets. Stack controls use the development Compose
configuration; production's explicit two-file invocation remains CLI-managed.

Editing Compose or environment settings in Dockge edits this checkout directly.
Do not delete this stack from the UI as a way to stop it; use Stop/Down instead.
The successful one-shot `migrate` container may make the stack appear exited
even while its long-running services are healthy. Check individual services.

## Proxy and Security

The host Docker client configuration directory (`DOCKER_CONFIG` or
`$HOME/.docker`) is mounted read-only to reuse proxy and registry settings.
Host-specific credential helper executables may not exist inside Dockge.
Linux loopback build proxies still require `DOCKER_BUILD_NETWORK=host` in the
business stack's local `.env`. Dockge itself does not use the inherited runtime
HTTP proxy. Its runtime network remains a bridge.

Dockge has root-equivalent host access through the Docker socket and can read
the mounted project's secrets and Docker credentials. Only trusted machine
administrators should use it. Port 5001 binds only to host loopback; use an SSH
tunnel or VPN for remote access, not a public port mapping. Other projects are
not automatically imported or modified.

Run `docker logs -f dockge-dockge-1` to inspect logs, or
`docker stop dockge-dockge-1` to stop the management container without stopping
business services. Rerun the startup script to start it again.
