//! Documentation-only wire models and assembly. Handlers continue to own serialization.
#![allow(dead_code)] // Response DTOs describe existing json! output; they are never instantiated.

use chrono::{DateTime, Utc};
use serde::Serialize;
use serde_json::json;
use utoipa::{OpenApi, ToSchema};
use uuid::Uuid;

use crate::routes;

#[derive(OpenApi)]
#[openapi(
    paths(
        routes::live, routes::ready,
        routes::register, routes::verify_email, routes::login, routes::logout, routes::me,
        routes::forgot_password, routes::reset_password,
        routes::user_licenses, routes::user_devices, routes::unbind_device,
        routes::admin_users, routes::admin_update_user,
        routes::admin_products, routes::admin_create_product,
        routes::admin_features, routes::admin_create_feature,
        routes::admin_licenses, routes::admin_create_license, routes::admin_update_license,
        routes::admin_codes, routes::admin_create_codes, routes::admin_audit,
        routes::activate, routes::device_events
    ),
    components(schemas(ApiErrorResponse, LiveResponse, ReadyResponse)),
    tags(
        (name = "Health", description = "Unauthenticated dependency probes"),
        (name = "Authentication", description = "Same-origin browser sessions"),
        (name = "User", description = "Resources owned by the active session user"),
        (name = "Admin", description = "Active session with role admin required for every operation. Writes also require the session CSRF token."),
        (name = "Device", description = "Activation and native device WebSocket protocol")
    )
)]
struct ApiDoc;

#[derive(Serialize, ToSchema)]
pub struct ApiErrorResponse {
    #[schema(example = "UNAUTHORIZED")]
    code: String,
    #[schema(example = "Authentication required.")]
    message: String,
}

#[derive(Serialize, ToSchema)]
pub struct LiveResponse {
    #[schema(example = "ok")]
    status: String,
}

#[derive(Serialize, ToSchema)]
pub struct ReadyResponse {
    /// ok or unavailable.
    status: String,
    postgres: bool,
    redis: bool,
}

#[derive(Serialize, ToSchema)]
#[serde(rename_all = "camelCase")]
pub struct LoginResponse {
    /// Send as X-CSRF-Token on protected writes. Replaced by each GET /auth/me.
    csrf_token: String,
}

#[derive(Serialize, ToSchema)]
pub struct SessionUser {
    id: Uuid,
    email: String,
    name: String,
    /// user or admin.
    role: String,
}

#[derive(Serialize, ToSchema)]
#[serde(rename_all = "camelCase")]
pub struct MeResponse {
    user: SessionUser,
    csrf_token: String,
}

#[derive(Serialize, ToSchema)]
#[serde(rename_all = "camelCase")]
pub struct LicenseSummary {
    id: Uuid,
    /// Masked key: first four and last four bytes separated by ..., or ******** for short values.
    #[schema(example = "YALC...ABCD")]
    key: String,
    product_name: String,
    /// revoked takes precedence over expired; otherwise active.
    status: String,
    #[schema(required = true)]
    expires_at: Option<DateTime<Utc>>,
    device_limit: i32,
    device_count: i64,
    /// Feature codes, not UUIDs, sorted by code.
    features: Vec<String>,
    #[schema(required = true)]
    user_email: Option<String>,
}

#[derive(Serialize, ToSchema)]
#[serde(rename_all = "camelCase")]
pub struct DeviceSummary {
    id: Uuid,
    name: String,
    /// Masked machine hint, not the original fingerprint or its hash.
    #[schema(example = "mach...1234")]
    fingerprint: String,
    #[schema(required = true)]
    platform: Option<String>,
    last_seen_at: DateTime<Utc>,
    /// Device creation timestamp, not the individual binding's activation time.
    activated_at: DateTime<Utc>,
    license_id: Uuid,
}

#[derive(Serialize, ToSchema)]
#[serde(rename_all = "camelCase")]
pub struct UserSummary {
    id: Uuid,
    email: String,
    name: String,
    /// user or admin.
    role: String,
    /// pending, active, or suspended (database disabled is mapped to suspended).
    status: String,
    created_at: DateTime<Utc>,
}

#[derive(Serialize, ToSchema)]
#[serde(rename_all = "camelCase")]
pub struct ProductSummary {
    id: Uuid,
    code: String,
    name: String,
    #[schema(required = true)]
    description: Option<String>,
    feature_count: i64,
}

#[derive(Serialize, ToSchema)]
#[serde(rename_all = "camelCase")]
pub struct FeatureSummary {
    id: Uuid,
    product_id: Uuid,
    code: String,
    name: String,
}

#[derive(Serialize, ToSchema)]
#[serde(rename_all = "camelCase")]
pub struct ActivationCodeSummary {
    id: Uuid,
    /// Stored hint: first seven and last four characters; never the full activation code.
    #[schema(example = "YA-ABCD...WXYZ")]
    code: String,
    product_name: String,
    /// disabled if disabled or expired, else redeemed if at limit, else active.
    status: String,
    created_at: DateTime<Utc>,
    /// Linked license owner's email, not necessarily the person who redeemed the code.
    #[schema(required = true)]
    redeemed_by: Option<String>,
}

#[derive(Serialize, ToSchema)]
#[serde(rename_all = "camelCase")]
pub struct AuditSummary {
    /// Decimal database integer serialized as a string, not a UUID.
    #[schema(example = "42")]
    id: String,
    #[schema(required = true)]
    actor_email: Option<String>,
    action: String,
    /// target_type:target_id; missing parts become empty strings.
    target: String,
    #[schema(required = true)]
    ip_address: Option<String>,
    created_at: DateTime<Utc>,
}

