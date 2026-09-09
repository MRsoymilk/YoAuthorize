#![forbid(unsafe_code)]

pub mod config;
mod error;
mod mail;
mod routes;
pub mod security;
mod signer;

use std::sync::Arc;

use axum::{Router, routing::get};
use base64::Engine;
use config::Config;
use mail::{Mailer, SmtpMailer};
use redis::Client as RedisClient;
use signer::SignerClient;
use sqlx::{PgPool, Row, postgres::PgPoolOptions};
use subtle::ConstantTimeEq;
use tower_http::{limit::RequestBodyLimitLayer, trace::TraceLayer};
use utoipa::OpenApi;
use utoipa_swagger_ui::SwaggerUi;

#[derive(Clone)]
pub struct AppState {
    pub pool: PgPool,
    pub redis: RedisClient,
    pub config: Arc<Config>,
    pub mailer: Arc<dyn Mailer>,
    pub signer: SignerClient,
}

#[derive(OpenApi)]
#[openapi(
    paths(routes::live, routes::ready, routes::activate),
    components(schemas(routes::ActivationRequest, routes::ActivationResponse)),
    tags((name = "YoAuthorize", description = "Remote licensing API"))
)]
struct ApiDoc;

pub fn app(state: AppState) -> Router {
    Router::new()
        .route("/health/live", get(routes::live))
        .route("/health/ready", get(routes::ready))
        .merge(routes::api())
        .merge(SwaggerUi::new("/docs").url("/api-docs/openapi.json", ApiDoc::openapi()))
        .layer(RequestBodyLimitLayer::new(1024 * 1024))
        .layer(TraceLayer::new_for_http())
        .with_state(state)
}

pub async fn state(config: Config) -> anyhow::Result<AppState> {
    let pool = PgPoolOptions::new()
        .max_connections(20)
        .connect(&config.database_url)
        .await?;
    let redis = RedisClient::open(config.redis_url.clone())?;
    let mailer = Arc::new(SmtpMailer::new(&config.smtp_url, &config.mail_from)?);
    let signer = SignerClient::new(config.signer_url.clone(), config.signer_secret.clone());
    let signing_key = signer.key_info().await?;
    let public_key = base64::engine::general_purpose::STANDARD.decode(&signing_key.public_key)?;
    if public_key.len() != 32 {
        anyhow::bail!("signer returned an invalid Ed25519 public key");
    }
    let mut tx = pool.begin().await?;
    sqlx::query("LOCK TABLE signing_keys IN SHARE ROW EXCLUSIVE MODE")
        .execute(&mut *tx)
        .await?;
    let stored = sqlx::query(
        "SELECT key_id,public_key FROM signing_keys WHERE status='active' AND (not_after IS NULL OR not_after>now()) ORDER BY created_at DESC LIMIT 1",
    )
    .fetch_optional(&mut *tx)
    .await?;
    if let Some(stored) = stored {
        if stored.get::<String, _>("key_id") != signing_key.key_id
            || stored
                .get::<Vec<u8>, _>("public_key")
                .ct_eq(&public_key)
                .unwrap_u8()
                != 1
        {
            anyhow::bail!("signer key does not match immutable active signing key metadata");
        }
    } else {
        sqlx::query("INSERT INTO signing_keys(key_id,public_key,status) VALUES($1,$2,'active')")
            .bind(&signing_key.key_id)
            .bind(&public_key)
            .execute(&mut *tx)
            .await?;
    }
    tx.commit().await?;
    Ok(AppState {
        pool,
        redis,
        config: Arc::new(config),
        mailer,
        signer,
    })
}

#[cfg(test)]
pub(crate) fn test_state(mailer: Arc<dyn Mailer>) -> AppState {
    let config = Config {
        listen: "127.0.0.1:0".parse().unwrap(),
        database_url: "postgres://localhost/test".into(),
        redis_url: "redis://127.0.0.1/".into(),
        public_url: "https://example.test".into(),
        cookie_secure: true,
        activation_pepper: vec![1; 32],
        signer_url: "http://127.0.0.1:1".into(),
        signer_secret: "x".repeat(32),
        smtp_url: "smtp://localhost".into(),
        mail_from: "test@example.test".into(),
    };
    AppState {
        pool: PgPoolOptions::new()
            .connect_lazy(&config.database_url)
            .unwrap(),
        redis: RedisClient::open(config.redis_url.clone()).unwrap(),
        signer: SignerClient::new(config.signer_url.clone(), config.signer_secret.clone()),
        config: Arc::new(config),
        mailer,
    }
}
