#![forbid(unsafe_code)]

use std::{
    env, fs,
    io::{Read, Write},
    net::{SocketAddr, TcpStream},
    path::Path,
    time::Duration,
};

use axum::{
    Json, Router,
    extract::State,
    http::{HeaderMap, StatusCode, header::AUTHORIZATION},
    response::{IntoResponse, Response},
    routing::{get, post},
};
use base64::Engine;
use ed25519_dalek::SigningKey;
use license_format::LicenseDefinition;
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use subtle::ConstantTimeEq;
use tower_http::{limit::RequestBodyLimitLayer, trace::TraceLayer};

#[derive(Clone)]
struct AppState {
    key_id: String,
    key: SigningKey,
    secret_hash: [u8; 32],
}

#[derive(Deserialize)]
struct SignRequest {
    definition: LicenseDefinition,
}
#[derive(Serialize)]
struct SignResponse {
    license: String,
    key_id: String,
}
#[derive(Serialize)]
struct KeyInfo {
    key_id: String,
    public_key: String,
}

fn router(state: AppState) -> Router {
    Router::new()
        .route(
            "/health/live",
            get(|| async { Json(serde_json::json!({"status":"ok"})) }),
        )
        .route("/internal/v1/licenses/sign", post(sign))
        .route("/internal/v1/signing-key", get(signing_key))
        .layer(RequestBodyLimitLayer::new(128 * 1024))
        .layer(TraceLayer::new_for_http())
        .with_state(state)
}

fn authorized(headers: &HeaderMap, state: &AppState) -> bool {
    headers
        .get(AUTHORIZATION)
        .and_then(|value| value.to_str().ok())
        .and_then(|value| value.strip_prefix("Bearer "))
        .is_some_and(|secret| {
            Sha256::digest(secret.as_bytes())
                .as_slice()
                .ct_eq(&state.secret_hash)
                .unwrap_u8()
                == 1
        })
}

async fn signing_key(State(state): State<AppState>, headers: HeaderMap) -> Response {
    if !authorized(&headers, &state) {
        return (
            StatusCode::UNAUTHORIZED,
            Json(serde_json::json!({"code":"UNAUTHORIZED"})),
        )
            .into_response();
    }
    Json(KeyInfo {
        key_id: state.key_id,
        public_key: base64::engine::general_purpose::STANDARD
            .encode(state.key.verifying_key().to_bytes()),
    })
    .into_response()
}

async fn sign(
    State(state): State<AppState>,
    headers: HeaderMap,
    Json(input): Json<SignRequest>,
) -> Response {
    if !authorized(&headers, &state) {
        return (
            StatusCode::UNAUTHORIZED,
            Json(serde_json::json!({"code":"UNAUTHORIZED"})),
        )
            .into_response();
    }
    match license_format::generate(&input.definition, &state.key_id, &state.key) {
        Ok(bytes) => Json(SignResponse {
            license: base64::engine::general_purpose::STANDARD.encode(bytes),
            key_id: state.key_id,
        })
        .into_response(),
        Err(error) => (
            StatusCode::BAD_REQUEST,
            Json(serde_json::json!({"code":"INVALID_DEFINITION","message":error.to_string()})),
        )
            .into_response(),
    }
}

fn load_key(path: &Path) -> anyhow::Result<SigningKey> {
    #[cfg(unix)]
    {
        use std::os::unix::fs::MetadataExt;
        if fs::metadata(path)?.mode() & 0o077 != 0 {
            anyhow::bail!("signing key file must not be accessible by group or others");
        }
    }
    let contents = fs::read(path)?;
    let decoded = if contents.len() == 32 {
        contents
    } else {
        base64::engine::general_purpose::STANDARD.decode(String::from_utf8(contents)?.trim())?
    };
    let bytes: [u8; 32] = decoded
        .try_into()
        .map_err(|_| anyhow::anyhow!("signing key must contain exactly 32 bytes"))?;
    Ok(SigningKey::from_bytes(&bytes))
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    if env::args().nth(1).as_deref() == Some("--healthcheck") {
        let listen = env::var("SIGNER_LISTEN_ADDR").unwrap_or_else(|_| "0.0.0.0:8090".into());
        let port = listen
            .rsplit_once(':')
            .map(|(_, port)| port)
            .unwrap_or("8090");
        let mut stream = TcpStream::connect(format!("127.0.0.1:{port}"))?;
        stream.set_read_timeout(Some(Duration::from_secs(2)))?;
        stream.write_all(b"GET /health/live HTTP/1.0\r\nHost: localhost\r\n\r\n")?;
        let mut response = [0; 64];
        let read = stream.read(&mut response)?;
        anyhow::ensure!(
            response[..read].starts_with(b"HTTP/1.0 200")
                || response[..read].starts_with(b"HTTP/1.1 200"),
            "signer is not healthy"
        );
        return Ok(());
    }
    tracing_subscriber::fmt()
        .json()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| "license_signer=info,tower_http=info".into()),
        )
        .init();
    let listen: SocketAddr = env::var("SIGNER_LISTEN_ADDR")
        .unwrap_or_else(|_| "0.0.0.0:8090".into())
        .parse()?;
    let key_id = env::var("SIGNING_KEY_ID")?;
    if key_id.is_empty() || key_id.len() > 255 {
        anyhow::bail!("SIGNING_KEY_ID must contain 1-255 bytes");
    }
    let secret = if let Ok(path) = env::var("SIGNER_SHARED_SECRET_FILE") {
        fs::read_to_string(path)?.trim().to_owned()
    } else {
        env::var("SIGNER_SHARED_SECRET")?
    };
    if secret.len() < 32 {
        anyhow::bail!("SIGNER_SHARED_SECRET must contain at least 32 bytes");
    }
    let state = AppState {
        key_id,
        key: load_key(Path::new(&env::var("SIGNING_KEY_FILE")?))?,
        secret_hash: Sha256::digest(secret).into(),
    };
    let listener = tokio::net::TcpListener::bind(listen).await?;
    tracing::info!(%listen, "license signer listening");
    axum::serve(listener, router(state))
        .with_graceful_shutdown(async {
            let _ = tokio::signal::ctrl_c().await;
        })
        .await?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use axum::{body::Body, http::Request};
    use tower::ServiceExt;

    fn state() -> AppState {
        AppState {
            key_id: "test-key".into(),
            key: SigningKey::from_bytes(&[7; 32]),
            secret_hash: Sha256::digest(b"01234567890123456789012345678901").into(),
        }
    }

    #[tokio::test]
    async fn arbitrary_or_unauthenticated_input_is_rejected() {
        let response = router(state())
            .oneshot(
                Request::post("/internal/v1/licenses/sign")
                    .header("content-type", "application/octet-stream")
                    .body(Body::from(vec![0; 64]))
                    .unwrap(),
            )
            .await
            .unwrap();
        assert!(matches!(
            response.status(),
            StatusCode::UNAUTHORIZED | StatusCode::UNSUPPORTED_MEDIA_TYPE
        ));
    }
}