macro_rules! page_schema {
    ($name:ident, $item:ty) => {
        #[derive(Serialize, ToSchema)]
        pub struct $name {
            /// Number of returned items, not the total database count. No pagination parameters.
            total: usize,
            items: Vec<$item>,
        }
    };
}
page_schema!(UserPage, UserSummary);
page_schema!(ProductPage, ProductSummary);
page_schema!(FeaturePage, FeatureSummary);
page_schema!(LicensePage, LicenseSummary);
page_schema!(ActivationCodePage, ActivationCodeSummary);
page_schema!(AuditPage, AuditSummary);

#[derive(Serialize, ToSchema)]
pub struct CreatedId {
    id: Uuid,
}

#[derive(Serialize, ToSchema)]
pub struct CreatedLicense {
    id: Uuid,
    #[schema(example = "YALC-0123456789ABCDEF0123")]
    key: String,
}

#[derive(Serialize, ToSchema)]
pub struct CreatedCodes {
    /// Full secrets; retain securely because list operations return only hints.
    codes: Vec<String>,
}

pub fn swagger_config() -> utoipa_swagger_ui::Config<'static> {
    utoipa_swagger_ui::Config::default()
        .with_credentials(true)
        .persist_authorization(false)
        .try_it_out_enabled(false)
        .validator_url("")
}

