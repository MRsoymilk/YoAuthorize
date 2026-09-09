use axum::{Json, http::StatusCode, response::IntoResponse};
use serde_json::json;

#[derive(Debug, thiserror::Error)]
pub enum ApiError {
    #[error("{1}")]
    Public(StatusCode, &'static str, &'static str),
    #[error(transparent)]
    Internal(#[from] anyhow::Error),
}

impl ApiError {
    pub fn bad(code: &'static str, message: &'static str) -> Self {
        Self::Public(StatusCode::BAD_REQUEST, code, message)
    }
    pub fn unauthorized() -> Self {
        Self::Public(
            StatusCode::UNAUTHORIZED,
            "UNAUTHORIZED",
            "Authentication required.",
        )
    }
    pub fn forbidden() -> Self {
        Self::Public(
            StatusCode::FORBIDDEN,
            "FORBIDDEN",
            "You do not have permission to do that.",
        )
    }
    pub fn conflict(code: &'static str, message: &'static str) -> Self {
        Self::Public(StatusCode::CONFLICT, code, message)
    }
    pub fn rate_limited() -> Self {
        Self::Public(
            StatusCode::TOO_MANY_REQUESTS,
            "RATE_LIMITED",
            "Too many requests. Try again later.",
        )
    }
}

impl IntoResponse for ApiError {
    fn into_response(self) -> axum::response::Response {
        match self {
            Self::Public(status, code, message) => {
                (status, Json(json!({"code": code, "message": message}))).into_response()
            }
            Self::Internal(error) => {
                tracing::error!(error = %error, "request failed");
                (StatusCode::INTERNAL_SERVER_ERROR, Json(json!({"code":"INTERNAL_ERROR", "message":"The service is temporarily unavailable."}))).into_response()
            }
        }
    }
}

impl From<sqlx::Error> for ApiError {
    fn from(value: sqlx::Error) -> Self {
        Self::Internal(value.into())
    }
}

impl From<reqwest::Error> for ApiError {
    fn from(value: reqwest::Error) -> Self {
        Self::Internal(value.into())
    }
}
