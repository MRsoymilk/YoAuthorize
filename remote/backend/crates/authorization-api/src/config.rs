use std::{env, fs, net::SocketAddr, str::FromStr};

use anyhow::{Context, Result, bail};

#[derive(Clone, Debug)]
pub struct Config {
    pub listen: SocketAddr,
    pub database_url: String,
    pub redis_url: String,
    pub public_url: String,
    pub cookie_secure: bool,
    pub activation_pepper: Vec<u8>,
    pub signer_url: String,
    pub signer_secret: String,
    pub smtp_url: String,
    pub mail_from: String,
}

impl Config {
    pub fn from_env() -> Result<Self> {
        let required = |key: &str| required_value(key);
        let pepper = required("ACTIVATION_PEPPER")?.into_bytes();
        let signer_secret = required("SIGNER_SHARED_SECRET")?;
        if pepper.len() < 32 || signer_secret.len() < 32 {
            bail!("ACTIVATION_PEPPER and SIGNER_SHARED_SECRET must contain at least 32 bytes");
        }
        let public_url = required("PUBLIC_URL")?.trim_end_matches('/').to_owned();
        let cookie_secure =
            bool::from_str(&env::var("COOKIE_SECURE").unwrap_or_else(|_| "true".into()))?;
        validate_cookie_security(&public_url, cookie_secure)?;
        Ok(Self {
            listen: env::var("LISTEN_ADDR")
                .unwrap_or_else(|_| "0.0.0.0:8080".into())
                .parse()?,
            database_url: required("DATABASE_URL")?,
            redis_url: required("REDIS_URL")?,
            public_url,
            cookie_secure,
            activation_pepper: pepper,
            signer_url: required("SIGNER_URL")?.trim_end_matches('/').to_owned(),
            signer_secret,
            smtp_url: required("SMTP_URL")?,
            mail_from: required("MAIL_FROM")?,
        })
    }
}

fn validate_cookie_security(public_url: &str, cookie_secure: bool) -> Result<()> {
    if url::Url::parse(public_url).is_ok_and(|url| url.scheme() == "https") && !cookie_secure {
        bail!("COOKIE_SECURE must be true when PUBLIC_URL uses HTTPS");
    }
    Ok(())
}

pub fn required_value(key: &str) -> Result<String> {
    if let Ok(path) = env::var(format!("{key}_FILE")) {
        return Ok(fs::read_to_string(&path)
            .with_context(|| format!("failed to read {key}_FILE at {path}"))?
            .trim()
            .to_owned());
    }
    env::var(key).with_context(|| format!("{key} or {key}_FILE is required"))
}

#[cfg(test)]
mod tests {
    use super::validate_cookie_security;

    #[test]
    fn https_requires_secure_cookies() {
        assert!(validate_cookie_security("https://licenses.example.com", false).is_err());
        assert!(validate_cookie_security("https://licenses.example.com", true).is_ok());
        assert!(validate_cookie_security("http://localhost:8088", false).is_ok());
    }
}