pub fn document() -> utoipa::openapi::OpenApi {
    // Enrich the generated document rather than duplicating common contracts on every handler.
    let mut doc = serde_json::to_value(ApiDoc::openapi()).expect("OpenAPI serializes");
    doc["info"]["title"] = json!("YoAuthorize Authorization API");
    doc["info"]["description"] = json!(
        "Same-origin API. Browser sessions use an HttpOnly ya_session cookie and CSRF token, not bearer auth. Login returns CSRF in body and header; GET /auth/me rotates it in the body only. Swagger cannot set Cookie manually: log in on this origin and supply the latest CSRF token. Try it out is not enabled by default; authorization is not persisted. JSON bodies are limited to 1 MiB. Application errors are {code,message}; Axum extractor and body-limit rejections are plain text. Database constraint failures are 500, not automatically 404/409. No list accepts pagination parameters."
    );
    doc["servers"] = json!([{"url":"/", "description":"Same origin"}]);
    doc["components"]["securitySchemes"] = json!({
        "sessionCookie":{"type":"apiKey","in":"cookie","name":"ya_session","description":"Opaque 12-hour web session. HttpOnly; Path=/; SameSite=Lax; Secure when COOKIE_SECURE is enabled."},
        "csrfToken":{"type":"apiKey","in":"header","name":"X-CSRF-Token","description":"Latest token from login or /auth/me, bound to the session. Required together with sessionCookie for protected writes."},
        "deviceBearer":{"type":"http","scheme":"bearer","bearerFormat":"opaque","description":"24-hour opaque activation token, not JWT. Only for the native device WebSocket handshake."}
    });
    for (path, item) in doc["paths"].as_object_mut().unwrap() {
        for (method, op) in item.as_object_mut().unwrap() {
            let id = op["operationId"].as_str().unwrap().to_owned();
            let admin = path.starts_with("/api/v1/admin/");
            let protected = admin
                || matches!(
                    id.as_str(),
                    "logout" | "me" | "user_licenses" | "user_devices" | "unbind_device"
                );
            let health = path.starts_with("/health/");
            let websocket = id == "device_events";
            let tag = if health {
                "Health"
            } else if admin {
                "Admin"
            } else if path.contains("/auth/") {
                "Authentication"
            } else if id == "activate" || websocket {
                "Device"
            } else {
                "User"
            };
            op["tags"] = json!([tag]);
            op["summary"] = json!(id.replace('_', " "));
            op["security"] = if websocket {
                json!([{"deviceBearer":[]}])
            } else if protected && method != "get" {
                json!([{"sessionCookie":[],"csrfToken":[]}])
            } else if protected {
                json!([{"sessionCookie":[]}])
            } else {
                json!([])
            };
            if admin {
                op["description"] = json!(
                    "Requires an active session whose user has role admin. Writes require both the session cookie AND its current X-CSRF-Token. Lists return {total,items}; total is the returned count, not a database-wide count. No pagination parameters."
                );
            }
            if health {
                let schema = if id == "live" {
                    "LiveResponse"
                } else {
                    "ReadyResponse"
                };
                for response in op["responses"].as_object_mut().unwrap().values_mut() {
                    response["description"] = json!(if id == "live" {
                        "Process is alive; no dependency checks."
                    } else {
                        "PostgreSQL and Redis probes; 503 when either is unavailable. Signer and SMTP are not checked."
                    });
                    response["content"] = json!({"application/json":{"schema":{"$ref":format!("#/components/schemas/{schema}")}}});
                }
                continue;
            }
            let body = op.get("requestBody").is_some();
            let responses = op["responses"].as_object_mut().unwrap();
            let mut errors = vec![(
                500,
                "INTERNAL_ERROR",
                "The service is temporarily unavailable.",
            )];
            if protected || websocket {
                errors.push((401, "UNAUTHORIZED", "Authentication required."));
            }
            if admin || (protected && method != "get") || id == "login" {
                errors.push((403, "FORBIDDEN", "You do not have permission to do that."));
            }
            match id.as_str() {
                "register" => { errors.push((400,"INVALID_REGISTRATION","Invalid email, blank/oversized name, or password outside 10..1024 bytes (INVALID_EMAIL or INVALID_REGISTRATION).")); errors.push((409,"EMAIL_EXISTS","An account already exists for this email.")); }
                "login" => { errors.push((400,"INVALID_EMAIL","Enter a valid email address.")); errors.push((401,"INVALID_CREDENTIALS","Email or password is incorrect.")); }
                "forgot_password" => errors.push((400,"INVALID_EMAIL","Enter a valid email address.")),
                "verify_email" => errors.push((400,"INVALID_TOKEN","Token is invalid or expired.")),
                "reset_password" => errors.push((400,"INVALID_TOKEN","INVALID_TOKEN for invalid/expired/used token; WEAK_PASSWORD for password outside 10..1024 bytes.")),
                "admin_update_user" => errors.push((400,"INVALID_USER_UPDATE","Invalid role or status.")),
                "admin_create_product" | "admin_create_feature" => errors.push((400,"INVALID_CODE","Code contains invalid characters.")),
                "admin_create_license" => errors.push((400,"INVALID_LICENSE","Invalid license definition.")),
                "admin_update_license" => errors.push((400,"INVALID_STATUS","Invalid license status.")),
                "admin_create_codes" => errors.push((400,"INVALID_QUANTITY","INVALID_QUANTITY (1..100), INVALID_LIMIT (1..10000), or PRODUCT_MISMATCH.")),
                "unbind_device" => errors.push((404,"NOT_FOUND","Device was not found.")),
                "activate" => {
                    errors.push((400,"INVALID_ACTIVATION","Invalid activation request."));
                    errors.push((403,"INVALID_CODE","INVALID_CODE, PRODUCT_MISMATCH, LICENSE_REVOKED, LICENSE_NOT_YET_VALID, or LICENSE_EXPIRED."));
                    errors.push((409,"ACTIVATION_LIMIT","ACTIVATION_LIMIT, DEVICE_LIMIT, IDEMPOTENCY_MISMATCH, ACTIVATION_INACTIVE, or ACTIVATION_CONFLICT."));
                }
                _ => {}
            }
            if matches!(
                id.as_str(),
                "register" | "login" | "forgot_password" | "activate"
            ) {
                errors.push((429, "RATE_LIMITED", "Too many requests. Try again later."));
            }
            for (status, code, description) in errors {
                responses.insert(status.to_string(), json!({"description":description,"content":{"application/json":{"schema":{"$ref":"#/components/schemas/ApiErrorResponse"},"example":{"code":code,"message": match code { "INVALID_REGISTRATION" => "Name and a password of at least 10 characters are required.", "INVALID_QUANTITY" => "Quantity must be between 1 and 100.", "INVALID_TOKEN" => "Token is invalid or expired.", "INVALID_CODE" if id == "activate" => "Activation code is invalid or expired.", "ACTIVATION_LIMIT" => "Activation limit has been reached.", _ => description }}}}}));
            }
            let mut text_errors = Vec::new();
            if body {
                text_errors.extend([(400,"Malformed JSON."),(413,"Body exceeds 1 MiB."),(415,"Missing or unsupported JSON Content-Type."),(422,"JSON cannot deserialize into the request DTO (missing required field, wrong type, invalid UUID/date, or numeric overflow).")]);
            }
            if path.contains("{id}") {
                text_errors.push((400, "Invalid UUID path parameter."));
            }
            if websocket {
                text_errors.extend([(400,"Invalid WebSocket Connection/Upgrade headers, missing Sec-WebSocket-Key, or unsupported version (requires 13)."),(426,"Connection is not upgradable: WebSocket upgrade extension unavailable."),(405,"Invalid WebSocket handshake method; this route documents HTTP/1.1 GET upgrade, not HTTP/2 CONNECT.")]);
            }
            for (status, description) in text_errors {
                let response = responses
                    .entry(status.to_string())
                    .or_insert_with(|| json!({"description":description}));
                let previous = response["description"].as_str().unwrap().to_owned();
                response["description"] = json!(format!(
                    "{previous} Extractor rejection: {description} Plain text, not ApiErrorResponse."
                ));
                response["content"]["text/plain"] = json!({"schema":{"type":"string"}});
            }
        }
    }
    let paths = &mut doc["paths"];
    paths["/api/v1/auth/login"]["post"]["responses"]["200"]["headers"] = json!({
        "Set-Cookie":{"description":"ya_session=<opaque>; Path=/; HttpOnly; SameSite=Lax; Max-Age=43200; Secure when configured.","schema":{"type":"string"}},
        "X-CSRF-Token":{"description":"Same token as body csrfToken.","schema":{"type":"string"}}
    });
    paths["/api/v1/auth/logout"]["post"]["responses"]["204"]["headers"] = json!({"Set-Cookie":{"description":"Clears ya_session with Max-Age=0, Path=/, HttpOnly, SameSite=Lax and configured Secure.","schema":{"type":"string"}}});
    paths["/api/v1/activations"]["post"]["description"] = json!(
        "Public activation using snake_case. product_id is the product CODE, not a UUID. protocol_version must be 1; machine_id 1..1024 bytes; optional idempotency_key 1..200 bytes. Limits: 1000 attempts globally per 60 seconds and 10 per machine_id per 600 seconds (Redis fixed windows, including replays; no Retry-After header). Valid code/product and active, started, unexpired license required even on replay. Same code/key on a different machine conflicts; unbound keyed activation conflicts. Same key/machine or an existing active license/device binding returns the stored signed blob without consuming another slot, but always issues a fresh 24-hour opaque token (old tokens are not revoked). New bindings enforce both code activation_limit and license device_limit. Codes without licenses create an unassigned permanent license with device_limit equal to activation_limit and max_sessions=1."
    );
    paths["/api/v1/device/events"]["get"]["responses"]["101"]["headers"] = json!({
        "Connection":{"schema":{"type":"string"},"description":"upgrade"},
        "Upgrade":{"schema":{"type":"string"},"description":"websocket"},
        "Sec-WebSocket-Accept":{"schema":{"type":"string"},"description":"RFC 6455 handshake accept value"}
    });
    paths["/api/v1/device/events"]["get"]["parameters"] = json!([
        {"name":"Connection","in":"header","required":true,"schema":{"type":"string"},"example":"Upgrade"},
        {"name":"Upgrade","in":"header","required":true,"schema":{"type":"string"},"example":"websocket"},
        {"name":"Sec-WebSocket-Version","in":"header","required":true,"schema":{"type":"string"},"example":"13"},
        {"name":"Sec-WebSocket-Key","in":"header","required":true,"schema":{"type":"string"},"description":"Base64-encoded 16-byte nonce generated by the WebSocket client."}
    ]);
    paths["/api/v1/device/events"]["get"]["description"] = json!(
        "HTTP/1.1 GET WebSocket handshake only; not executable in Swagger UI. Native clients must send Authorization: Bearer <opaque access_token> and standard upgrade headers. Authentication runs after upgrade extraction, so malformed handshakes can reject before credentials are checked. JSON text frames include {\"type\":\"server.ping\",\"time\":\"2026-09-11T12:00:00Z\"}, {\"type\":\"license.updated\",\"license_id\":\"00000000-0000-4000-8000-000000000002\"}, {\"type\":\"license.revoked\",\"license_id\":\"00000000-0000-4000-8000-000000000002\"}, and {\"type\":\"device.unbound\",\"device_id\":\"00000000-0000-4000-8000-000000000003\"}. The reader also forwards features.changed outbox payloads, but these routes do not produce them or define their fields. Client application messages are ignored; there is no subscription, acknowledgement, or resume message. At most 100 outbox events are read per tick. Reconnecting starts from zero; delivery is not exactly-once. No specific WebSocket close code is guaranteed."
    );
    for (path, method, status, example) in [
        ("/health/live", "get", "200", json!({"status":"ok"})),
        (
            "/health/ready",
            "get",
            "200",
            json!({"status":"ok","postgres":true,"redis":true}),
        ),
        (
            "/health/ready",
            "get",
            "503",
            json!({"status":"unavailable","postgres":false,"redis":true}),
        ),
        (
            "/api/v1/auth/login",
            "post",
            "200",
            json!({"csrfToken":"opaque-example-not-a-real-token"}),
        ),
        (
            "/api/v1/auth/me",
            "get",
            "200",
            json!({"user":{"id":"00000000-0000-4000-8000-000000000001","email":"user@example.test","name":"Example User","role":"user"},"csrfToken":"new-opaque-example-token"}),
        ),
        (
            "/api/v1/licenses",
            "get",
            "200",
            json!([{"id":"00000000-0000-4000-8000-000000000002","key":"YALC...0123","productName":"Desktop Pro","status":"active","expiresAt":null,"deviceLimit":2,"deviceCount":1,"features":["export"],"userEmail":"user@example.test"}]),
        ),
        (
            "/api/v1/devices",
            "get",
            "200",
            json!([{"id":"00000000-0000-4000-8000-000000000003","name":"","fingerprint":"mach...1234","platform":null,"lastSeenAt":"2026-09-11T12:00:00Z","activatedAt":"2026-09-10T12:00:00Z","licenseId":"00000000-0000-4000-8000-000000000002"}]),
        ),
        (
            "/api/v1/admin/products",
            "post",
            "201",
            json!({"id":"00000000-0000-4000-8000-000000000004"}),
        ),
        (
            "/api/v1/admin/features",
            "post",
            "201",
            json!({"id":"00000000-0000-4000-8000-000000000005"}),
        ),
        (
            "/api/v1/admin/licenses",
            "post",
            "201",
            json!({"id":"00000000-0000-4000-8000-000000000002","key":"YALC-0123456789ABCDEF0123"}),
        ),
        (
            "/api/v1/admin/activation-codes",
            "post",
            "201",
            json!({"codes":["YA-EXAMPLEONLYNOTVALID00"]}),
        ),
        (
            "/api/v1/activations",
            "post",
            "200",
            json!({"license":"ZXhhbXBsZS1ub3QtYS1zaWduZWQtbGljZW5zZQ==","realtime_url":"wss://example.test/api/v1/device/events","access_token":"opaque-example-not-a-real-token","token_expire_time":1789214400}),
        ),
    ] {
        paths[path][method]["responses"][status]["content"]["application/json"]["example"] =
            example;
    }
    for path in [
        "users",
        "products",
        "features",
        "licenses",
        "activation-codes",
        "audit-logs",
    ] {
        paths[format!("/api/v1/admin/{path}")]["get"]["responses"]["200"]["content"]["application/json"]
            ["example"] = json!({"total":0,"items":[]});
    }
    for (path, method, example) in [
        (
            "/api/v1/auth/register",
            "post",
            json!({"name":"Example User","email":"user@example.test","password":"example-password-not-real"}),
        ),
        (
            "/api/v1/auth/login",
            "post",
            json!({"email":"user@example.test","password":"example-password-not-real"}),
        ),
        (
            "/api/v1/auth/verify-email",
            "post",
            json!({"token":"opaque-email-token-example"}),
        ),
        (
            "/api/v1/auth/forgot-password",
            "post",
            json!({"email":"user@example.test"}),
        ),
        (
            "/api/v1/auth/reset-password",
            "post",
            json!({"token":"opaque-reset-token-example","password":"new-example-password-not-real"}),
        ),
        (
            "/api/v1/admin/users/{id}",
            "patch",
            json!({"status":"suspended","role":null}),
        ),
        (
            "/api/v1/admin/products",
            "post",
            json!({"name":"Desktop Pro","code":"desktop-pro","description":null}),
        ),
        (
            "/api/v1/admin/features",
            "post",
            json!({"productId":"00000000-0000-4000-8000-000000000004","name":"Export","code":"export"}),
        ),
        (
            "/api/v1/admin/licenses",
            "post",
            json!({"productId":"00000000-0000-4000-8000-000000000004","userId":null,"licenseType":"permanent","expiresAt":null,"deviceLimit":2,"maxSessions":1,"featureIds":[]}),
        ),
        (
            "/api/v1/admin/licenses/{id}",
            "patch",
            json!({"status":"revoked"}),
        ),
        (
            "/api/v1/admin/activation-codes",
            "post",
            json!({"productId":"00000000-0000-4000-8000-000000000004","licenseId":null,"quantity":1,"activationLimit":2,"expiresAt":null}),
        ),
        (
            "/api/v1/activations",
            "post",
            json!({"protocol_version":1,"product_id":"desktop-pro","activation_code":"YA-EXAMPLEONLYNOTVALID00","machine_id":"machine-fingerprint-example","idempotency_key":"request-example-1"}),
        ),
    ] {
        paths[path][method]["requestBody"]["content"]["application/json"]["example"] = example;
    }
    for (canonical, alias, method, id) in [
        (
            "/api/v1/auth/forgot-password",
            "/api/v1/auth/forgot",
            "post",
            "forgot_password_alias",
        ),
        (
            "/api/v1/auth/reset-password",
            "/api/v1/auth/reset",
            "post",
            "reset_password_alias",
        ),
        (
            "/api/v1/admin/audit-logs",
            "/api/v1/admin/audit",
            "get",
            "admin_audit_alias",
        ),
    ] {
        let mut operation = paths[canonical][method].clone();
        operation["operationId"] = json!(id);
        paths[alias] = json!({method:operation});
    }
    serde_json::from_value(doc).expect("enriched OpenAPI is valid")
}

