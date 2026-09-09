use std::time::Duration;

use axum::{
    Json, Router,
    extract::{
        Path, State, WebSocketUpgrade,
        ws::{Message, WebSocket},
    },
    http::{
        HeaderMap, HeaderValue, StatusCode,
        header::{AUTHORIZATION, SET_COOKIE},
    },
    response::{IntoResponse, Response},
    routing::{delete, get, patch, post},
};
use base64::Engine;
use chrono::{DateTime, Duration as ChronoDuration, Utc};
use license_format::{LicenseDefinition, LicenseKind};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use sha2::{Digest, Sha256};
use sqlx::{PgPool, Postgres, Row, Transaction};
use subtle::ConstantTimeEq;
use utoipa::ToSchema;
use uuid::Uuid;

use crate::{AppState, error::ApiError, security};

type Result<T> = std::result::Result<T, ApiError>;

const REGISTER_LIMIT: (u64, Duration) = (3, Duration::from_secs(60 * 60));
const LOGIN_LIMIT: (u64, Duration) = (10, Duration::from_secs(15 * 60));
const FORGOT_PASSWORD_LIMIT: (u64, Duration) = (3, Duration::from_secs(60 * 60));
const ACTIVATION_MACHINE_LIMIT: (u64, Duration) = (10, Duration::from_secs(10 * 60));
const ACTIVATION_GLOBAL_LIMIT: (u64, Duration) = (1_000, Duration::from_secs(60));

pub fn api() -> Router<AppState> {
    Router::new()
        .route("/api/v1/auth/register", post(register))
        .route("/api/v1/auth/verify-email", post(verify_email))
        .route("/api/v1/auth/login", post(login))
        .route("/api/v1/auth/logout", post(logout))
        .route("/api/v1/auth/me", get(me))
        .route("/api/v1/auth/forgot", post(forgot_password))
        .route("/api/v1/auth/forgot-password", post(forgot_password))
        .route("/api/v1/auth/reset", post(reset_password))
        .route("/api/v1/auth/reset-password", post(reset_password))
        .route("/api/v1/licenses", get(user_licenses))
        .route("/api/v1/devices", get(user_devices))
        .route("/api/v1/devices/{id}", delete(unbind_device))
        .route("/api/v1/admin/users", get(admin_users))
        .route("/api/v1/admin/users/{id}", patch(admin_update_user))
        .route(
            "/api/v1/admin/products",
            get(admin_products).post(admin_create_product),
        )
        .route(
            "/api/v1/admin/features",
            get(admin_features).post(admin_create_feature),
        )
        .route(
            "/api/v1/admin/licenses",
            get(admin_licenses).post(admin_create_license),
        )
        .route("/api/v1/admin/licenses/{id}", patch(admin_update_license))
        .route(
            "/api/v1/admin/activation-codes",
            get(admin_codes).post(admin_create_codes),
        )
        .route("/api/v1/admin/audit", get(admin_audit))
        .route("/api/v1/admin/audit-logs", get(admin_audit))
        .route("/api/v1/activations", post(activate))
        .route("/api/v1/device/events", get(device_events))
}

#[utoipa::path(get, path = "/health/live", responses((status = 200, description = "Process is alive")))]
pub async fn live() -> Json<Value> {
    Json(json!({"status":"ok"}))
}

#[utoipa::path(get, path = "/health/ready", responses((status = 200), (status = 503)))]
pub async fn ready(State(state): State<AppState>) -> Response {
    let postgres = sqlx::query_scalar::<_, i32>("SELECT 1")
        .fetch_one(&state.pool)
        .await
        .is_ok();
    let redis = match state.redis.get_multiplexed_async_connection().await {
        Ok(mut connection) => redis::cmd("PING")
            .query_async::<String>(&mut connection)
            .await
            .is_ok(),
        Err(_) => false,
    };
    let status = if postgres && redis {
        StatusCode::OK
    } else {
        StatusCode::SERVICE_UNAVAILABLE
    };
    (status, Json(json!({"status": if status == StatusCode::OK {"ok"} else {"unavailable"}, "postgres":postgres, "redis":redis}))).into_response()
}

#[derive(Deserialize)]
struct RegisterRequest {
    name: String,
    email: String,
    password: String,
}

async fn register(
    State(state): State<AppState>,
    Json(input): Json<RegisterRequest>,
) -> Result<StatusCode> {
    let email = normalize_email(&input.email)?;
    if input.name.trim().is_empty()
        || input.name.len() > 200
        || input.password.len() < 10
        || input.password.len() > 1024
    {
        return Err(ApiError::bad(
            "INVALID_REGISTRATION",
            "Name and a password of at least 10 characters are required.",
        ));
    }
    enforce_rate_limit(
        &state,
        "register",
        &email,
        REGISTER_LIMIT.0,
        REGISTER_LIMIT.1,
    )
    .await?;
    let hash = password_hash(input.password).await?;
    let mut tx = state.pool.begin().await?;
    let inserted_user_id: Option<Uuid> = sqlx::query_scalar(
        "INSERT INTO users(email,name,password_hash) VALUES($1,$2,$3) ON CONFLICT DO NOTHING RETURNING id",
    )
    .bind(&email)
    .bind(input.name.trim())
    .bind(hash)
    .fetch_optional(&mut *tx)
    .await?;
    let user_id = if let Some(user_id) = inserted_user_id {
        audit(
            &mut tx,
            Some(user_id),
            "user.registered",
            "user",
            user_id.to_string(),
            json!({}),
        )
        .await?;
        user_id
    } else {
        let row = sqlx::query("SELECT id,status::text FROM users WHERE email=$1 FOR UPDATE")
            .bind(&email)
            .fetch_one(&mut *tx)
            .await?;
        if row.get::<String, _>("status") != "pending" {
            return Err(ApiError::conflict(
                "EMAIL_EXISTS",
                "An account already exists for this email.",
            ));
        }
        let user_id: Uuid = row.get("id");
        sqlx::query("UPDATE email_tokens SET used_at=now() WHERE user_id=$1 AND purpose='verify_email' AND used_at IS NULL")
            .bind(user_id)
            .execute(&mut *tx)
            .await?;
        user_id
    };
    let token = security::opaque_token().map_err(ApiError::Internal)?;
    sqlx::query("INSERT INTO email_tokens(user_id,purpose,token_hash,expires_at) VALUES($1,'verify_email',$2,now()+interval '24 hours')")
        .bind(user_id).bind(security::hash_token(&token)).execute(&mut *tx).await?;
    tx.commit().await?;
    let link = format!(
        "{}/verify-email?token={}",
        state.config.public_url,
        url::form_urlencoded::byte_serialize(token.as_bytes()).collect::<String>()
    );
    state
        .mailer
        .send_link(&email, "Verify your YoAuthorize account", &link)
        .await
        .map_err(ApiError::Internal)?;
    Ok(StatusCode::ACCEPTED)
}

#[derive(Deserialize)]
struct TokenRequest {
    token: String,
}

