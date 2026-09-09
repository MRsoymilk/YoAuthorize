mod generated;

use ed25519_dalek::{Signature, Signer, SigningKey, Verifier, VerifyingKey};
use flatbuffers::FlatBufferBuilder;
use generated::yoauthorize::license as fb;
use serde::{Deserialize, Serialize};

pub const FORMAT_VERSION: u16 = 1;
pub const MAX_PACKAGE_SIZE: usize = 1024 * 1024;
pub const MAX_FEATURES: usize = 256;
pub const MAX_STRING_BYTES: usize = 1024;
const DOMAIN: &[u8] = b"YOAUTHORIZE-LICENSE-V1";

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum LicenseKind {
    Trial,
    Subscription,
    Permanent,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct LicenseDefinition {
    pub license_id: String,
    pub product_id: String,
    pub customer_id: String,
    pub issue_time: u64,
    pub not_before: u64,
    pub expire_time: u64,
    pub license_type: LicenseKind,
    pub features: Vec<String>,
    pub max_sessions: u32,
    pub machine_id: Option<String>,
}

#[derive(Debug, thiserror::Error)]
pub enum FormatError {
    #[error("invalid license definition: {0}")]
    Invalid(&'static str),
    #[error("invalid FlatBuffer")]
    Flatbuffer,
    #[error("unknown or mismatched signing key")]
    Key,
    #[error("invalid signature")]
    Signature,
}

pub fn validate(def: &LicenseDefinition) -> Result<(), FormatError> {
    let strings = [&def.license_id, &def.product_id, &def.customer_id];
    if strings
        .iter()
        .any(|s| s.is_empty() || s.len() > MAX_STRING_BYTES)
    {
        return Err(FormatError::Invalid("identifier length"));
    }
    if def.features.len() > MAX_FEATURES
        || def.features.iter().any(|s| {
            s.is_empty()
                || s.len() > MAX_STRING_BYTES
                || !s
                    .bytes()
                    .all(|c| c.is_ascii_alphanumeric() || b"_.-".contains(&c))
        })
    {
        return Err(FormatError::Invalid("features"));
    }
    if def.max_sessions == 0 {
        return Err(FormatError::Invalid("max_sessions"));
    }
    if def.license_type == LicenseKind::Permanent {
        if def.expire_time != 0 {
            return Err(FormatError::Invalid("permanent expiration"));
        }
    } else if def.expire_time <= def.not_before {
        return Err(FormatError::Invalid("expiration"));
    }
    if def
        .machine_id
        .as_ref()
        .is_some_and(|s| s.is_empty() || s.len() > MAX_STRING_BYTES)
    {
        return Err(FormatError::Invalid("machine_id"));
    }
    Ok(())
}

fn payload_bytes(def: &LicenseDefinition) -> Result<Vec<u8>, FormatError> {
    validate(def)?;
    let mut b = FlatBufferBuilder::new();
    let license_id = b.create_string(&def.license_id);
    let product_id = b.create_string(&def.product_id);
    let customer_id = b.create_string(&def.customer_id);
    let feature_strings: Vec<_> = def.features.iter().map(|s| b.create_string(s)).collect();
    let features = b.create_vector(&feature_strings);
    let machine_id = def.machine_id.as_ref().map(|s| b.create_string(s));
    let machine_policy = fb::MachinePolicy::create(
        &mut b,
        &fb::MachinePolicyArgs {
            mode: if def.machine_id.is_some() {
                fb::MachineMode::Exact
            } else {
                fb::MachineMode::Unbound
            },
            machine_id,
        },
    );
    let payload = fb::LicensePayload::create(
        &mut b,
        &fb::LicensePayloadArgs {
            license_id: Some(license_id),
            product_id: Some(product_id),
            customer_id: Some(customer_id),
            issue_time: def.issue_time,
            not_before: def.not_before,
            expire_time: def.expire_time,
            license_type: match def.license_type {
                LicenseKind::Trial => fb::LicenseType::Trial,
                LicenseKind::Subscription => fb::LicenseType::Subscription,
                LicenseKind::Permanent => fb::LicenseType::Permanent,
            },
            features: Some(features),
            max_sessions: def.max_sessions,
            machine_policy: Some(machine_policy),
        },
    );
    b.finish(payload, None);
    Ok(b.finished_data().to_vec())
}

pub fn signing_message(key_id: &str, payload: &[u8]) -> Result<Vec<u8>, FormatError> {
    if key_id.is_empty() || key_id.len() > u16::MAX as usize || payload.len() > u32::MAX as usize {
        return Err(FormatError::Invalid("signing input"));
    }
    let mut out = Vec::with_capacity(DOMAIN.len() + 8 + key_id.len() + payload.len());
    out.extend_from_slice(DOMAIN);
    out.extend_from_slice(&FORMAT_VERSION.to_be_bytes());
    out.extend_from_slice(&(key_id.len() as u16).to_be_bytes());
    out.extend_from_slice(key_id.as_bytes());
    out.extend_from_slice(&(payload.len() as u32).to_be_bytes());
    out.extend_from_slice(payload);
    Ok(out)
}

pub fn generate(
    def: &LicenseDefinition,
    key_id: &str,
    key: &SigningKey,
) -> Result<Vec<u8>, FormatError> {
    let payload = payload_bytes(def)?;
    let signature = key.sign(&signing_message(key_id, &payload)?).to_bytes();
    let mut b = FlatBufferBuilder::new();
    let key_id = b.create_string(key_id);
    let payload = b.create_vector(&payload);
    let signature = b.create_vector(&signature);
    let package = fb::LicensePackage::create(
        &mut b,
        &fb::LicensePackageArgs {
            format_version: FORMAT_VERSION,
            key_id: Some(key_id),
            payload: Some(payload),
            signature: Some(signature),
        },
    );
    b.finish(package, Some("YALC"));
    let bytes = b.finished_data().to_vec();
    if bytes.len() > MAX_PACKAGE_SIZE {
        return Err(FormatError::Invalid("package size"));
    }
    Ok(bytes)
}

pub fn verify(
    package: &[u8],
    expected_key_id: &str,
    key: &VerifyingKey,
) -> Result<(), FormatError> {
    if package.len() > MAX_PACKAGE_SIZE
        || !flatbuffers::buffer_has_identifier(package, "YALC", false)
    {
        return Err(FormatError::Flatbuffer);
    }
    let package = fb::root_as_license_package(package).map_err(|_| FormatError::Flatbuffer)?;
    if package.format_version() != FORMAT_VERSION || package.key_id() != Some(expected_key_id) {
        return Err(FormatError::Key);
    }
    let payload = package.payload().ok_or(FormatError::Flatbuffer)?.bytes();
    let signature: [u8; 64] = package
        .signature()
        .ok_or(FormatError::Signature)?
        .bytes()
        .try_into()
        .map_err(|_| FormatError::Signature)?;
    key.verify(
        &signing_message(expected_key_id, payload)?,
        &Signature::from_bytes(&signature),
    )
    .map_err(|_| FormatError::Signature)?;
    flatbuffers::root::<fb::LicensePayload<'_>>(payload).map_err(|_| FormatError::Flatbuffer)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    fn definition() -> LicenseDefinition {
        LicenseDefinition {
            license_id: "license-1".into(),
            product_id: "product-qt".into(),
            customer_id: "user-1".into(),
            issue_time: 10,
            not_before: 10,
            expire_time: 20,
            license_type: LicenseKind::Subscription,
            features: vec!["capture".into(), "export".into()],
            max_sessions: 2,
            machine_id: Some("opaque-machine".into()),
        }
    }

    #[test]
    fn generated_package_is_yalc_and_verifies() {
        let key = SigningKey::from_bytes(&[7; 32]);
        let bytes = generate(&definition(), "key-1", &key).unwrap();
        assert!(flatbuffers::buffer_has_identifier(&bytes, "YALC", false));
        verify(&bytes, "key-1", &key.verifying_key()).unwrap();
    }

    #[test]
    fn mutation_invalidates_signature() {
        let key = SigningKey::from_bytes(&[9; 32]);
        let mut bytes = generate(&definition(), "key-1", &key).unwrap();
        let index = bytes
            .windows(b"capture".len())
            .position(|window| window == b"capture")
            .unwrap();
        bytes[index] ^= 1;
        assert!(verify(&bytes, "key-1", &key.verifying_key()).is_err());
    }
}
