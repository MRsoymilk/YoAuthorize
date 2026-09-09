use license_format::LicenseDefinition;
use serde::{Deserialize, Serialize};

#[derive(Clone)]
pub struct SignerClient {
    client: reqwest::Client,
    endpoint: String,
    secret: String,
}

#[derive(Serialize)]
struct SignRequest<'a> {
    definition: &'a LicenseDefinition,
}

#[derive(Deserialize)]
struct SignResponse {
    license: String,
    key_id: String,
}

#[derive(Deserialize)]
pub struct SigningKeyInfo {
    pub key_id: String,
    pub public_key: String,
}

pub struct SignedLicense {
    pub license: String,
    pub key_id: String,
}

impl SignerClient {
    pub fn new(endpoint: String, secret: String) -> Self {
        Self {
            client: reqwest::Client::builder()
                .no_proxy()
                .connect_timeout(std::time::Duration::from_secs(3))
                .timeout(std::time::Duration::from_secs(10))
                .build()
                .expect("static HTTP client configuration is valid"),
            endpoint,
            secret,
        }
    }
    pub async fn key_info(&self) -> anyhow::Result<SigningKeyInfo> {
        Ok(self
            .client
            .get(format!("{}/internal/v1/signing-key", self.endpoint))
            .bearer_auth(&self.secret)
            .send()
            .await?
            .error_for_status()?
            .json()
            .await?)
    }
    pub async fn sign(&self, definition: &LicenseDefinition) -> anyhow::Result<SignedLicense> {
        let response = self
            .client
            .post(format!("{}/internal/v1/licenses/sign", self.endpoint))
            .bearer_auth(&self.secret)
            .json(&SignRequest { definition })
            .send()
            .await?
            .error_for_status()?
            .json::<SignResponse>()
            .await?;
        Ok(SignedLicense {
            license: response.license,
            key_id: response.key_id,
        })
    }
}