async fn verify_email(
    State(state): State<AppState>,
    Json(input): Json<TokenRequest>,
) -> Result<StatusCode> {
    let mut tx = state.pool.begin().await?;
    let user_id = consume_token(&mut tx, &input.token, "verify_email").await?;
    sqlx::query(
        "UPDATE users SET status='active',updated_at=now() WHERE id=$1 AND status='pending'",
    )
    .bind(user_id)
    .execute(&mut *tx)
    .await?;
    audit(
        &mut tx,
        Some(user_id),
        "user.email_verified",
        "user",
        user_id.to_string(),
        json!({}),
    )
    .await?;
    tx.commit().await?;
    Ok(StatusCode::NO_CONTENT)
}

#[derive(Deserialize)]
struct LoginRequest {
    email: String,
    password: String,
}

async fn login(State(state): State<AppState>, Json(input): Json<LoginRequest>) -> Result<Response> {
    let email = normalize_email(&input.email)?;
    enforce_rate_limit(&state, "login", &email, LOGIN_LIMIT.0, LOGIN_LIMIT.1).await?;
    let row = sqlx::query("SELECT id,password_hash,status::text FROM users WHERE email=$1")
        .bind(&email)
        .fetch_optional(&state.pool)
        .await?;
    let valid = if let Some(row) = &row {
        password_valid(row.get("password_hash"), input.password).await?
    } else {
        false
    };
    if !valid {
        return Err(ApiError::Public(
            StatusCode::UNAUTHORIZED,
            "INVALID_CREDENTIALS",
            "Email or password is incorrect.",
        ));
    }
    let row = row.expect("checked above");
    if row.get::<String, _>("status") != "active" {
        return Err(ApiError::forbidden());
    }
    let user_id: Uuid = row.get("id");
    let session = security::opaque_token().map_err(ApiError::Internal)?;
    let csrf = security::opaque_token().map_err(ApiError::Internal)?;
    sqlx::query("INSERT INTO web_sessions(user_id,token_hash,csrf_hash,expires_at) VALUES($1,$2,$3,now()+interval '12 hours')")
        .bind(user_id).bind(security::hash_token(&session)).bind(security::hash_token(&csrf)).execute(&state.pool).await?;
    let mut headers = HeaderMap::new();
    headers.insert(
        SET_COOKIE,
        session_cookie(&session, state.config.cookie_secure)?,
    );
    headers.insert(
        "x-csrf-token",
        HeaderValue::from_str(&csrf).map_err(|e| ApiError::Internal(e.into()))?,
    );
    Ok((headers, Json(json!({"csrfToken":csrf}))).into_response())
}

async fn logout(State(state): State<AppState>, headers: HeaderMap) -> Result<Response> {
    let principal = principal(&state.pool, &headers, true).await?;
    sqlx::query("DELETE FROM web_sessions WHERE id=$1")
        .bind(principal.session_id)
        .execute(&state.pool)
        .await?;
    let clear = format!(
        "ya_session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0{}",
        if state.config.cookie_secure {
            "; Secure"
        } else {
            ""
        }
    );
    Ok((
        [(SET_COOKIE, HeaderValue::from_str(&clear).unwrap())],
        StatusCode::NO_CONTENT,
    )
        .into_response())
}

async fn me(State(state): State<AppState>, headers: HeaderMap) -> Result<Json<Value>> {
    let principal = principal(&state.pool, &headers, false).await?;
    let csrf = security::opaque_token().map_err(ApiError::Internal)?;
    sqlx::query("UPDATE web_sessions SET csrf_hash=$1,last_seen_at=now() WHERE id=$2")
        .bind(security::hash_token(&csrf))
        .bind(principal.session_id)
        .execute(&state.pool)
        .await?;
    Ok(Json(
        json!({"user":{"id":principal.user_id,"email":principal.email,"name":principal.name,"role":principal.role},"csrfToken":csrf}),
    ))
}

#[derive(Deserialize)]
struct EmailRequest {
    email: String,
}

async fn forgot_password(
    State(state): State<AppState>,
    Json(input): Json<EmailRequest>,
) -> Result<StatusCode> {
    let normalized_email = normalize_email(&input.email)?;
    enforce_rate_limit(
        &state,
        "forgot-password",
        &normalized_email,
        FORGOT_PASSWORD_LIMIT.0,
        FORGOT_PASSWORD_LIMIT.1,
    )
    .await?;
    if let Some(row) = sqlx::query("SELECT id,email FROM users WHERE email=$1 AND status='active'")
        .bind(normalized_email)
        .fetch_optional(&state.pool)
        .await?
    {
        let id: Uuid = row.get("id");
        let email: String = row.get("email");
        let token = security::opaque_token().map_err(ApiError::Internal)?;
        sqlx::query("UPDATE email_tokens SET used_at=now() WHERE user_id=$1 AND purpose='reset_password' AND used_at IS NULL").bind(id).execute(&state.pool).await?;
        sqlx::query("INSERT INTO email_tokens(user_id,purpose,token_hash,expires_at) VALUES($1,'reset_password',$2,now()+interval '1 hour')").bind(id).bind(security::hash_token(&token)).execute(&state.pool).await?;
        let link = format!(
            "{}/reset-password?token={}",
            state.config.public_url,
            url::form_urlencoded::byte_serialize(token.as_bytes()).collect::<String>()
        );
        state
            .mailer
            .send_link(&email, "Reset your YoAuthorize password", &link)
            .await
            .map_err(ApiError::Internal)?;
    }
    Ok(StatusCode::ACCEPTED)
}

#[derive(Deserialize)]
struct ResetRequest {
    token: String,
    password: String,
}

async fn reset_password(
    State(state): State<AppState>,
    Json(input): Json<ResetRequest>,
) -> Result<StatusCode> {
    if input.password.len() < 10 || input.password.len() > 1024 {
        return Err(ApiError::bad(
            "WEAK_PASSWORD",
            "Password must contain at least 10 characters.",
        ));
    }
    let hash = password_hash(input.password).await?;
    let mut tx = state.pool.begin().await?;
    let user_id = consume_token(&mut tx, &input.token, "reset_password").await?;
    sqlx::query("UPDATE users SET password_hash=$1,updated_at=now() WHERE id=$2")
        .bind(hash)
        .bind(user_id)
        .execute(&mut *tx)
        .await?;
    sqlx::query("DELETE FROM web_sessions WHERE user_id=$1")
        .bind(user_id)
        .execute(&mut *tx)
        .await?;
    audit(
        &mut tx,
        Some(user_id),
        "user.password_reset",
        "user",
        user_id.to_string(),
        json!({}),
    )
    .await?;
    tx.commit().await?;
    Ok(StatusCode::NO_CONTENT)
}

async fn user_licenses(State(state): State<AppState>, headers: HeaderMap) -> Result<Json<Value>> {
    let p = principal(&state.pool, &headers, false).await?;
    Ok(Json(Value::Array(
        license_rows(&state.pool, "WHERE l.user_id=$1", Some(p.user_id)).await?,
    )))
}

