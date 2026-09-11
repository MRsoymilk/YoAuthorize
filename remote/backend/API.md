# Public API Guide

Base path: `/api/v1`. Use the same external origin as the UI (development: `http://localhost:8088`). This guide covers **29 method/path operations on 25 distinct paths**, including aliases and the two root-level health endpoints, excluding documentation endpoints. The source of behavior is [routes.rs](crates/authorization-api/src/routes.rs), [errors](crates/authorization-api/src/error.rs), and [database constraints](crates/authorization-api/migrations/0001_initial.sql).

Swagger UI: `/docs`; OpenAPI JSON: `/api-docs/openapi.json`. Consult this guide for workflow details and limitations even when using generated schemas. Normal Swagger UI cannot establish/test a WebSocket stream. The [internal signer API](README.md#internal-signer) is separate and must not be exposed publicly.

## Authentication

| Access label | Requirement |
| --- | --- |
| Public | No session or CSRF requirement |
| Session | Active user and valid `ya_session` cookie; both `user` and `admin` roles allowed |
| Session + CSRF | Session plus `X-CSRF-Token` matching the current session token |
| Admin | Session whose role is `admin` |
| Admin + CSRF | Admin session plus current `X-CSRF-Token` |
| Device | `Authorization: Bearer <device-access-token>` on WebSocket upgrade, not a web cookie |

Login sets `ya_session` with `Path=/; HttpOnly; SameSite=Lax; Max-Age=43200` and conditional `Secure`. The database session expires after 12 hours. Login returns `csrfToken` in JSON **and** `x-csrf-token` in the response header. `GET /auth/me` requires only the cookie, rotates CSRF state on every call, and returns the new token **only in JSON**, not a token response header. It does not extend session expiration. Logout requires CSRF, deletes that session, and clears its cookie. Password reset deletes all of the user's web sessions.

Bearer device tokens cannot authenticate account/admin routes; admin cookies cannot substitute for a device token. Admin status does not expand ownership on `/licenses`, `/devices`, or device deletion: these remain scoped to that administrator's own licenses.

## Operation Inventory

Paths below are relative to `/api/v1` except the explicitly marked root-level health paths. Bodies and models are defined in the following sections. `204` and `202` responses here have no body.

| Method | Path | Access | Request | Success |
| --- | --- | --- | --- | --- |
| POST | `/auth/register` | Public | Registration | 202 |
| POST | `/auth/verify-email` | Public | `{token}` | 204 |
| POST | `/auth/login` | Public | `{email,password}` | 200 `{csrfToken}` + cookie/header |
| POST | `/auth/logout` | Session + CSRF | None | 204 + cleared cookie |
| GET | `/auth/me` | Session | None | 200 `{user,csrfToken}` |
| POST | `/auth/forgot` | Public | `{email}` | 202 |
| POST | `/auth/forgot-password` | Public | Same alias | 202 |
| POST | `/auth/reset` | Public | `{token,password}` | 204 |
| POST | `/auth/reset-password` | Public | Same alias | 204 |
| GET | `/licenses` | Session | None | 200 `License[]` |
| GET | `/devices` | Session | None | 200 `Device[]` |
| DELETE | `/devices/{id}` | Session + CSRF | UUID path | 204 |
| GET | `/admin/users` | Admin | None | 200 `Page<User>` |
| PATCH | `/admin/users/{id}` | Admin + CSRF | UUID path, UserUpdate | 204 |
| GET | `/admin/products` | Admin | None | 200 `Page<Product>` |
| POST | `/admin/products` | Admin + CSRF | ProductInput | 201 `{id}` |
| GET | `/admin/features` | Admin | None | 200 `Page<Feature>` |
| POST | `/admin/features` | Admin + CSRF | FeatureInput | 201 `{id}` |
| GET | `/admin/licenses` | Admin | None | 200 `Page<License>` |
| POST | `/admin/licenses` | Admin + CSRF | LicenseInput | 201 `{id,key}` |
| PATCH | `/admin/licenses/{id}` | Admin + CSRF | UUID path, `{status}` | 204 |
| GET | `/admin/activation-codes` | Admin | None | 200 `Page<ActivationCode>` |
| POST | `/admin/activation-codes` | Admin + CSRF | CodeInput | 201 `{codes: string[]}` |
| GET | `/admin/audit` | Admin | None | 200 `Page<Audit>` |
| GET | `/admin/audit-logs` | Admin | Same alias | 200 `Page<Audit>` |
| POST | `/activations` | Public | ActivationRequest | 200 ActivationResponse |
| GET | `/device/events` | Device | WebSocket upgrade | 101, JSON text events |
| GET | `/health/live` (root-level) | Public | None | 200 `{status}` |
| GET | `/health/ready` (root-level) | Public | None | 200/503 `{status,postgres,redis}` |

Rows combining GET/POST are deliberately separate operations. Aliases execute the same handler and share behavior/rate-limit scope. No public product/feature update/delete, license-detail GET, activation-code revoke, or device-token refresh endpoint is implemented.

Additional unauthenticated endpoints: `GET /health/live` returns 200 `{"status":"ok"}`; `GET /health/ready` returns 200 or 503 with `{status,postgres,redis}` where the last two are booleans and status is `ok` or `unavailable`. Readiness checks only PostgreSQL/Redis, not SMTP or continued signer availability.

## Requests And Validation

JSON bodies require `Content-Type: application/json`; public API request bodies are capped at 1 MiB. Field names are case-sensitive. Account/admin JSON is camelCase where fields have multiple words; activation JSON is snake_case. IDs are UUID strings except activation `product_id`, which is the **product code**, not its UUID. Timestamps are RFC 3339 strings unless explicitly described as Unix seconds. Length checks in Rust use **UTF-8 bytes**, not character counts.

All fields are required and non-null unless marked optional below. Optional request fields may be omitted or `null`. Unknown JSON fields are not explicitly rejected by the request structs. No filtering/pagination query parameters are implemented.

| Body | Fields and constraints |
| --- | --- |
| Registration | `name`: nonblank after trim, at most 200 bytes before trim; `email`: trimmed, ASCII-lowercased, at most 320 bytes, basic `@`/domain-dot check; `password`: 10-1024 bytes |
| Login | `email` normalized as above; `password` string; only active accounts may log in |
| Verification/reset | `token`: opaque email token; reset `password`: 10-1024 bytes |
| UserUpdate | Optional `status`: `pending`, `active`, `disabled`, or alias `suspended`; optional `role`: `user` or `admin`. Null/omitted leaves unchanged; `{}` allowed |
| ProductInput | `name`, `code`: strings; optional `description`: string. Code 1-100 ASCII bytes; use letters, digits, `_`, `-` (see mismatch below). No handler name/description length validation |
| FeatureInput | `productId`: UUID; `name`: string; `code`: 1-100 ASCII letters/digits/`_`/`.`/`-` |
| LicenseInput | `productId`: UUID; optional `userId`: UUID; `licenseType`: `trial`, `subscription`, `permanent`; optional `expiresAt`: timestamp; `deviceLimit`: integer 1-10000; optional `maxSessions`: integer, default 1, database requires 1-10000; `featureIds`: UUID array (may be empty) |
| License status update | `status`: `active` or `revoked`; this endpoint does not edit features, expiration, or limits |
| CodeInput | `productId`: UUID; optional `licenseId`: UUID matching product; optional `quantity`: unsigned integer 1-100, default 1; optional `activationLimit`: integer 1-10000, default 1; optional `expiresAt`: timestamp (no handler future-time validation) |
| ActivationRequest | `protocol_version`: unsigned 16-bit integer, must be 1; `product_id`: product code string; `activation_code`: string; `machine_id`: 1-1024 bytes; optional `idempotency_key`: 1-200 bytes |

Permanent licenses require absent/null `expiresAt`; trial/subscription require a future expiration. Database expiration must also follow `not_before`. Feature IDs from another product or missing IDs are silently skipped during license creation; duplicate matching IDs can fail a database constraint. The signer allows at most 256 features, but license creation does not enforce this upfront.

Registration for an existing pending email replaces outstanding verification tokens and resends mail without replacing its original name/password. Other existing accounts return 409 `EMAIL_EXISTS`. Verification tokens expire in 24 hours, reset tokens in one hour, and are single use. Forgot-password normally returns 202 even for unknown/inactive accounts, although SMTP/infrastructure failures can still return 500. Mail is sent after database changes, so a failed send is not proof that nothing was persisted.

## Response Models

`Page<T>` is `{items: T[], total: number}`. `total` is the number **returned**, not the full database count. Users, licenses, and activation-code lists cap at 500; audit caps at 1000; products/features and user devices have no explicit SQL cap. Account lists are bare arrays, not pages.

Fields below are always present. `?` here denotes a nullable value, not an omitted property. IDs are UUID strings unless noted.

| Model | Fields |
| --- | --- |
| `/auth/me` user | `id`, `email`, `name`, `role` (`user` or `admin`); no status or creation timestamp |
| User | `id`, `email`, `name`, `role`, `status` (`pending`, `active`, `suspended`), `createdAt` |
| Product | `id`, `code`, `name`, `description` string?, `featureCount` integer |
| Feature | `id`, `productId`, `code`, `name` |
| License | `id`, `key` (masked), `productName`, `status` (`active`, `expired`, `revoked`), `expiresAt` timestamp?, `deviceLimit`, `deviceCount`, `features` string[] of codes, `userEmail` string? |
| Device | `id`, `name`, `fingerprint` (masked machine hint), `platform` string?, `lastSeenAt`, `activatedAt`, `licenseId` |
| ActivationCode | `id`, `code` (hint, not redeemable full code), `productName`, `status` (`active`, `redeemed`, `disabled`), `createdAt`, `redeemedBy` string? |
| Audit | `id` decimal string (not UUID), `actorEmail` string?, `action`, `target` string `type:id`, `ipAddress` string?, `createdAt` |
| ActivationResponse | `license`: standard Base64 YALC v1 package; `realtime_url`: `ws://`/`wss://` URL; `access_token`: opaque device token; `token_expire_time`: integer Unix seconds |

License creation returns an unmasked `key`; list endpoints mask it. Full activation codes appear only in the creation response and cannot be recovered from the list. Store/distribute them securely. `redeemedBy` is the linked license owner's email, not a redemption-history record. Device `activatedAt` currently comes from device creation time, not the activation row; a device can appear for multiple licenses. Audit IPs are nullable and current handlers do not populate them.

Disabling/suspending a user deletes their web sessions. License revocation emits events but does not erase issued offline packages. Device deletion unbinds all active activations for that device on the caller's licenses, revokes their device tokens, decrements activation-code counts, and emits events; it returns 404 if none match. User/license PATCH handlers do not check affected-row counts, so a nonexistent UUID may still yield 204.

## Activation And Events

Activation requires possession of a valid code, not account login. A code with no license creates an unassigned permanent license on first activation, with device limit equal to the code's activation limit and `max_sessions=1`. It does not assign an account or grant all product features automatically.

The server validates product, code, license status/start/expiry, code capacity, and license device count. Reusing a live activation for the same license/device returns its stored signed blob and a **new 24-hour device token**, not a byte-identical response. An idempotency key is scoped to the activation code; reuse for another machine returns 409 `IDEMPOTENCY_MISMATCH`, and reuse after unbinding returns 409 `ACTIVATION_INACTIVE`. Existing activation reuse does not consume another slot. Old tokens are not automatically revoked by a successful retry. There is no separate refresh endpoint; an eligible activation retry issues a token.

Connect to `realtime_url` with a native/server WebSocket client capable of sending `Authorization: Bearer <access_token>` during upgrade. A normal HTTP GET is not enough. Browser `WebSocket` cannot set this arbitrary header; no cookie, query-token, or subprotocol-token alternative is implemented. Do not leak tokens into URLs. Use `wss://` outside local development.

The server polls durable `outbox_events` and rechecks authorization every 15 seconds (initial tick is immediate), reading up to 100 events per poll. JSON text payloads include:

```json
{"type":"license.updated","license_id":"00000000-0000-4000-8000-000000000001"}
```

`license.revoked` also carries `license_id`; `device.unbound` carries `device_id`. `server.ping` carries `time` as a timestamp and is an application text event, not a WebSocket ping frame. `features.changed` is a recognized outbox topic but there is no public feature-update producer or fixed payload beyond the stored JSON. Unauthorized streams attempt terminal revoke/unbind event delivery and then close; delivery is not guaranteed.

Each connection starts its cursor at zero, so reconnects can replay history. There is no client resume cursor, acknowledgement protocol, or exactly-once promise. Make consumers tolerant of duplicates and reconcile authorization after reconnect. The stream reads the outbox directly; it does not mark `published_at`/`attempts`. Those fields are available for a separate publisher, not evidence one is deployed.

## Errors And Rate Limits

Application errors use this JSON envelope, without a nested `error` or guaranteed `details`:

```json
{"code":"FORBIDDEN","message":"You do not have permission to do that."}
```

| Status | Application codes/examples |
| --- | --- |
| 400 | `INVALID_EMAIL`, `INVALID_REGISTRATION`, `WEAK_PASSWORD`, `INVALID_TOKEN`, `INVALID_USER_UPDATE`, `INVALID_CODE` (code syntax), `INVALID_LICENSE`, `INVALID_STATUS`, `INVALID_QUANTITY`, `INVALID_LIMIT`, `PRODUCT_MISMATCH` (code creation), `INVALID_ACTIVATION` |
| 401 | `UNAUTHORIZED`, `INVALID_CREDENTIALS` |
| 403 | `FORBIDDEN` (including CSRF/inactive login), `INVALID_CODE` (activation), `PRODUCT_MISMATCH` (activation), `LICENSE_REVOKED`, `LICENSE_NOT_YET_VALID`, `LICENSE_EXPIRED` |
| 404 | `NOT_FOUND` for unmatched device deletion |
| 409 | `EMAIL_EXISTS`, `IDEMPOTENCY_MISMATCH`, `ACTIVATION_INACTIVE`, `ACTIVATION_LIMIT`, `DEVICE_LIMIT`, `ACTIVATION_CONFLICT` |
| 429 | `RATE_LIMITED` |
| 500 | `INTERNAL_ERROR`, generic message; details logged server-side |

This envelope is **not universal**. Axum JSON/path/WebSocket extractors, body-limit middleware, routing, and reverse proxies can produce non-JSON errors (for example 400 malformed JSON/UUID, 415 content type, 422 JSON data mismatch, 413 oversized body, or failed upgrade). Clients must branch on status/content type and tolerate absent JSON. Most SQL constraint errors become generic 500, not field-specific 400/409.

Redis counters enforce registration 3/hour per normalized email, login 10/15 minutes per normalized email, forgot-password 3/hour per normalized email, activation 10/10 minutes per raw machine ID and 1000/minute globally. Windows start when counters are first created. Aliases share counters; these are not IP-based limits. No `Retry-After` header is implemented. Redis failure fails affected operations rather than bypassing throttling.

## Safe Integration Examples

These are illustrative requests, not a script to run against a live deployment. All credentials, codes, IDs, and tokens are placeholders. Registration, login, activation, and admin requests mutate state; use an explicitly disposable environment for manual exercises.

Read-only liveness check:

```bash
curl --fail-with-body http://localhost:8088/health/live
```

Login request (capture the cookie and token privately):

```http
POST /api/v1/auth/login HTTP/1.1
Content-Type: application/json

{"email":"operator@example.invalid","password":"<password-from-secret-store>"}
```

Session recovery requires only the cookie; replace your CSRF token with the JSON response before a subsequent mutation:

```http
GET /api/v1/auth/me HTTP/1.1
Cookie: ya_session=<session-cookie>
```

Example admin code creation after creating a product and optionally a license:

```http
POST /api/v1/admin/activation-codes HTTP/1.1
Content-Type: application/json
Cookie: ya_session=<admin-session-cookie>
X-CSRF-Token: <latest-csrf-token>

{"productId":"00000000-0000-4000-8000-000000000001","licenseId":null,"quantity":1,"activationLimit":1,"expiresAt":null}
```

Device activation uses the product **code**:

```http
POST /api/v1/activations HTTP/1.1
Content-Type: application/json

{"protocol_version":1,"product_id":"example_product","activation_code":"<issued-code>","machine_id":"example-ascii-machine-id","idempotency_key":"example-request-1"}
```

## Known Limitations

- Product-code validation accepts `.` in the handler, but the products table rejects it. Use `[A-Za-z0-9_-]{1,100}`; a dotted code can currently produce 500 rather than a validation response.
- `maxSessions` is not range-validated by the license creation handler. The database enforces 1-10000, so out-of-range values currently produce 500. Supply a valid positive value; this is not a documented 400 validation guarantee.
- `/auth/me` CSRF rotation races with parallel requests and tabs sharing a session. Even response arrival order need not match database update order. Serialize refresh/mutations where possible and coordinate tabs; do not blindly retry arbitrary mutations after a 403.
- Machine hints are masked using byte-index string slices. Non-ASCII machine IDs longer than 12 bytes can panic when a slice crosses a UTF-8 boundary. Use stable ASCII machine IDs until the implementation is corrected; the handler does not enforce ASCII.
- Lists are capped snapshots, not real pagination. Signer feature limits and database constraints are not consistently validated at public API input. Offline enforcement/revocation policy belongs to the consuming client; receiving a WebSocket event alone cannot invalidate an already-issued offline blob.

These are observed product limitations, not fixes made by this documentation change.
