# 12. 开发路线图

## Phase 1：Local License MVP

目标：

> 建立完整可运行闭环。

实现：

- Protocol 基础消息
- Windows Named Pipe
- Linux Unix Socket
- License 文件
- Ed25519 签名
- Machine ID
- Session
- Heartbeat
- Feature Mask
- C/C++ SDK
- TestApp
- LicenseGenerator

运行链路：

```text
LicenseGenerator
      ↓
 license.dat
      ↓
LicenseService
      ↕
License SDK
      ↕
TestApp
```

## Phase 2：Protocol Security

增加：

- Challenge-Response
- Server Authentication
- X25519
- Session Key
- HMAC / AEAD
- Sequence
- Nonce
- Replay Protection
- PID + Process Start Time
- Grace Period
- Secure State Storage

## Phase 3：Online Activation

增加：

```text
LicenseServer
```

能力：

- Activation Key
- Device registration
- Device unbind
- Revocation
- Online Feature Update
- Trusted Time
- License Refresh

## Phase 4：SDK Ecosystem

增加：

- C ABI
- C++ wrapper
- C# binding
- Python binding
- Rust native API
- Java/JNI
- Go

## Phase 5：Client Hardening

按商业价值选择：

- Binary Integrity
- Code Obfuscation
- Anti-Debug
- Anti-Hook
- Encrypted Resource
- Hardware Dongle
- TPM Binding

## 第一阶段建议开发顺序

```text
1. protocol
2. IPC transport
3. Service dispatcher
4. SDK connect
5. License load
6. Ed25519 verify
7. Machine ID
8. Session
9. Heartbeat
10. Feature
11. TestApp
12. LicenseGenerator
```

## MVP 完成判定

满足以下条件即可认为第一阶段完成：

- TestApp 可通过 SDK 连接 Service
- 合法 License 可启动
- 修改 License 后失败
- 换机器后失败
- License 过期后失败
- Heartbeat 可检测 Service 异常
- Service 重启后 SDK 可恢复
- Feature 可控制测试按钮
- max_sessions 可限制多实例
