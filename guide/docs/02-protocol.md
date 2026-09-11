# 02. Wire Protocol

本文件定义实现必须遵守的 v1 Wire Protocol。FlatBuffers 只编码 Payload；Frame、
握手和记录保护由协议层定义，不依赖具体 IPC。

## 1. Frame

每个 Frame 由 20 字节 Header 和 `payload_length` 字节 Payload 组成：

| Offset | Size | Field | Encoding |
|---:|---:|---|---|
| 0 | 4 | Magic | ASCII `YAL1` |
| 4 | 2 | Frame Version | unsigned, big-endian；v1 为 `1` |
| 6 | 2 | Flags | unsigned, big-endian |
| 8 | 8 | Sequence | unsigned, big-endian |
| 16 | 4 | Payload Length | unsigned, big-endian |

Flags v1：

- `0x0000`：明文握手 Frame。
- `0x0001`：Payload 是 ChaCha20-Poly1305 的 `ciphertext || 16-byte tag`。
- 其他位必须为 0；收到未知位立即关闭连接。

规则：

- 默认 Payload 上限为 1 MiB，必须在分配内存前检查。
- 不接受错误 Magic、未知 Frame Version、零长度 Payload 或长度溢出。
- Header 原始 20 字节是 AEAD Additional Authenticated Data。
- 握手完成后只允许加密 Frame；认证失败不得向对端返回详细原因。
- 明文 `ClientHello`、`ServerHello` 和 `UnsupportedVersion` 的 Sequence 为 0；
  `ClientFinished` 是 Client 到 Service 的 Sequence 1。
- 每个方向使用独立 Sequence，握手后从 1 开始且必须严格递增。
- Sequence 达到 `UINT64_MAX` 前必须断开并重新握手。

## 2. FlatBuffers Envelope

所有 Payload 使用一个根类型 `Envelope`，文件标识为 `YAMS`：

```text
Envelope
├── protocol_version: uint16
├── request_id: uint64
└── body: Message union
```

- 请求的 `request_id` 非零；响应复制对应值。
- Service 主动事件使用 `request_id = 0`。
- 版本协商完成前 Envelope 的 `protocol_version` 为 0；完成后必须等于协商版本。
- 消息类型由 `body_type` 唯一确定，不在 Frame 中重复保存 Command。
- FlatBuffers Payload 遵循其标准 little-endian Wire Format；Frame 整数仍使用
  big-endian，两者不得混用。
- 解密后必须先检查文件标识并运行 FlatBuffers Verifier，再访问字段。
- Verifier 必须使用深度、表数量和 Payload 大小限制。

Schema 演进规则：

- 已发布字段不得改类型、改编号或改变默认值语义。
- 新字段追加到表末尾；删除字段使用 `deprecated`。
- Union 数值一经发布不得复用。
- `flatc` 版本由构建固定；兼容性测试使用历史 Golden Vectors。

## 3. 版本协商

`ClientHello` 提供 `min_protocol_version` 和 `max_protocol_version`，Service 选择双方
支持的最高版本并写入 `ServerHello`。无交集时返回明文、版本为 0 的
`UnsupportedVersion` 后关闭连接。

Server 签名覆盖双方版本范围和最终版本，防止降级。握手完成后，Envelope 的
`protocol_version` 必须等于协商结果。v1 收到未知消息类型时返回
`UnsupportedMessage`；未知字段由 FlatBuffers 兼容规则忽略。

## 4. 消息与状态

```text
Connected
  ClientHello -> ServerHello -> ClientFinished
AuthenticatedTransport
  Authorize -> AuthResult
SessionActive
  Heartbeat, QueryLicense, QueryFeatures, CloseSession
  LicenseChanged, FeatureChanged, LicenseExpired, LicenseRevoked
```

- 握手阶段只允许三个握手消息。
- `Authorize` 成功后才允许 Session 消息。
- 同一 `request_id` 的重复请求不得重复创建 Session；Service 应返回缓存结果或
  `DuplicateRequest`。
- Response 和 Event 共用 Service 到 Client 的 Sequence 空间，因此接收顺序就是
 观察顺序。

关键字段：

- `ClientHello`：版本范围、32 字节 client nonce、32 字节 X25519 公钥、SDK 版本。
- `ServerHello`：选定版本、32 字节 server nonce、32 字节 X25519 公钥、Service
  key ID、64 字节 Ed25519 签名。
- `ClientFinished`：包含 32 字节 `SHA-256(transcript)`，并作为第一个加密 Frame
  发送；成功解密且 Hash 一致即完成 key confirmation。
- `Authorize`：product ID；进程元数据仅作诊断，不作为可信身份。
- `AuthResult`：状态、Session ID、Feature 字符串集合、License 到期时间和
  Heartbeat 参数；不得传输 Session Key。
- `Heartbeat`：Session ID。Sequence 和认证 Tag 只存在于 Frame，不在消息中重复。

## 5. 错误

稳定错误码分组：

```text
1000-1999 Protocol: InvalidFrame, UnsupportedVersion, UnsupportedMessage
2000-2999 License:  LicenseNotFound, InvalidLicense, InvalidSignature,
                    NotYetValid, Expired, MachineMismatch, ProductMismatch,
                    Revoked
3000-3999 Session:  InvalidSession, SessionExpired, SessionLimitExceeded,
                    DuplicateRequest
4000-4999 Auth:     AuthenticationFailed, ServiceIdentityInvalid
5000-5999 Internal: InternalError, StorageError
```

- Frame、解密、Verifier 和握手错误是连接级错误，通常直接关闭连接。
- 普通请求错误通过相同 `request_id` 返回，连接和现有 Session 可继续使用。
- `InvalidSession`、`Expired` 和 `Revoked` 会使对应 Session 终止。

具体数值必须集中定义在 Schema 中；发布后不得重排或复用。

## 6. 时间

- License 时间使用 UTC Unix seconds。
- Heartbeat interval、timeout 和 Grace Period 使用毫秒。
- 超时计算使用单调时钟，不使用可回拨的系统墙上时间。
- v1 不依赖客户端提交的 timestamp 完成防重放。
