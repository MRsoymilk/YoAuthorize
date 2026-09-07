# 07. 密码学设计

## 1. 目标

保护：

- License 真实性
- IPC 消息完整性
- Session
- 防重放
- 服务端身份

## 2. License 签名

推荐：

> Ed25519

Private Key：

```text
LicenseGenerator / LicenseServer
```

Public Key：

```text
LicenseService / SDK（按需要）
```

## 3. Challenge-Response

Client：

```text
client_nonce
```

Service：

```text
server_nonce
```

认证证明应绑定：

```text
client_nonce
server_nonce
product_id
process identity
protocol version
```

## 4. Server Authentication

需要防止 Fake LicenseService。

可由 Service 对 Challenge / Handshake Transcript 签名。

SDK 内保存验证公钥。

## 5. Session Key

可以使用：

> X25519

进行临时密钥协商。

之后用于：

- HMAC
- AEAD

## 6. IPC 完整性

最小方案：

```text
HMAC-SHA256
```

覆盖：

```text
header
sequence
timestamp
payload
nonce
```

更完整方案：

```text
ChaCha20-Poly1305
```

提供：

- 加密
- 完整性
- Authentication Tag

## 7. 防重放

至少使用：

- Sequence
- Nonce
- Session ID

收到：

```text
sequence <= last_sequence
```

应拒绝。

## 8. 随机数

必须使用 OS CSPRNG：

- Windows BCryptGenRandom
- Linux getrandom
- Rust rand_core / OsRng
- libsodium randombytes

不得使用：

```text
rand()
time(NULL)
```

生成安全随机数。

## 9. 本地敏感状态

Windows：

- DPAPI

Linux：

- 权限隔离
- system keyring / secret service（按环境）
- 可结合 TPM

## 10. Key Rotation

License 中包含：

```text
key_id
```

Service 支持多个验证公钥，以便后续轮换签名密钥。

## 11. 不推荐

- 自研加密算法
- Base64 当加密
- XOR
- MD5 作为签名
- CRC 作为防篡改