#[cfg(test)]
mod tests {
    use std::{collections::BTreeSet, sync::Arc};

    use axum::{
        body::{Body, to_bytes},
        http::Request,
    };
    use serde_json::Value;
    use tower::ServiceExt;

    use super::*;

    struct NoMail;
    #[async_trait::async_trait]
    impl crate::mail::Mailer for NoMail {
        async fn send_link(&self, _: &str, _: &str, _: &str) -> anyhow::Result<()> {
            panic!("OpenAPI tests must not send mail")
        }
    }

    const INVENTORY: &[(&str, &str, &str)] = &[
        ("/health/live", "get", "live"),
        ("/health/ready", "get", "ready"),
        ("/api/v1/auth/register", "post", "register"),
        ("/api/v1/auth/verify-email", "post", "verify_email"),
        ("/api/v1/auth/login", "post", "login"),
        ("/api/v1/auth/logout", "post", "logout"),
        ("/api/v1/auth/me", "get", "me"),
        ("/api/v1/auth/forgot", "post", "forgot_password_alias"),
        ("/api/v1/auth/forgot-password", "post", "forgot_password"),
        ("/api/v1/auth/reset", "post", "reset_password_alias"),
        ("/api/v1/auth/reset-password", "post", "reset_password"),
        ("/api/v1/licenses", "get", "user_licenses"),
        ("/api/v1/devices", "get", "user_devices"),
        ("/api/v1/devices/{id}", "delete", "unbind_device"),
        ("/api/v1/admin/users", "get", "admin_users"),
        ("/api/v1/admin/users/{id}", "patch", "admin_update_user"),
        ("/api/v1/admin/products", "get", "admin_products"),
        ("/api/v1/admin/products", "post", "admin_create_product"),
        ("/api/v1/admin/features", "get", "admin_features"),
        ("/api/v1/admin/features", "post", "admin_create_feature"),
        ("/api/v1/admin/licenses", "get", "admin_licenses"),
        ("/api/v1/admin/licenses", "post", "admin_create_license"),
        (
            "/api/v1/admin/licenses/{id}",
            "patch",
            "admin_update_license",
        ),
        ("/api/v1/admin/activation-codes", "get", "admin_codes"),
        (
            "/api/v1/admin/activation-codes",
            "post",
            "admin_create_codes",
        ),
        ("/api/v1/admin/audit", "get", "admin_audit_alias"),
        ("/api/v1/admin/audit-logs", "get", "admin_audit"),
        ("/api/v1/activations", "post", "activate"),
        ("/api/v1/device/events", "get", "device_events"),
    ];