async fn user_devices(State(state): State<AppState>, headers: HeaderMap) -> Result<Json<Value>> {
    let p = principal(&state.pool, &headers, false).await?;
    let rows = sqlx::query("SELECT d.id,d.name,d.machine_hint,d.platform,d.last_seen_at,d.created_at,a.license_id FROM devices d JOIN activations a ON a.device_id=d.id AND a.unbound_at IS NULL JOIN licenses l ON l.id=a.license_id WHERE l.user_id=$1 ORDER BY d.last_seen_at DESC").bind(p.user_id).fetch_all(&state.pool).await?;
    Ok(Json(Value::Array(rows.into_iter().map(|r| json!({"id":r.get::<Uuid,_>("id"),"name":r.get::<String,_>("name"),"fingerprint":r.get::<String,_>("machine_hint"),"platform":r.get::<Option<String>,_>("platform"),"lastSeenAt":r.get::<DateTime<Utc>,_>("last_seen_at"),"activatedAt":r.get::<DateTime<Utc>,_>("created_at"),"licenseId":r.get::<Uuid,_>("license_id")})).collect())))
}

async fn unbind_device(
    State(state): State<AppState>,
    headers: HeaderMap,
    Path(id): Path<Uuid>,
) -> Result<StatusCode> {
    let p = principal(&state.pool, &headers, true).await?;
    let mut tx = state.pool.begin().await?;
    let unbound = sqlx::query("UPDATE activations a SET unbound_at=now() FROM licenses l WHERE a.device_id=$1 AND a.license_id=l.id AND l.user_id=$2 AND a.unbound_at IS NULL RETURNING a.id,a.activation_code_id")
        .bind(id)
        .bind(p.user_id)
        .fetch_all(&mut *tx)
        .await?;
    if unbound.is_empty() {
        return Err(ApiError::Public(
            StatusCode::NOT_FOUND,
            "NOT_FOUND",
            "Device was not found.",
        ));
    }
    for row in unbound {
        let activation_id: Uuid = row.get("id");
        let activation_code_id: Uuid = row.get("activation_code_id");
        sqlx::query("UPDATE device_tokens SET revoked_at=now() WHERE activation_id=$1 AND revoked_at IS NULL")
            .bind(activation_id)
            .execute(&mut *tx)
            .await?;
        sqlx::query("UPDATE activation_codes SET activation_count=activation_count-1 WHERE id=$1")
            .bind(activation_code_id)
            .execute(&mut *tx)
            .await?;
        event(
            &mut tx,
            "device.unbound",
            activation_id,
            json!({"type":"device.unbound","device_id":id}),
        )
        .await?;
    }
    audit(
        &mut tx,
        Some(p.user_id),
        "device.unbound",
        "device",
        id.to_string(),
        json!({}),
    )
    .await?;
    tx.commit().await?;
    Ok(StatusCode::NO_CONTENT)
}

async fn admin_users(State(state): State<AppState>, headers: HeaderMap) -> Result<Json<Value>> {
    admin(&state.pool, &headers, false).await?;
    let rows = sqlx::query("SELECT id,email,name,role::text,status::text,created_at FROM users ORDER BY created_at DESC LIMIT 500").fetch_all(&state.pool).await?;
    Ok(Json(page(rows.into_iter().map(|r| json!({"id":r.get::<Uuid,_>("id"),"email":r.get::<String,_>("email"),"name":r.get::<String,_>("name"),"role":r.get::<String,_>("role"),"status":status_frontend(&r.get::<String,_>("status")),"createdAt":r.get::<DateTime<Utc>,_>("created_at")})).collect())))
}

#[derive(Deserialize)]
struct UpdateUser {
    status: Option<String>,
    role: Option<String>,
}

async fn admin_update_user(
    State(state): State<AppState>,
    headers: HeaderMap,
    Path(id): Path<Uuid>,
    Json(input): Json<UpdateUser>,
) -> Result<StatusCode> {
    let actor = admin(&state.pool, &headers, true).await?;
    let status = input
        .status
        .as_deref()
        .map(|s| if s == "suspended" { "disabled" } else { s });
    if status.is_some_and(|s| !matches!(s, "pending" | "active" | "disabled"))
        || input
            .role
            .as_deref()
            .is_some_and(|s| !matches!(s, "user" | "admin"))
    {
        return Err(ApiError::bad(
            "INVALID_USER_UPDATE",
            "Invalid role or status.",
        ));
    }
    let mut tx = state.pool.begin().await?;
    sqlx::query("UPDATE users SET status=COALESCE($1::user_status,status),role=COALESCE($2::user_role,role),updated_at=now() WHERE id=$3").bind(status).bind(input.role).bind(id).execute(&mut *tx).await?;
    if status == Some("disabled") {
        sqlx::query("DELETE FROM web_sessions WHERE user_id=$1")
            .bind(id)
            .execute(&mut *tx)
            .await?;
    }
    audit(
        &mut tx,
        Some(actor.user_id),
        "admin.user_updated",
        "user",
        id.to_string(),
        json!({"status":status}),
    )
    .await?;
    tx.commit().await?;
    Ok(StatusCode::NO_CONTENT)
}

async fn admin_products(State(state): State<AppState>, headers: HeaderMap) -> Result<Json<Value>> {
    admin(&state.pool, &headers, false).await?;
    let rows = sqlx::query("SELECT p.id,p.code,p.name,p.description,count(f.id) feature_count FROM products p LEFT JOIN features f ON f.product_id=p.id GROUP BY p.id ORDER BY p.name").fetch_all(&state.pool).await?;
    Ok(Json(page(rows.into_iter().map(|r| json!({"id":r.get::<Uuid,_>("id"),"code":r.get::<String,_>("code"),"name":r.get::<String,_>("name"),"description":r.get::<Option<String>,_>("description"),"featureCount":r.get::<i64,_>("feature_count")})).collect())))
}

#[derive(Deserialize)]
struct ProductInput {
    name: String,
    code: String,
    description: Option<String>,
}
async fn admin_create_product(
    State(state): State<AppState>,
    headers: HeaderMap,
    Json(i): Json<ProductInput>,
) -> Result<(StatusCode, Json<Value>)> {
    let actor = admin(&state.pool, &headers, true).await?;
    valid_code(&i.code)?;
    let id: Uuid = sqlx::query_scalar(
        "INSERT INTO products(name,code,description) VALUES($1,$2,$3) RETURNING id",
    )
    .bind(i.name)
    .bind(i.code)
    .bind(i.description)
    .fetch_one(&state.pool)
    .await?;
    record_audit(
        &state.pool,
        actor.user_id,
        "admin.product_created",
        "product",
        id,
    )
    .await?;
    Ok((StatusCode::CREATED, Json(json!({"id":id}))))
}

