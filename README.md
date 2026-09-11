<p align="center">
  <img src="res/logo.png" alt="YoAuthorize logo" width="180">
</p>

# YoAuthorize

YoAuthorize contains a local C++20 licensing service and SDK, plus a remote licensing system with a Rust API, isolated Ed25519 signer, and React management UI. It is under development; the deployment examples are not a claim of production readiness.

## Start Here

| Area | Documentation |
| --- | --- |
| Architecture, protocol, security, and roadmap | [Design guide](guide/README.md) (includes future goals) |
| Native integration | [Local demo](guide/docs/13-local-demo.md), [C++ SDK](guide/docs/08-sdk.md), [License Service](guide/docs/09-service.md), [Qt sample](samples/TestYoQt/README.md) |
| Remote installation and operations | [Deployment](remote/deploy/README.md) |
| Rust services and configuration | [Backend](remote/backend/README.md) |
| HTTP and device WebSocket integration | [Public API guide](remote/backend/API.md) |
| Browser UI development | [Frontend](remote/frontend/README.md) |
| Optional local service controls | [Host Manager](remote/manager/README.md) |

## Local C++ Build

Use a C++20 compiler, CMake (top-level minimum 3.14; dependencies may require newer), OpenSSL 3.x development libraries, and platform thread support. Initialize pinned submodules rather than adding or checking out arbitrary dependency versions:

```bash
git submodule update --init --recursive
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The service and SDK default to enabled. `YOAUTHORIZE_BUILD_TOOLS`, `YOAUTHORIZE_BUILD_SAMPLES` (Qt), and `YOAUTHORIZE_BUILD_LEGACY` default to disabled. See [CMakeLists.txt](CMakeLists.txt) and [license format](guide/docs/04-license-format.md).

Core dependencies include [FlatBuffers](https://github.com/google/flatbuffers), [tomlplusplus](https://github.com/marzer/tomlplusplus), [OpenSSL](https://www.openssl.org/), and [GoogleTest](https://github.com/google/googletest). Legacy/optional modules reference [spdlog](https://github.com/gabime/spdlog), [nng](https://github.com/nanomsg/nng), [SQLite](https://www.sqlite.org/download.html), [MySQL Connector/C++](https://github.com/mysql/mysql-connector-cpp), and [MongoDB C++ driver](https://github.com/mongodb/mongo-cxx-driver). These are not all requirements for the default build.

## Remote Quickstart

For a fresh development checkout, install Bash, OpenSSL CLI, Docker Engine, and Docker Compose v2 >= 2.24.4. Run as a non-root user with Docker access:

```bash
cd remote/deploy
./init-secrets.sh
./start-development.sh -d --build
```

By default, open <http://localhost:8088>; development mail is captured at <http://localhost:8025>. Fresh initialization creates `remote/deploy/.env` with your UID/GID, centralized host port defaults, and the Vite API proxy target; an existing `.env` is left untouched. Process environment overrides `.env`, which overrides defaults. Configure `YOAUTHORIZE_HTTP_PORT`, `MAILPIT_HTTP_PORT`, `MANAGER_PORT`, `FRONTEND_DEV_PORT`, and `FRONTEND_API_TARGET` there as needed. When `PUBLIC_URL` is absent, Compose derives `http://localhost:<YOAUTHORIZE_HTTP_PORT>`; an explicit `PUBLIC_URL` remains authoritative. See the [deployment configuration](remote/deploy/README.md#configuration-and-layout) for custom-port examples and which services/processes to restart after edits.

Set `BOOTSTRAP_ADMIN_EMAIL` in `remote/deploy/.env`, then run `docker compose --profile tools run --rm bootstrap-admin` from that directory. Retrieve the generated password privately from `secrets/bootstrap-admin-password`.

Do not rerun secret initialization on an existing installation or replace its keys. Read the [deployment guide](remote/deploy/README.md) for existing environments, production TLS/SMTP, backups, or upgrades. Compose runs the explicit migration tool before the API; the API itself does not migrate the database.