    async fn served_doc() -> Value {
        let response = crate::app(crate::test_state(Arc::new(NoMail)))
            .oneshot(
                Request::builder()
                    .uri("/api-docs/openapi.json")
                    .body(Body::empty())
                    .unwrap(),
            )
            .await
            .unwrap();
        assert_eq!(response.status(), 200);
        assert_eq!(response.headers()["content-type"], "application/json");
        serde_json::from_slice(
            &to_bytes(response.into_body(), 2 * 1024 * 1024)
                .await
                .unwrap(),
        )
        .unwrap()
    }

    fn check_refs(root: &Value, value: &Value) {
        match value {
            Value::Object(object) => {
                if let Some(reference) = object.get("$ref") {
                    let reference = reference.as_str().unwrap();
                    assert!(
                        reference.starts_with("#/"),
                        "external reference: {reference}"
                    );
                    assert!(
                        root.pointer(&reference[1..]).is_some(),
                        "unresolved: {reference}"
                    );
                }
                for child in object.values() {
                    check_refs(root, child);
                }
            }
            Value::Array(array) => {
                for child in array {
                    check_refs(root, child);
                }
            }
            _ => {}
        }
    }

    #[tokio::test]
    async fn served_inventory_refs_security_and_aliases() {
        let doc = served_doc().await;
        let paths = doc["paths"].as_object().unwrap();
        assert_eq!(paths.len(), 25);
        let mut actual = BTreeSet::new();
        let mut ids = BTreeSet::new();
        for (path, item) in paths {
            for (method, op) in item.as_object().unwrap() {
                let id = op["operationId"].as_str().unwrap();
                assert!(ids.insert(id), "duplicate operationId: {id}");
                actual.insert((path.as_str(), method.as_str(), id));
                let expected_security = if path == "/api/v1/device/events" {
                    json!([{"deviceBearer":[]}])
                } else if path.starts_with("/api/v1/admin/")
                    || matches!(
                        path.as_str(),
                        "/api/v1/auth/me"
                            | "/api/v1/auth/logout"
                            | "/api/v1/licenses"
                            | "/api/v1/devices"
                            | "/api/v1/devices/{id}"
                    )
                {
                    if method == "get" {
                        json!([{"sessionCookie":[]}])
                    } else {
                        json!([{"sessionCookie":[],"csrfToken":[]}])
                    }
                } else {
                    json!([])
                };
                assert_eq!(op["security"], expected_security, "security for {id}");
                for parameter in op["parameters"].as_array().into_iter().flatten() {
                    assert_ne!(
                        parameter["in"], "query",
                        "invented query parameter for {id}"
                    );
                }
                if path.starts_with("/api/v1/admin/") {
                    assert!(op["description"].as_str().unwrap().contains("role admin"));
                }
                if op.get("requestBody").is_some() {
                    for status in ["400", "413", "415", "422"] {
                        assert_eq!(
                            op["responses"][status]["content"]["text/plain"]["schema"]["type"],
                            "string"
                        );
                    }
                    assert!(
                        op["requestBody"]["content"]["application/json"]
                            .get("example")
                            .is_some()
                    );
                }
                for (status, response) in op["responses"].as_object().unwrap() {
                    if status == "204" || status == "202" || status == "101" {
                        assert!(
                            response.get("content").is_none(),
                            "unexpected body for {id} {status}"
                        );
                    }
                }
            }
        }
        assert_eq!(actual.len(), 29);
        assert_eq!(actual, INVENTORY.iter().copied().collect());
        check_refs(&doc, &doc);
        assert_eq!(
            doc["servers"],
            json!([{"url":"/","description":"Same origin"}])
        );
        assert!(doc.get("security").is_none());
        let schemes = &doc["components"]["securitySchemes"];
        assert_eq!(schemes.as_object().unwrap().len(), 3);
        assert_eq!(schemes["sessionCookie"]["type"], "apiKey");
        assert_eq!(schemes["sessionCookie"]["in"], "cookie");
        assert_eq!(schemes["sessionCookie"]["name"], "ya_session");
        assert_eq!(schemes["csrfToken"]["type"], "apiKey");
        assert_eq!(schemes["csrfToken"]["in"], "header");
        assert_eq!(schemes["csrfToken"]["name"], "X-CSRF-Token");
        assert_eq!(schemes["deviceBearer"]["scheme"], "bearer");
        assert_eq!(schemes["deviceBearer"]["bearerFormat"], "opaque");
        for (canonical, alias, method) in [
            (
                "/api/v1/auth/forgot-password",
                "/api/v1/auth/forgot",
                "post",
            ),
            ("/api/v1/auth/reset-password", "/api/v1/auth/reset", "post"),
            ("/api/v1/admin/audit-logs", "/api/v1/admin/audit", "get"),
        ] {
            let mut canonical = paths[canonical][method].clone();
            let mut alias = paths[alias][method].clone();
            assert_ne!(canonical["operationId"], alias["operationId"]);
            canonical.as_object_mut().unwrap().remove("operationId");
            alias.as_object_mut().unwrap().remove("operationId");
            assert_eq!(canonical, alias);
        }
    }

