# Remote Frontend

React/TypeScript SPA built with Vite. See [deployment](../deploy/README.md) for the packaged stack and [public API](../backend/API.md) for its backend contract.

## Development

Use Node.js/npm compatible with `package-lock.json`; the Docker build uses Node 26 Alpine. From `remote/frontend`:

```bash
npm ci
npm run dev
```

Vite requests port 5173 (it may select another if occupied). Its development proxy forwards `/api` to **`http://localhost:8080`**. Compose does **not** publish API port 8080, so starting Compose and Vite alone does not connect this proxy to the API.

For the unchanged Vite configuration, run a host API on `127.0.0.1:8080` with host-reachable dependencies as described in [backend development](../backend/README.md). Alternatively use an operator-managed loopback forwarding proxy at 8080 to Compose Caddy at `http://localhost:8088`, with the appropriate upstream Host header. Do not expose the signer to make frontend development work. The simplest full-stack option is the Compose UI at `http://localhost:8088`, without Vite.

Set the host API's `PUBLIC_URL` to the SPA origin so mail verification/reset links return to Vite. Use `COOKIE_SECURE=false` only for local HTTP. Vite proxies `/api`, not `/health`, `/docs`, or `/api-docs`; access these through the backend/Caddy origin.

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
