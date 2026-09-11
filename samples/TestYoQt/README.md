# TestYoQt Authorization Sample

`TestYoQt` demonstrates fail-closed feature authorization with an English Qt 6
Widgets interface. It supports two sources:

- A locally stored, signed `.yalc` license.
- An HTTPS activation endpoint that returns the same signed license as Base64
  JSON data.

Both paths verify the Ed25519 signature, product, validity period, machine
policy, and feature list locally before enabling `Capture` or `Export`.

Build from the repository root:

```bash
cmake -S . -B build \
  -DBUILD_TESTING=ON \
  -DYOAUTHORIZE_BUILD_TOOLS=ON \
  -DYOAUTHORIZE_BUILD_SAMPLES=ON
cmake --build build --target TestYoQt
```

Run the reproducible local and HTTPS demos:

```bash
bash tools/run_qt_authorization_demo.sh build all
```

The HTTPS activation request uses this JSON contract:

```json
{
  "protocol_version": 1,
  "product_id": "product-qt",
  "activation_code": "activate-qt",
  "machine_id": "opaque-machine-id"
}
```

The response contains a signed license and reserved real-time channel fields:

```json
{
  "license": "base64-encoded-yalc",
  "realtime_url": "wss://license.example.com/v1/events",
  "access_token": "short-lived-token",
  "token_expire_time": 0
}
```

The sample does not connect to `realtime_url` yet. Activation uses HTTPS;
future authorization events and distributed jobs should use a separate WSS
channel authenticated with the short-lived token.
