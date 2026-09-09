use argon2::{Argon2, PasswordHash, PasswordHasher, PasswordVerifier, password_hash::SaltString};
use hmac::{Hmac, Mac};
use rand::TryRngCore;
use sha2::{Digest, Sha256};

pub fn opaque_token() -> anyhow::Result<String> {
    let mut bytes = [0_u8; 32];
    rand::rngs::OsRng.try_fill_bytes(&mut bytes)?;
    Ok(base64::Engine::encode(
        &base64::engine::general_purpose::URL_SAFE_NO_PAD,
        bytes,
    ))
}

pub fn hash_token(token: &str) -> Vec<u8> {
    Sha256::digest(token.as_bytes()).to_vec()
}

pub fn hash_activation(pepper: &[u8], code: &str) -> Vec<u8> {
    let mut mac = Hmac::<Sha256>::new_from_slice(pepper).expect("HMAC accepts any key length");
    mac.update(code.trim().to_ascii_uppercase().as_bytes());
    mac.finalize().into_bytes().to_vec()
}

pub fn hash_password(password: &str) -> anyhow::Result<String> {
    let salt = SaltString::generate(&mut argon2::password_hash::rand_core::OsRng);
    Argon2::default()
        .hash_password(password.as_bytes(), &salt)
        .map(|hash| hash.to_string())
        .map_err(|error| anyhow::anyhow!(error.to_string()))
}

pub fn verify_password(encoded: &str, password: &str) -> bool {
    PasswordHash::new(encoded).is_ok_and(|hash| {
        Argon2::default()
            .verify_password(password.as_bytes(), &hash)
            .is_ok()
    })
}

pub fn cookie_value(headers: &axum::http::HeaderMap, name: &str) -> Option<String> {
    headers
        .get(axum::http::header::COOKIE)?
        .to_str()
        .ok()?
        .split(';')
        .find_map(|part| {
            let (key, value) = part.trim().split_once('=')?;
            (key == name).then(|| value.to_owned())
        })
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn passwords_and_tokens_are_not_stored_plaintext() {
        let hash = hash_password("long-enough-password").unwrap();
        assert!(verify_password(&hash, "long-enough-password"));
        assert!(!verify_password(&hash, "wrong"));
        assert_ne!(
            hash_activation(b"pepper", "abc"),
            hash_activation(b"pepper", "abd")
        );
        assert_ne!(opaque_token().unwrap(), opaque_token().unwrap());
    }
}