    #[test]
    fn explicit_router_registration_guard() {
        // Scoped source guard for the current literal .route + get/post/patch/delete style.
        // Not a general Rust parser: routing refactors must update this guard. Excludes Swagger
        // infrastructure, implicit HEAD/OPTIONS, and layers; includes chained methods and handlers.
        let mut actual = BTreeSet::new();
        for (source, start, end) in [
            (include_str!("routes.rs"), "pub fn api()", "#[utoipa::path"),
            (include_str!("lib.rs"), "pub fn app(", "pub async fn state("),
        ] {
            let block = source
                .split_once(start)
                .unwrap()
                .1
                .split_once(end)
                .unwrap()
                .0;
            let compact: String = block.chars().filter(|c| !c.is_whitespace()).collect();
            for registration in compact.split(".route(").skip(1) {
                let (path, rest) = registration
                    .strip_prefix('"')
                    .expect("literal route required")
                    .split_once('"')
                    .unwrap();
                // Stop at the balanced close of .route so merge/layer calls cannot be counted.
                let mut depth = 1_i32;
                let end = rest
                    .char_indices()
                    .find_map(|(index, ch)| {
                        match ch {
                            '(' => depth += 1,
                            ')' => depth -= 1,
                            _ => {}
                        }
                        (depth == 0).then_some(index)
                    })
                    .unwrap();
                let rest = &rest[..end];
                let mut count = 0;
                for method in [
                    "get", "post", "put", "patch", "delete", "head", "options", "trace", "connect",
                ] {
                    for call in rest.split(&format!("{method}(")).skip(1) {
                        let handler = call
                            .split_once(')')
                            .unwrap()
                            .0
                            .strip_prefix("routes::")
                            .unwrap_or(call.split_once(')').unwrap().0);
                        assert!(actual.insert((
                            path.to_owned(),
                            method.to_owned(),
                            handler.to_owned()
                        )));
                        count += 1;
                    }
                }
                assert!(count > 0, "unrecognized registration for {path}");
            }
        }
        let expected = INVENTORY
            .iter()
            .map(|(path, method, id)| {
                (
                    path.to_string(),
                    method.to_string(),
                    id.strip_suffix("_alias").unwrap_or(id).to_string(),
                )
            })
            .collect();
        assert_eq!(
            actual, expected,
            "explicit runtime routes drifted from the documented inventory"
        );
    }

