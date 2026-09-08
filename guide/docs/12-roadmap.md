# 12. 开发路线图

每个阶段完成后先冻结对应接口和测试向量，再进入下一阶段。未完成安全基线的构建
只能用于开发验证，不能作为生产授权系统发布。

## Phase 0：Protocol Foundation

- 固定 `protocol.fbs`、`license.fbs` 和 `flatc` 版本
- 实现 Frame codec、FlatBuffers Verifier 和错误模型
- 建立 Golden Vectors、Fuzz target 和 Schema 兼容测试
- 接入 Ed25519、X25519、HKDF-SHA256、ChaCha20-Poly1305 标准向量

## Phase 1：C++ Local License MVP

- C++20 License Core、Service 和原生 C++ SDK
- Linux Unix Domain Socket 和 `SO_PEERCRED`
- Service 身份认证和加密 Session
- License Generator、Ed25519 验签和 key ID
- Exact Machine ID、有效期、Feature Set 和 `max_sessions`
- Heartbeat、Grace、重连和 License 原子热更新
- C++ TestApp 与 Linux 集成测试

Phase 1 只有在 Fake Service、Replay、畸形输入和并发 Session 测试通过后才可发布。

## Phase 2：运维与平台完善

- systemd 配置和 Linux 安装程序
- Windows Named Pipe、Windows Service 和平台集成测试
- 受保护状态、时间回拨检测、日志与审计
- key rotation 和本地吊销列表
- macOS Unix Domain Socket、launchd 和平台测试
- 性能、资源上限、故障注入和升级/回滚测试

## Phase 3：Online Capability

- 激活码、设备注册/解绑和远程吊销
- 在线 Feature 更新、可信时间和 License 刷新
- Service 到 License Server 的 TLS 身份认证
- 离线缓存和服务不可达策略

## Phase 4：SDK Ecosystem

- 稳定 C ABI 和 ABI 兼容测试
- C#、Python、Rust 包装
- 示例、打包、版本兼容矩阵和发布流程
- 只有确有需要时才实现各语言原生 Wire Protocol

## Phase 5：Client Hardening

- Binary integrity、关键资源加密和符号控制
- 按产品价值选择混淆、Anti-Debug、Anti-Hook、TPM 或硬件 Dongle

## MVP 实现顺序

```text
Schema/Frame -> Crypto vectors -> Service handshake -> IPC
-> License verify -> Machine ID -> Session/Feature
-> C++ SDK/Heartbeat -> Generator/TestApp -> Integration/Fuzz
```
