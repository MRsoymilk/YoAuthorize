# Remote Frontend

React/TypeScript SPA built with Vite. See [deployment](../deploy/README.md) for the packaged stack and [public API](../backend/API.md) for its backend contract.

## Development

Use Node.js/npm compatible with `package-lock.json`; the Docker build uses Node 26 Alpine. From `remote/frontend`:

```bash
npm ci
npm run dev
```

For the `serve` command outside test mode, Vite reads server-only development settings from the central `remote/deploy/.env` using Vite's standard `loadEnv` behavior (including mode-specific/local files and variable expansion). Builds and test mode skip deploy loading and dev-setting validation. The directory is resolved relative to `vite.config.ts`, not the shell's working directory. Existing process environment values override the file values. Restart Vite after changing settings.

| Setting | Default | Constraints |
| --- | --- | --- |
| `FRONTEND_DEV_PORT` | `5173` | Digits only, integer 1 through 65535; Vite fails if occupied rather than choosing another port |
| `FRONTEND_API_TARGET` | `http://localhost:8088` | HTTP(S) origin, optional trailing `/`; no credentials, other path, query, or fragment |

The default proxy forwards `/api` unchanged to **Compose Caddy**, which routes API requests independently of the development hostname. `changeOrigin: true` sends the target's Host header; the configured hostname is retained rather than replaced with an IP. Compose does not need to publish the API's port 8080. Do not expose the signer to make frontend development work. The Compose UI at `http://localhost:8088` is also available without Vite.

For a host Rust API with host-reachable dependencies, follow [backend development](../backend/README.md) and override the proxy target:

```bash
FRONTEND_API_TARGET=http://localhost:8080 npm run dev
```

Set the API's `PUBLIC_URL` to the browser-facing frontend origin (for example, `http://localhost:5173`) when verification/reset email links should return to Vite. This applies to the Compose API too: retaining the Compose origin sends email links to the Compose UI, not Vite. The proxy target and `PUBLIC_URL` serve different purposes; changing the dev port does not update `PUBLIC_URL`. Apply API environment changes by restarting/recreating the API as appropriate. Use `COOKIE_SECURE=false` only for local HTTP. Vite proxies `/api`, not `/health`, `/docs`, or `/api-docs`; access these through the backend/Caddy origin.

Only the two selected deploy keys are used in Vite's server configuration. The client env directory remains `remote/frontend`, with the default public `VITE_` prefix; deploy variables are not copied into client definitions. Never put secrets in `VITE_` variables. The proxy target is an explicit developer setting, not a URL accepted from external requests.

## Commands

| Command | Actual script |
| --- | --- |
| `npm run dev` | `vite` |
| `npm run build` | `tsc -b && vite build`; output `dist/` |
| `npm test` | `vitest run` (jsdom, Testing Library setup) |
| `npm run test:watch` | `vitest` |
| `npm run preview` | `vite preview`; requires a prior build |

There is no lint script. Preview is a local build inspection tool, not the deployed API gateway; do not assume it provides a working API proxy. Use Caddy or an explicitly configured same-origin gateway for integrated testing of built assets.

## API And Sessions

`src/lib/api.ts` defaults `VITE_API_BASE_URL` to `/api/v1`, removes one trailing slash, includes cookies, and sends the in-memory CSRF token on unsafe methods. This variable is substituted at **build time** (or Vite dev startup), is public, and must never contain credentials. Changing a running Nginx container's environment does not change the bundle. The Dockerfile has no dedicated build argument for it.

Keep browser requests same-origin. An absolute cross-origin base URL does not enable CORS: the backend has no configured CORS middleware, and cookie/CSRF rules still apply. Neither Nginx nor a preview command replaces production Caddy API routing.

Login returns a CSRF token in JSON and a response header. `api.me()` reads the replacement token from JSON; `/auth/me` rotates it without a response token header. Concurrent refreshes or tabs sharing the cookie can invalidate tokens and cause 403 responses. Serialize refresh and mutations in integrations; the UI does not coordinate across tabs. Guards control navigation; the backend enforces authorization.

## Pages And Delivery

Public pages: `/login`, `/register`, `/verify-email`, `/forgot-password`, `/reset-password`. Signed-in pages: `/app`, `/app/licenses`, `/app/devices`. Admin pages: `/admin/users`, `/admin/products`, `/admin/licenses`, `/admin/codes`, `/admin/audit`. `/` redirects to login; unknown paths redirect to `/`.

The image serves `dist/` with Nginx on container port 80. `nginx.conf` provides SPA history fallback, no-cache `index.html`, long-lived static asset caching, and `/healthz`. Caddy sends `/api/*`, `/health/*`, `/docs*`, and `/api-docs/*` to the API and everything else to Nginx. The web service has a read-only filesystem with selected tmpfs mounts; the supplied Nginx service is not explicitly configured non-root.

Device activation/WebSocket integration is separate from account UI. Browser `WebSocket` cannot set the required arbitrary `Authorization` header; cookie login does not authenticate device events. See [API.md](../backend/API.md); do not put device tokens in URLs.