    #[tokio::test]
    async fn wire_shapes_headers_and_swagger_configuration() {
        let doc = served_doc().await;
        let schemas = &doc["components"]["schemas"];
        for (name, fields) in [
            ("ApiErrorResponse", vec!["code", "message"]),
            (
                "ActivationRequest",
                vec![
                    "protocol_version",
                    "product_id",
                    "activation_code",
                    "machine_id",
                    "idempotency_key",
                ],
            ),
            (
                "ActivationResponse",
                vec![
                    "license",
                    "realtime_url",
                    "access_token",
                    "token_expire_time",
                ],
            ),
            ("MeResponse", vec!["user", "csrfToken"]),
            (
                "LicenseSummary",
                vec![
                    "id",
                    "key",
                    "productName",
                    "status",
                    "expiresAt",
                    "deviceLimit",
                    "deviceCount",
                    "features",
                    "userEmail",
                ],
            ),
            (
                "DeviceSummary",
                vec![
                    "id",
                    "name",
                    "fingerprint",
                    "platform",
                    "lastSeenAt",
                    "activatedAt",
                    "licenseId",
                ],
            ),
            (
                "UserSummary",
                vec!["id", "email", "name", "role", "status", "createdAt"],
            ),
            (
                "ProductSummary",
                vec!["id", "code", "name", "description", "featureCount"],
            ),
            ("FeatureSummary", vec!["id", "productId", "code", "name"]),
            (
                "ActivationCodeSummary",
                vec![
                    "id",
                    "code",
                    "productName",
                    "status",
                    "createdAt",
                    "redeemedBy",
                ],
            ),
            (
                "AuditSummary",
                vec![
                    "id",
                    "actorEmail",
                    "action",
                    "target",
                    "ipAddress",
                    "createdAt",
                ],
            ),
            (
                "LicenseInput",
                vec![
                    "productId",
                    "userId",
                    "licenseType",
                    "expiresAt",
                    "deviceLimit",
                    "maxSessions",
                    "featureIds",
                ],
            ),
        ] {
            let actual: BTreeSet<_> = schemas[name]["properties"]
                .as_object()
                .unwrap()
                .keys()
                .map(String::as_str)
                .collect();
            assert_eq!(actual, fields.into_iter().collect(), "fields for {name}");
        }
        assert_eq!(
            schemas["ActivationRequest"]["properties"]["product_id"]["type"],
            "string"
        );
        assert!(
            schemas["ActivationRequest"]["properties"]["product_id"]
                .get("format")
                .is_none()
        );
        assert_eq!(
            schemas["FeatureInput"]["properties"]["productId"]["format"],
            "uuid"
        );
        assert_eq!(
            schemas["AuditSummary"]["properties"]["id"]["type"],
            "string"
        );
        for (name, field) in [
            ("LicenseSummary", "expiresAt"),
            ("LicenseSummary", "userEmail"),
            ("DeviceSummary", "platform"),
            ("ProductSummary", "description"),
            ("ActivationCodeSummary", "redeemedBy"),
            ("AuditSummary", "actorEmail"),
            ("AuditSummary", "ipAddress"),
        ] {
            let schema = &schemas[name];
            assert!(
                schema["required"]
                    .as_array()
                    .unwrap()
                    .contains(&json!(field)),
                "nullable response field must still be present: {name}.{field}"
            );
            assert!(
                schema["properties"][field]["type"]
                    .as_array()
                    .unwrap()
                    .contains(&json!("null"))
            );
        }
        for (path, item) in [("licenses", "LicenseSummary"), ("devices", "DeviceSummary")] {
            let schema = &doc["paths"][format!("/api/v1/{path}")]["get"]["responses"]["200"]["content"]
                ["application/json"]["schema"];
            assert_eq!(schema["type"], "array");
            assert_eq!(
                schema["items"]["$ref"],
                format!("#/components/schemas/{item}")
            );
        }
        for (path, page, item) in [
            ("users", "UserPage", "UserSummary"),
            ("products", "ProductPage", "ProductSummary"),
            ("features", "FeaturePage", "FeatureSummary"),
            ("licenses", "LicensePage", "LicenseSummary"),
            (
                "activation-codes",
                "ActivationCodePage",
                "ActivationCodeSummary",
            ),
            ("audit-logs", "AuditPage", "AuditSummary"),
        ] {
            assert_eq!(
                doc["paths"][format!("/api/v1/admin/{path}")]["get"]["responses"]["200"]["content"]
                    ["application/json"]["schema"]["$ref"],
                format!("#/components/schemas/{page}")
            );
            assert_eq!(schemas[page]["properties"]["items"]["type"], "array");
            assert_eq!(
                schemas[page]["properties"]["items"]["items"]["$ref"],
                format!("#/components/schemas/{item}")
            );
            assert_eq!(schemas[page]["properties"].as_object().unwrap().len(), 2);
            assert_eq!(schemas[page]["properties"]["total"]["type"], "integer");
        }
        let paths = &doc["paths"];
        let login = &paths["/api/v1/auth/login"]["post"]["responses"]["200"];
        assert!(login["headers"].get("Set-Cookie").is_some());
        assert!(login["headers"].get("X-CSRF-Token").is_some());
        assert!(
            paths["/api/v1/auth/me"]["get"]["responses"]["200"]
                .get("headers")
                .is_none()
        );
        assert!(
            paths["/api/v1/auth/me"]["get"]["responses"]["200"]["description"]
                .as_str()
                .unwrap()
                .contains("Rotates")
        );
        assert!(
            paths["/api/v1/auth/logout"]["post"]["responses"]["204"]["headers"]
                .get("Set-Cookie")
                .is_some()
        );
        assert!(
            paths["/api/v1/device/events"]["get"]["responses"]
                .get("200")
                .is_none()
        );
        assert!(
            paths["/api/v1/device/events"]["get"]["responses"]
                .get("101")
                .is_some()
        );
        let config = serde_json::to_value(swagger_config()).unwrap();
        assert_eq!(config["withCredentials"], true);
        assert_eq!(config["persistAuthorization"], false);
        assert_eq!(config["tryItOutEnabled"], false);
        assert_eq!(config["validatorUrl"], "");
        let app = crate::app(crate::test_state(Arc::new(NoMail)));
        let response = app
            .oneshot(
                Request::builder()
                    .uri("/docs/swagger-initializer.js")
                    .body(Body::empty())
                    .unwrap(),
            )
            .await
            .unwrap();
        assert_eq!(response.status(), 200);
        let initializer = String::from_utf8(
            to_bytes(response.into_body(), 1024 * 1024)
                .await
                .unwrap()
                .to_vec(),
        )
        .unwrap();
        for setting in [
            "\"withCredentials\": true",
            "\"persistAuthorization\": false",
            "\"tryItOutEnabled\": false",
            "\"validatorUrl\": \"\"",
            "/api-docs/openapi.json",
        ] {
            assert!(
                initializer.contains(setting),
                "missing Swagger setting: {setting}"
            );
        }
    }

