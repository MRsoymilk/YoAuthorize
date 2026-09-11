# 07. 密码学设计

## 1. 固定算法套件

v1 不进行算法自由协商：

- License Signature：Ed25519
- Service Authentication：独立的 Ed25519 身份密钥
- Ephemeral Key Exchange：X25519
- KDF：HKDF-SHA256
- Record Protection：ChaCha20-Poly1305
- Hash：SHA-256
- Random：操作系统 CSPRNG
- C++ Provider：OpenSSL 3.x 高层 EVP/KDF/RAND API

License 签名密钥与 Service 身份密钥具有不同 key ID、用途和轮换周期，不得复用。

## 2. Service 认证握手

1. Client 生成 32 字节 nonce 和 ephemeral X25519 key pair，发送 `ClientHello`。
2. Service 生成独立 nonce 和 ephemeral key pair，选择协议版本。
3. Service 对 transcript 签名并发送 `ServerHello`。
4. Client 使用 SDK 内置的 Service 身份公钥验证签名。
5. 双方计算 X25519 shared secret，经 HKDF 派生方向密钥和 nonce salt。
6. Client 发送首个加密 `ClientFinished`，Service 验证后进入加密状态。

Transcript v1 是以下字段的长度前缀拼接，不依赖 FlatBuffers 重新序列化：

```text
"YOAUTHORIZE-SERVICE-HANDSHAKE-V1"
client version range, selected version
client nonce, server nonce
client ephemeral public key, server ephemeral public key
service key ID
```

所有整数使用 big-endian，变长字段使用 `u16be(length) || bytes`。Service 签名为
64 字节 Ed25519 signature。Nonce 和 ephemeral public key 都必须恰好 32 字节。

客户端没有可保密的长期凭据，因此握手不声称密码学认证客户端。Service 使用 IPC
ACL 和 OS peer credentials 限制调用者；可选的可执行文件校验只是额外策略。

## 3. 密钥派生与记录保护

```text
salt = SHA-256(transcript)
prk  = HKDF-Extract(salt, x25519_shared_secret)
c2s_key   = HKDF-Expand(prk, "YA-V1-C2S-KEY", 32)
s2c_key   = HKDF-Expand(prk, "YA-V1-S2C-KEY", 32)
c2s_salt  = HKDF-Expand(prk, "YA-V1-C2S-NONCE", 4)
s2c_salt  = HKDF-Expand(prk, "YA-V1-S2C-NONCE", 4)
```

ChaCha20-Poly1305 nonce 为 `4-byte directional salt || u64be(sequence)`。两个方向
Sequence 独立，从 1 开始并严格递增。Frame Header 原始字节作为 AAD。任何 Tag、
Sequence 或解密失败都立即终止连接，不能尝试继续解析。

## 4. 随机数与内存

- 使用 OpenSSL `RAND_priv_bytes`，并检查返回值；OpenSSL 必须正确接入平台 CSPRNG。
- 禁止使用 `rand()`、时间、PID 或硬件标识生成 nonce、key 或 Session ID。
- 私钥、shared secret 和 Session Key 不记录日志；生命周期结束后尽力清零内存。
- 不使用已废弃的 OpenSSL low-level API，不自行拼装曲线运算或 AEAD。

## 5. Key Rotation

- SDK 可内置多个标记为 `service-auth` 的公钥。
- Service 可配置多个标记为 `license-signing` 的验证公钥。
- 新 key 先随软件/配置发布，再用于签名；旧 key 在兼容窗口后吊销。
- 未知 key ID、错误用途或降级到非 v1 算法一律拒绝。

## 6. 时间与本地状态

离线系统无法仅凭本机时间彻底防止回拨。Service 保存受保护的
`last_valid_time`，检测明显倒退并返回明确状态；在线可信时间和 TPM-backed state
属于后续增强，不能在 MVP 文档中宣称已解决。
