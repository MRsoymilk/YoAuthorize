# 10. 威胁模型

## 1. 边界

目标是阻止 License 篡改、跨机器复制、伪造 Service、IPC 重放和简单单点 Patch，
并提高在用户控制的本机上绕过授权的成本。系统不承诺客户端绝对不可破解。

信任：

- 受控签发环境中的 License 私钥。
- SDK 内置的 Service 身份公钥。
- 正确配置的 Service 账户、IPC ACL 和系统密码学实现。

不信任：

- IPC 输入、License 文件、客户端自报 PID/路径/时间。
- 普通用户可修改的配置、环境变量和目录。
- 应用进程内存及完全控制本机的管理员/root。

## 2. 威胁与基线防护

| Threat | Required control |
|---|---|
| 修改 License | Ed25519 签名覆盖原始 Payload |
| 复制 License | 明确的 Machine Policy |
| Fake Service | 独立 Service 身份签名和 SDK pinning |
| IPC 窃听/修改 | X25519 + HKDF + ChaCha20-Poly1305 |
| Replay/乱序 | 每方向严格递增 Sequence |
| 未授权本地进程 | Named Pipe ACL / Unix peer credentials |
| 多实例竞态 | 原子 Session 配额分配 |
| Service 终止 | Heartbeat、Grace、重连和安全降级 |
| 系统时间回拨 | protected last-valid time；后续可信时间 |
| 畸形输入 | 长度上限、FlatBuffers Verifier、状态机校验 |

Server Authentication 和 AEAD 是发布版必需能力，不是可选加固。

## 3. 已知限制

- 客户端没有可安全隐藏的长期 secret，Service 不能仅凭协议证明应用未被修改。
- 管理员/root 可以调试、Hook、替换 SDK 或 Patch 业务判断。
- 本地 `last_valid_time` 只能检测部分时间回拨，不能替代在线可信时间或 TPM。
- Machine ID 是授权策略，不能证明设备具有不可克隆的硬件身份。

因此应用应在多个关键功能点检查 Feature，并让授权派生状态参与资源解锁。单个
`if (licensed)` 仍然容易被 Patch。

## 4. 数据保护

- 不记录私钥、Session Key、shared secret、完整 License 或原始硬件标识。
- 错误响应不区分 AEAD、签名或密钥细节，详细原因只进入受限本地审计日志。
- State 和 trust key 目录由 Service 账户控制，采用原子写入和权限检查。
- 崩溃转储和诊断模式不得默认包含密码学上下文。

## 5. 后续加固

代码完整性、混淆、Anti-Debug、Anti-Hook、TPM 和硬件 Dongle 是第二层措施，必须
在协议与密钥管理正确后按商业价值选择，不能替代基线防护。