async fn admin_features(State(state): State<AppState>, headers: HeaderMap) -> Result<Json<Value>> {
    admin(&state.pool, &headers, false).await?;
    let rows = sqlx::query("SELECT id,product_id,code,name FROM features ORDER BY name")
        .fetch_all(&state.pool)
        .await?;
    Ok(Json(page(rows.into_iter().map(|r| json!({"id":r.get::<Uuid,_>("id"),"productId":r.get::<Uuid,_>("product_id"),"code":r.get::<String,_>("code"),"name":r.get::<String,_>("name")})).collect())))
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct FeatureInput {
    product_id: Uuid,
    name: String,
    code: String,
}
async fn admin_create_feature(
    State(state): State<AppState>,
    headers: HeaderMap,
    Json(i): Json<FeatureInput>,
) -> Result<(StatusCode, Json<Value>)> {
    let actor = admin(&state.pool, &headers, true).await?;
    valid_code(&i.code)?;
    let id: Uuid = sqlx::query_scalar(
        "INSERT INTO features(product_id,name,code) VALUES($1,$2,$3) RETURNING id",
    )
    .bind(i.product_id)
    .bind(i.name)
    .bind(i.code)
    .fetch_one(&state.pool)
    .await?;
    record_audit(
        &state.pool,
        actor.user_id,
        "admin.feature_created",
        "feature",
        id,
    )
    .await?;
    Ok((StatusCode::CREATED, Json(json!({"id":id}))))
}

async fn admin_licenses(State(state): State<AppState>, headers: HeaderMap) -> Result<Json<Value>> {
    admin(&state.pool, &headers, false).await?;
    Ok(Json(page(license_rows(&state.pool, "", None).await?)))
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct LicenseInput {
    product_id: Uuid,
    user_id: Option<Uuid>,
    license_type: String,
    expires_at: Option<DateTime<Utc>>,
    device_limit: i32,
    max_sessions: Option<i32>,
    feature_ids: Vec<Uuid>,
}
async fn admin_create_license(
    State(state): State<AppState>,
    headers: HeaderMap,
    Json(i): Json<LicenseInput>,
) -> Result<(StatusCode, Json<Value>)> {
    let actor = admin(&state.pool, &headers, true).await?;
    if !matches!(
        i.license_type.as_str(),
        "trial" | "subscription" | "permanent"
    ) || i.device_limit < 1
        || i.device_limit > 10000
        || !valid_license_expiration(&i.license_type, i.expires_at)
    {
        return Err(ApiError::bad(
            "INVALID_LICENSE",
            "Invalid license definition.",
        ));
    }
    let mut tx = state.pool.begin().await?;
    let id = Uuid::new_v4();
    let key = format!(
        "YALC-{}",
        id.simple().to_string()[..20].to_ascii_uppercase()
    );
    sqlx::query("INSERT INTO licenses(id,license_key,product_id,user_id,license_type,expires_at,device_limit,max_sessions) VALUES($1,$2,$3,$4,$5,$6,$7,$8)").bind(id).bind(&key).bind(i.product_id).bind(i.user_id).bind(i.license_type).bind(i.expires_at).bind(i.device_limit).bind(i.max_sessions.unwrap_or(1)).execute(&mut *tx).await?;
    for feature in i.feature_ids {
        sqlx::query("INSERT INTO license_features(license_id,feature_id) SELECT $1,id FROM features WHERE id=$2 AND product_id=$3").bind(id).bind(feature).bind(i.product_id).execute(&mut *tx).await?;
    }
    audit(
        &mut tx,
        Some(actor.user_id),
        "admin.license_created",
        "license",
        id.to_string(),
        json!({}),
    )
    .await?;
    tx.commit().await?;
    Ok((StatusCode::CREATED, Json(json!({"id":id,"key":key}))))
}

#[derive(Deserialize)]
struct LicenseUpdate {
    status: String,
}
async fn admin_update_license(
    State(state): State<AppState>,
    headers: HeaderMap,
    Path(id): Path<Uuid>,
    Json(i): Json<LicenseUpdate>,
) -> Result<StatusCode> {
    let actor = admin(&state.pool, &headers, true).await?;
    if !matches!(i.status.as_str(), "active" | "revoked") {
        return Err(ApiError::bad("INVALID_STATUS", "Invalid license status."));
    }
    let mut tx = state.pool.begin().await?;
    sqlx::query("UPDATE licenses SET status=$1::license_status,updated_at=now() WHERE id=$2")
        .bind(&i.status)
        .bind(id)
        .execute(&mut *tx)
        .await?;
    let event_type = if i.status == "revoked" {
        "license.revoked"
    } else {
        "license.updated"
    };
    sqlx::query("INSERT INTO outbox_events(topic,aggregate_id,payload) SELECT $1,a.id::text,jsonb_build_object('type',$1,'license_id',$2) FROM activations a WHERE a.license_id=$2 AND a.unbound_at IS NULL").bind(event_type).bind(id).execute(&mut *tx).await?;
    audit(
        &mut tx,
        Some(actor.user_id),
        "admin.license_updated",
        "license",
        id.to_string(),
        json!({"status":i.status}),
    )
    .await?;
    tx.commit().await?;
    Ok(StatusCode::NO_CONTENT)
}

async fn admin_codes(State(state): State<AppState>, headers: HeaderMap) -> Result<Json<Value>> {
    admin(&state.pool, &headers, false).await?;
    let rows=sqlx::query("SELECT ac.id,ac.code_hint,p.name product_name,ac.activation_count,ac.activation_limit,ac.disabled_at,ac.expires_at,ac.created_at,u.email FROM activation_codes ac JOIN products p ON p.id=ac.product_id LEFT JOIN licenses l ON l.id=ac.license_id LEFT JOIN users u ON u.id=l.user_id ORDER BY ac.created_at DESC LIMIT 500").fetch_all(&state.pool).await?;
    Ok(Json(page(rows.into_iter().map(|r| { let disabled:Option<DateTime<Utc>>=r.get("disabled_at"); let expires:Option<DateTime<Utc>>=r.get("expires_at"); let count:i32=r.get("activation_count"); let limit:i32=r.get("activation_limit"); json!({"id":r.get::<Uuid,_>("id"),"code":r.get::<String,_>("code_hint"),"productName":r.get::<String,_>("product_name"),"status":if disabled.is_some()||expires.is_some_and(|x|x<Utc::now()){ "disabled" }else if count>=limit{"redeemed"}else{"active"},"createdAt":r.get::<DateTime<Utc>,_>("created_at"),"redeemedBy":r.get::<Option<String>,_>("email")}) }).collect())))
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct CodeInput {
    product_id: Uuid,
    license_id: Option<Uuid>,
    quantity: Option<u32>,
    activation_limit: Option<i32>,
    expires_at: Option<DateTime<Utc>>,
}
async fn admin_create_codes(
    State(state): State<AppState>,
    headers: HeaderMap,
    Json(i): Json<CodeInput>,
) -> Result<(StatusCode, Json<Value>)> {
    let actor = admin(&state.pool, &headers, true).await?;
    let quantity = i.quantity.unwrap_or(1);
    if quantity == 0 || quantity > 100 {
        return Err(ApiError::bad(
            "INVALID_QUANTITY",
            "Quantity must be between 1 and 100.",
        ));
    }
    let limit = i.activation_limit.unwrap_or(1);
    if !(1..=10000).contains(&limit) {
        return Err(ApiError::bad("INVALID_LIMIT", "Invalid activation limit."));
    }
    let mut tx = state.pool.begin().await?;
    if let Some(license_id) = i.license_id {
        let matches: bool = sqlx::query_scalar(
            "SELECT EXISTS(SELECT 1 FROM licenses WHERE id=$1 AND product_id=$2)",
        )
        .bind(license_id)
        .bind(i.product_id)
        .fetch_one(&mut *tx)
        .await?;
        if !matches {
            return Err(ApiError::bad(
                "PRODUCT_MISMATCH",
                "Activation code product must match its license.",
            ));
        }
    }
    let mut codes = Vec::new();
    for _ in 0..quantity {
        let raw = format!(
            "YA-{}",
            security::opaque_token().map_err(ApiError::Internal)?[..20].to_ascii_uppercase()
        );
        let hint = format!("{}...{}", &raw[..7], &raw[raw.len() - 4..]);
        sqlx::query("INSERT INTO activation_codes(product_id,license_id,code_hash,code_hint,activation_limit,expires_at) VALUES($1,$2,$3,$4,$5,$6)").bind(i.product_id).bind(i.license_id).bind(security::hash_activation(&state.config.activation_pepper,&raw)).bind(&hint).bind(limit).bind(i.expires_at).execute(&mut *tx).await?;
        codes.push(raw);
    }
    audit(
        &mut tx,
        Some(actor.user_id),
        "admin.activation_codes_created",
        "product",
        i.product_id.to_string(),
        json!({"quantity":quantity}),
    )
    .await?;
    tx.commit().await?;
    Ok((StatusCode::CREATED, Json(json!({"codes":codes}))))
}

async fn admin_audit(State(state): State<AppState>, headers: HeaderMap) -> Result<Json<Value>> {
    admin(&state.pool, &headers, false).await?;
    let rows=sqlx::query("SELECT a.id,a.action,a.target_type,a.target_id,a.ip_address::text,a.created_at,u.email FROM audit_logs a LEFT JOIN users u ON u.id=a.actor_user_id ORDER BY a.id DESC LIMIT 1000").fetch_all(&state.pool).await?;
    Ok(Json(page(rows.into_iter().map(|r|json!({"id":r.get::<i64,_>("id").to_string(),"actorEmail":r.get::<Option<String>,_>("email"),"action":r.get::<String,_>("action"),"target":format!("{}:{}",r.get::<Option<String>,_>("target_type").unwrap_or_default(),r.get::<Option<String>,_>("target_id").unwrap_or_default()),"ipAddress":r.get::<Option<String>,_>("ip_address"),"createdAt":r.get::<DateTime<Utc>,_>("created_at")})).collect())))
}

#[derive(Debug, Deserialize, ToSchema)]
pub struct ActivationRequest {
    pub protocol_version: u16,
    pub product_id: String,
    pub activation_code: String,
    pub machine_id: String,
    pub idempotency_key: Option<String>,
}
#[derive(Debug, Serialize, ToSchema)]
pub struct ActivationResponse {
    pub license: String,
    pub realtime_url: String,
    pub access_token: String,
    pub token_expire_time: i64,
}

#[utoipa::path(post,path="/api/v1/activations",request_body=ActivationRequest,responses((status=200,body=ActivationResponse),(status=400),(status=403),(status=409),(status=429)))]
pub async fn activate(
    State(state): State<AppState>,
    Json(i): Json<ActivationRequest>,
) -> Result<Json<ActivationResponse>> {
    if i.protocol_version != 1
        || i.machine_id.is_empty()
        || i.machine_id.len() > 1024
        || i.idempotency_key
            .as_ref()
            .is_some_and(|x| x.is_empty() || x.len() > 200)
    {
        return Err(ApiError::bad(
            "INVALID_ACTIVATION",
            "Invalid activation request.",
        ));
    }
    enforce_rate_limit(
        &state,
        "activation-global",
        "all",
        ACTIVATION_GLOBAL_LIMIT.0,
        ACTIVATION_GLOBAL_LIMIT.1,
    )
    .await?;
    enforce_rate_limit(
        &state,
        "activation-machine",
        &i.machine_id,
        ACTIVATION_MACHINE_LIMIT.0,
        ACTIVATION_MACHINE_LIMIT.1,
    )
    .await?;
    let code_hash = security::hash_activation(&state.config.activation_pepper, &i.activation_code);
    let machine_hash = Sha256::digest(i.machine_id.as_bytes()).to_vec();
    let mut tx = state.pool.begin().await?;
    let code=sqlx::query("SELECT ac.id,ac.product_id,ac.license_id,ac.activation_limit,ac.activation_count,p.code product_code FROM activation_codes ac JOIN products p ON p.id=ac.product_id WHERE ac.code_hash=$1 AND ac.disabled_at IS NULL AND (ac.expires_at IS NULL OR ac.expires_at>now()) FOR UPDATE OF ac").bind(code_hash).fetch_optional(&mut *tx).await?.ok_or(ApiError::Public(StatusCode::FORBIDDEN,"INVALID_CODE","Activation code is invalid or expired."))?;
    if code.get::<String, _>("product_code") != i.product_id {
        return Err(ApiError::Public(
            StatusCode::FORBIDDEN,
            "PRODUCT_MISMATCH",
            "Activation code is not valid for this product.",
        ));
    }
    let code_id: Uuid = code.get("id");
    let product_id: Uuid = code.get("product_id");
    let license_id: Uuid = if let Some(id) = code.get::<Option<Uuid>, _>("license_id") {
        id
    } else {
        let id = Uuid::new_v4();
        let key = format!(
            "YALC-{}",
            id.simple().to_string()[..20].to_ascii_uppercase()
        );
        sqlx::query("INSERT INTO licenses(id,license_key,product_id,license_type,device_limit,max_sessions) VALUES($1,$2,$3,'permanent',$4,1)").bind(id).bind(key).bind(product_id).bind(code.get::<i32,_>("activation_limit")).execute(&mut *tx).await?;
        sqlx::query("UPDATE activation_codes SET license_id=$1 WHERE id=$2")
            .bind(id)
            .bind(code_id)
            .execute(&mut *tx)
            .await?;
        id
    };
    let l=sqlx::query("SELECT l.product_id,l.user_id,l.license_type,l.not_before,l.expires_at,l.device_limit,l.max_sessions,l.status::text,l.not_before<=now() started,(l.expires_at IS NULL OR l.expires_at>now()) unexpired FROM licenses l WHERE l.id=$1 FOR UPDATE").bind(license_id).fetch_one(&mut *tx).await?;
    if l.get::<Uuid, _>("product_id") != product_id {
        return Err(ApiError::Public(
            StatusCode::FORBIDDEN,
            "PRODUCT_MISMATCH",
            "Activation code is not valid for this license.",
        ));
    }
    validate_license_state(&l)?;
    if let Some(key) = &i.idempotency_key
        && let Some(r)=sqlx::query("SELECT a.id,a.license_blob,a.unbound_at,d.machine_hash FROM activations a JOIN devices d ON d.id=a.device_id WHERE a.activation_code_id=$1 AND a.idempotency_key=$2 FOR UPDATE OF a").bind(code_id).bind(key).fetch_optional(&mut *tx).await?
    {
        if r.get::<Vec<u8>,_>("machine_hash").ct_eq(&machine_hash).unwrap_u8()!=1 { return Err(ApiError::conflict("IDEMPOTENCY_MISMATCH","Idempotency key was already used for another request.")); }
        if r.get::<Option<DateTime<Utc>>, _>("unbound_at").is_some() {
            return Err(ApiError::conflict("ACTIVATION_INACTIVE", "Activation has been unbound."));
        }
        return finish_activation(&state,tx,r.get("license_blob"),r.get("id")).await;
    }
    let device_id:Uuid=sqlx::query_scalar("INSERT INTO devices(machine_hash,machine_hint) VALUES($1,$2) ON CONFLICT(machine_hash) DO UPDATE SET last_seen_at=now() RETURNING id").bind(machine_hash).bind(mask(&i.machine_id)).fetch_one(&mut *tx).await?;
    if let Some(r)=sqlx::query("SELECT id,license_blob FROM activations WHERE license_id=$1 AND device_id=$2 AND unbound_at IS NULL FOR UPDATE").bind(license_id).bind(device_id).fetch_optional(&mut *tx).await?{return finish_activation(&state,tx,r.get("license_blob"),r.get("id")).await;}
    if code.get::<i32, _>("activation_count") >= code.get::<i32, _>("activation_limit") {
        return Err(ApiError::conflict(
            "ACTIVATION_LIMIT",
            "Activation limit has been reached.",
        ));
    }
    let active_devices: i64 = sqlx::query_scalar(
        "SELECT count(*) FROM activations WHERE license_id=$1 AND unbound_at IS NULL",
    )
    .bind(license_id)
    .fetch_one(&mut *tx)
    .await?;
    if active_devices >= i64::from(l.get::<i32, _>("device_limit")) {
        return Err(ApiError::conflict(
            "DEVICE_LIMIT",
            "License device limit has been reached.",
        ));
    }
    let features=sqlx::query_scalar::<_,String>("SELECT f.code FROM license_features lf JOIN features f ON f.id=lf.feature_id WHERE lf.license_id=$1 ORDER BY f.code").bind(license_id).fetch_all(&mut *tx).await?;
    let signing_key=sqlx::query("SELECT key_id,public_key FROM signing_keys WHERE status='active' AND (not_after IS NULL OR not_after>now()) ORDER BY created_at DESC LIMIT 1").fetch_optional(&mut *tx).await?.ok_or_else(||ApiError::Internal(anyhow::anyhow!("no active signing key metadata")))?;
    let key_id: String = signing_key.get("key_id");
    let now = Utc::now();
    let kind = match l.get::<String, _>("license_type").as_str() {
        "trial" => LicenseKind::Trial,
        "subscription" => LicenseKind::Subscription,
        _ => LicenseKind::Permanent,
    };
    let definition = LicenseDefinition {
        license_id: license_id.to_string(),
        product_id: i.product_id,
        customer_id: l
            .get::<Option<Uuid>, _>("user_id")
            .map_or_else(|| "unassigned".into(), |x| x.to_string()),
        issue_time: now.timestamp() as u64,
        not_before: l.get::<DateTime<Utc>, _>("not_before").timestamp() as u64,
        expire_time: l
            .get::<Option<DateTime<Utc>>, _>("expires_at")
            .map_or(0, |x| x.timestamp() as u64),
        license_type: kind,
        features,
        max_sessions: l.get::<i32, _>("max_sessions") as u32,
        machine_id: Some(i.machine_id),
    };
    let signed = state
        .signer
        .sign(&definition)
        .await
        .map_err(ApiError::Internal)?;
    if signed.key_id != key_id {
        return Err(ApiError::Internal(anyhow::anyhow!(
            "signer key id does not match active metadata"
        )));
    }
    let blob = base64::engine::general_purpose::STANDARD
        .decode(&signed.license)
        .map_err(|e| ApiError::Internal(e.into()))?;
    let public_key: [u8; 32] = signing_key
        .get::<Vec<u8>, _>("public_key")
        .try_into()
        .map_err(|_| ApiError::Internal(anyhow::anyhow!("invalid signing public key metadata")))?;
    let public_key = ed25519_dalek::VerifyingKey::from_bytes(&public_key)
        .map_err(|e| ApiError::Internal(e.into()))?;
    license_format::verify(&blob, &key_id, &public_key)
        .map_err(|e| ApiError::Internal(e.into()))?;
    let activation_id: Uuid = sqlx::query_scalar("INSERT INTO activations(license_id,device_id,activation_code_id,idempotency_key,license_blob) VALUES($1,$2,$3,$4,$5) RETURNING id").bind(license_id).bind(device_id).bind(code_id).bind(i.idempotency_key).bind(&blob).fetch_one(&mut *tx).await.map_err(|e|if is_unique(&e){ApiError::conflict("ACTIVATION_CONFLICT","Activation request conflicts with an existing request.")}else{e.into()})?;
    sqlx::query("UPDATE activation_codes SET activation_count=activation_count+1 WHERE id=$1")
        .bind(code_id)
        .execute(&mut *tx)
        .await?;
    finish_activation(&state, tx, blob, activation_id).await
}

async fn finish_activation(
    state: &AppState,
    mut tx: Transaction<'_, Postgres>,
    blob: Vec<u8>,
    activation_id: Uuid,
) -> Result<Json<ActivationResponse>> {
    let license = sqlx::query("SELECT l.status::text,l.not_before<=clock_timestamp() started,(l.expires_at IS NULL OR l.expires_at>clock_timestamp()) unexpired,a.unbound_at FROM activations a JOIN licenses l ON l.id=a.license_id WHERE a.id=$1 FOR UPDATE OF a")
        .bind(activation_id)
        .fetch_one(&mut *tx)
        .await?;
    if license
        .get::<Option<DateTime<Utc>>, _>("unbound_at")
        .is_some()
    {
        return Err(ApiError::conflict(
            "ACTIVATION_INACTIVE",
            "Activation has been unbound.",
        ));
    }
    validate_license_state(&license)?;
    let token = security::opaque_token().map_err(ApiError::Internal)?;
    let expires = Utc::now() + ChronoDuration::hours(24);
    sqlx::query("INSERT INTO device_tokens(activation_id,token_hash,expires_at) VALUES($1,$2,$3)")
        .bind(activation_id)
        .bind(security::hash_token(&token))
        .bind(expires)
        .execute(&mut *tx)
        .await?;
    tx.commit().await?;
    Ok(Json(ActivationResponse {
        license: base64::engine::general_purpose::STANDARD.encode(blob),
        realtime_url: format!(
            "{}/api/v1/device/events",
            state
                .config
                .public_url
                .replacen("https://", "wss://", 1)
                .replacen("http://", "ws://", 1)
        ),
        access_token: token,
        token_expire_time: expires.timestamp(),
    }))
}

async fn device_events(
    State(state): State<AppState>,
    headers: HeaderMap,
    ws: WebSocketUpgrade,
) -> Result<Response> {
    let bearer = headers
        .get(AUTHORIZATION)
        .and_then(|v| v.to_str().ok())
        .and_then(|v| v.strip_prefix("Bearer "))
        .ok_or_else(ApiError::unauthorized)?;
    let token=sqlx::query("SELECT dt.id,dt.activation_id FROM device_tokens dt JOIN activations a ON a.id=dt.activation_id JOIN licenses l ON l.id=a.license_id WHERE dt.token_hash=$1 AND dt.revoked_at IS NULL AND dt.expires_at>now() AND a.unbound_at IS NULL AND l.status='active' AND l.not_before<=now() AND (l.expires_at IS NULL OR l.expires_at>now())").bind(security::hash_token(bearer)).fetch_optional(&state.pool).await?.ok_or_else(ApiError::unauthorized)?;
    let token_id: Uuid = token.get("id");
    let activation_id: Uuid = token.get("activation_id");
    Ok(ws
        .on_upgrade(move |socket| websocket(socket, state.pool, token_id, activation_id))
        .into_response())
}
async fn websocket(mut socket: WebSocket, pool: PgPool, token_id: Uuid, activation_id: Uuid) {
    let mut last_id = 0_i64;
    let mut interval = tokio::time::interval(Duration::from_secs(15));
    loop {
        tokio::select! {
            message = socket.recv() => match message {
                Some(Ok(Message::Close(_))) | None | Some(Err(_)) => break,
                _ => {}
            },
            _ = interval.tick() => {
                let authorized = sqlx::query_scalar::<_, bool>("SELECT EXISTS(SELECT 1 FROM device_tokens dt JOIN activations a ON a.id=dt.activation_id JOIN licenses l ON l.id=a.license_id WHERE dt.id=$1 AND dt.activation_id=$2 AND dt.revoked_at IS NULL AND dt.expires_at>now() AND a.unbound_at IS NULL AND l.status='active' AND l.not_before<=now() AND (l.expires_at IS NULL OR l.expires_at>now()))")
                    .bind(token_id).bind(activation_id).fetch_one(&pool).await;
                if !matches!(authorized, Ok(true)) {
                    if let Ok(rows) = sqlx::query("SELECT payload FROM outbox_events WHERE aggregate_id=$1 AND id>$2 AND topic IN ('license.revoked','device.unbound') ORDER BY id LIMIT 100")
                        .bind(activation_id.to_string()).bind(last_id).fetch_all(&pool).await
                    {
                        for row in rows {
                            let payload: Value = row.get("payload");
                            if socket.send(Message::Text(payload.to_string().into())).await.is_err() { return; }
                        }
                    }
                    break;
                }
                let rows = sqlx::query("SELECT id,payload FROM outbox_events WHERE aggregate_id=$1 AND id>$2 AND topic IN ('license.updated','license.revoked','device.unbound','features.changed') ORDER BY id LIMIT 100")
                    .bind(activation_id.to_string()).bind(last_id).fetch_all(&pool).await;
                if let Ok(rows) = rows {
                    for row in rows {
                        last_id = row.get("id");
                        let payload: Value = row.get("payload");
                        if socket.send(Message::Text(payload.to_string().into())).await.is_err() { return; }
                    }
                }
                if socket.send(Message::Text(json!({"type":"server.ping","time":Utc::now()}).to_string().into())).await.is_err() { break; }
            }
        }
    }
}

struct Principal {
    user_id: Uuid,
    session_id: Uuid,
    email: String,
    name: String,
    role: String,
}
async fn principal(pool: &PgPool, headers: &HeaderMap, csrf: bool) -> Result<Principal> {
    let token = security::cookie_value(headers, "ya_session").ok_or_else(ApiError::unauthorized)?;
    let row=sqlx::query("SELECT s.id session_id,s.csrf_hash,u.id user_id,u.email,u.name,u.role::text FROM web_sessions s JOIN users u ON u.id=s.user_id WHERE s.token_hash=$1 AND s.expires_at>now() AND u.status='active'").bind(security::hash_token(&token)).fetch_optional(pool).await?.ok_or_else(ApiError::unauthorized)?;
    if csrf {
        let supplied = headers
            .get("x-csrf-token")
            .and_then(|x| x.to_str().ok())
            .ok_or_else(ApiError::forbidden)?;
        let expected: Vec<u8> = row.get("csrf_hash");
        if security::hash_token(supplied).ct_eq(&expected).unwrap_u8() != 1 {
            return Err(ApiError::forbidden());
        }
    }
    Ok(Principal {
        user_id: row.get("user_id"),
        session_id: row.get("session_id"),
        email: row.get("email"),
        name: row.get("name"),
        role: row.get("role"),
    })
}
async fn admin(pool: &PgPool, headers: &HeaderMap, csrf: bool) -> Result<Principal> {
    let p = principal(pool, headers, csrf).await?;
    if p.role != "admin" {
        return Err(ApiError::forbidden());
    }
    Ok(p)
}
async fn consume_token(
    tx: &mut Transaction<'_, Postgres>,
    token: &str,
    purpose: &str,
) -> Result<Uuid> {
    sqlx::query_scalar("UPDATE email_tokens SET used_at=now() WHERE token_hash=$1 AND purpose=$2 AND used_at IS NULL AND expires_at>now() RETURNING user_id").bind(security::hash_token(token)).bind(purpose).fetch_optional(&mut **tx).await?.ok_or(ApiError::Public(StatusCode::BAD_REQUEST,"INVALID_TOKEN","Token is invalid or expired."))
}
async fn audit(
    tx: &mut Transaction<'_, Postgres>,
    actor: Option<Uuid>,
    action: &str,
    target_type: &str,
    target_id: String,
    metadata: Value,
) -> Result<()> {
    sqlx::query("INSERT INTO audit_logs(actor_user_id,action,target_type,target_id,metadata) VALUES($1,$2,$3,$4,$5)").bind(actor).bind(action).bind(target_type).bind(target_id).bind(metadata).execute(&mut **tx).await?;
    Ok(())
}
async fn event(
    tx: &mut Transaction<'_, Postgres>,
    topic: &str,
    id: Uuid,
    payload: Value,
) -> Result<()> {
    sqlx::query("INSERT INTO outbox_events(topic,aggregate_id,payload) VALUES($1,$2,$3)")
        .bind(topic)
        .bind(id.to_string())
        .bind(payload)
        .execute(&mut **tx)
        .await?;
    Ok(())
}
async fn record_audit(
    pool: &PgPool,
    actor: Uuid,
    action: &str,
    target_type: &str,
    id: Uuid,
) -> Result<()> {
    sqlx::query(
        "INSERT INTO audit_logs(actor_user_id,action,target_type,target_id) VALUES($1,$2,$3,$4)",
    )
    .bind(actor)
    .bind(action)
    .bind(target_type)
    .bind(id.to_string())
    .execute(pool)
    .await?;
    Ok(())
}
async fn license_rows(pool: &PgPool, clause: &str, user: Option<Uuid>) -> Result<Vec<Value>> {
    let sql = format!(
        "SELECT l.id,l.license_key,p.name product_name,l.status::text,l.expires_at,l.device_limit,u.email,(SELECT count(*) FROM activations a WHERE a.license_id=l.id AND a.unbound_at IS NULL) device_count,COALESCE((SELECT jsonb_agg(f.code ORDER BY f.code) FROM license_features lf JOIN features f ON f.id=lf.feature_id WHERE lf.license_id=l.id),'[]') features FROM licenses l JOIN products p ON p.id=l.product_id LEFT JOIN users u ON u.id=l.user_id {clause} ORDER BY l.created_at DESC LIMIT 500"
    );
    let mut query = sqlx::query(&sql);
    if let Some(id) = user {
        query = query.bind(id);
    }
    let rows = query.fetch_all(pool).await?;
    Ok(rows.into_iter().map(|r|{let status:String=r.get("status");let expires:Option<DateTime<Utc>>=r.get("expires_at");json!({"id":r.get::<Uuid,_>("id"),"key":mask(&r.get::<String,_>("license_key")),"productName":r.get::<String,_>("product_name"),"status":if status=="revoked"{"revoked"}else if expires.is_some_and(|x|x<Utc::now()){ "expired" }else{"active"},"expiresAt":expires,"deviceLimit":r.get::<i32,_>("device_limit"),"deviceCount":r.get::<i64,_>("device_count"),"features":r.get::<Value,_>("features"),"userEmail":r.get::<Option<String>,_>("email")})}).collect())
}
fn page(items: Vec<Value>) -> Value {
    json!({"total":items.len(),"items":items})
}
async fn enforce_rate_limit(
    state: &AppState,
    scope: &str,
    subject: &str,
    limit: u64,
    window: Duration,
) -> Result<()> {
    const SCRIPT: &str = r#"
local count = redis.call('INCR', KEYS[1])
if count == 1 or redis.call('TTL', KEYS[1]) < 0 then
  redis.call('EXPIRE', KEYS[1], ARGV[1])
end
return count
"#;

    let key = rate_limit_key(scope, subject);
    let mut connection = state
        .redis
        .get_multiplexed_async_connection()
        .await
        .map_err(|error| ApiError::Internal(error.into()))?;
    let count = redis::cmd("EVAL")
        .arg(SCRIPT)
        .arg(1)
        .arg(key)
        .arg(window.as_secs())
        .query_async::<u64>(&mut connection)
        .await
        .map_err(|error| ApiError::Internal(error.into()))?;
    if count > limit {
        Err(ApiError::rate_limited())
    } else {
        Ok(())
    }
}
fn rate_limit_key(scope: &str, subject: &str) -> String {
    format!(
        "ya:rate:{scope}:{}",
        hex::encode(Sha256::digest(subject.as_bytes()))
    )
}
fn normalize_email(email: &str) -> Result<String> {
    let value = email.trim().to_ascii_lowercase();
    if value.len() > 320
        || !value
            .split_once('@')
            .is_some_and(|(a, b)| !a.is_empty() && b.contains('.'))
    {
        Err(ApiError::bad(
            "INVALID_EMAIL",
            "Enter a valid email address.",
        ))
    } else {
        Ok(value)
    }
}
fn valid_code(code: &str) -> Result<()> {
    if code.is_empty()
        || code.len() > 100
        || !code
            .bytes()
            .all(|x| x.is_ascii_alphanumeric() || b"_.-".contains(&x))
    {
        Err(ApiError::bad(
            "INVALID_CODE",
            "Code contains invalid characters.",
        ))
    } else {
        Ok(())
    }
}
fn valid_license_expiration(license_type: &str, expires_at: Option<DateTime<Utc>>) -> bool {
    if license_type == "permanent" {
        expires_at.is_none()
    } else {
        expires_at.is_some_and(|expires| expires > Utc::now())
    }
}
fn validate_license_state(license: &sqlx::postgres::PgRow) -> Result<()> {
    if license.get::<String, _>("status") != "active" {
        return Err(ApiError::Public(
            StatusCode::FORBIDDEN,
            "LICENSE_REVOKED",
            "License is not active.",
        ));
    }
    if !license.get::<bool, _>("started") {
        return Err(ApiError::Public(
            StatusCode::FORBIDDEN,
            "LICENSE_NOT_YET_VALID",
            "License is not yet valid.",
        ));
    }
    if !license.get::<bool, _>("unexpired") {
        return Err(ApiError::Public(
            StatusCode::FORBIDDEN,
            "LICENSE_EXPIRED",
            "License has expired.",
        ));
    }
    Ok(())
}
fn mask(value: &str) -> String {
    if value.len() <= 12 {
        "********".into()
    } else {
        format!("{}...{}", &value[..4], &value[value.len() - 4..])
    }
}
fn status_frontend(status: &str) -> &str {
    if status == "disabled" {
        "suspended"
    } else {
        status
    }
}
fn is_unique(error: &sqlx::Error) -> bool {
    matches!(error,sqlx::Error::Database(e) if e.code().as_deref()==Some("23505"))
}
fn session_cookie(token: &str, secure: bool) -> Result<HeaderValue> {
    HeaderValue::from_str(&format!(
        "ya_session={token}; Path=/; HttpOnly; SameSite=Lax; Max-Age=43200{}",
        if secure { "; Secure" } else { "" }
    ))
    .map_err(|e| ApiError::Internal(e.into()))
}

async fn password_hash(password: String) -> Result<String> {
    tokio::task::spawn_blocking(move || security::hash_password(&password))
        .await
        .map_err(|error| ApiError::Internal(error.into()))?
        .map_err(ApiError::Internal)
}

async fn password_valid(encoded: String, password: String) -> Result<bool> {
    tokio::task::spawn_blocking(move || security::verify_password(&encoded, &password))
        .await
        .map_err(|error| ApiError::Internal(error.into()))
}

#[cfg(test)]
mod tests {
    use super::*;
    use async_trait::async_trait;
    use axum::body::Body;
    use std::sync::Arc;
    use tower::ServiceExt;
    struct NoMail;
    #[async_trait]
    impl crate::mail::Mailer for NoMail {
        async fn send_link(&self, _: &str, _: &str, _: &str) -> anyhow::Result<()> {
            Ok(())
        }
    }
    #[tokio::test]
    async fn live_does_not_need_dependencies() {
        let app = crate::app(crate::test_state(Arc::new(NoMail)));
        let response = app
            .oneshot(
                axum::http::Request::builder()
                    .uri("/health/live")
                    .body(Body::empty())
                    .unwrap(),
            )
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::OK);
    }
    #[test]
    fn email_and_codes_are_validated() {
        assert_eq!(normalize_email(" A@EXAMPLE.COM ").unwrap(), "a@example.com");
        assert!(normalize_email("invalid").is_err());
        assert!(valid_code("capture.v1").is_ok());
        assert!(valid_code("not valid").is_err());
    }
    #[test]
    fn rate_limit_keys_are_deterministic_and_hide_subjects() {
        let key = rate_limit_key("login", "person@example.com");
        assert_eq!(key, rate_limit_key("login", "person@example.com"));
        assert_ne!(key, rate_limit_key("register", "person@example.com"));
        assert!(!key.contains("person@example.com"));
    }
    #[test]
    fn license_expiration_matches_license_type() {
        assert!(valid_license_expiration("permanent", None));
        assert!(!valid_license_expiration("permanent", Some(Utc::now())));
        assert!(!valid_license_expiration("trial", None));
        assert!(valid_license_expiration(
            "subscription",
            Some(Utc::now() + ChronoDuration::hours(1))
        ));
    }
}