    #[tokio::test]
    async fn real_app_rejections_match_documented_media_types() {
        let doc = served_doc().await;
        let app = crate::app(crate::test_state(Arc::new(NoMail)));
        for (path, method, body, content_type, status, media) in [
            (
                "/api/v1/auth/register",
                "post",
                "{",
                Some("application/json"),
                400,
                "text/plain",
            ),
            (
                "/api/v1/auth/register",
                "post",
                "{}",
                Some("application/json"),
                422,
                "text/plain",
            ),
            (
                "/api/v1/auth/register",
                "post",
                "{}",
                None,
                415,
                "text/plain",
            ),
            ("/api/v1/auth/me", "get", "", None, 401, "application/json"),
            (
                "/api/v1/devices/not-a-uuid",
                "delete",
                "",
                None,
                400,
                "text/plain",
            ),
            ("/api/v1/device/events", "get", "", None, 400, "text/plain"),
        ] {
            let mut request = Request::builder()
                .method(method.to_uppercase().as_str())
                .uri(path);
            if let Some(content_type) = content_type {
                request = request.header("content-type", content_type);
            }
            let response = app
                .clone()
                .oneshot(request.body(Body::from(body)).unwrap())
                .await
                .unwrap();
            assert_eq!(response.status(), status, "{method} {path}");
            assert!(
                response.headers()["content-type"]
                    .to_str()
                    .unwrap()
                    .starts_with(media)
            );
            let path = if path.ends_with("not-a-uuid") {
                "/api/v1/devices/{id}"
            } else {
                path
            };
            assert!(
                doc["paths"][path][method]["responses"][status.to_string()]["content"]
                    .get(media)
                    .is_some()
            );
            if media == "application/json" {
                let value: Value =
                    serde_json::from_slice(&to_bytes(response.into_body(), 1024).await.unwrap())
                        .unwrap();
                assert_eq!(
                    value,
                    json!({"code":"UNAUTHORIZED","message":"Authentication required."})
                );
            }
        }
        let response = app
            .oneshot(
                Request::builder()
                    .method("POST")
                    .uri("/api/v1/auth/register")
                    .header("content-type", "application/json")
                    .body(Body::from("x".repeat(1024 * 1024 + 1)))
                    .unwrap(),
            )
            .await
            .unwrap();
        assert_eq!(response.status(), 413);
        assert!(
            response.headers()["content-type"]
                .to_str()
                .unwrap()
                .starts_with("text/plain")
        );
    }
}
