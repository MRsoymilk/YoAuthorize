# 05. Session 与 Heartbeat

## 1. Session 模型

授权成功后，Service 创建随机 128 位 Session ID，并保存：

```text
session_id, product_id, peer_identity
created_at, last_heartbeat_monotonic
license_expire_time, features, state
```

Session Key 由握手派生并绑定当前连接，不通过协议传输，也不得写入日志或持久化。

状态：

```text
Active -> Grace -> Active
   |         |
   +---------+-> Expired / Revoked / Closed
```

`Grace` 是 SDK 在连接丢失后的本地状态。Service 端 Session 只保留到 Heartbeat
timeout，超时后释放并发名额。

## 2. Heartbeat

默认参数：

- interval：5 秒
- Service timeout：15 秒
- SDK Grace Period：30 秒

这些值由 Service 在 `AuthResult` 返回，SDK 应限制到自身支持的安全范围。单次 IPC
失败不能立即使应用退出。间隔和超时使用单调时钟；License 到期使用墙上时间与
可信时间策略。

Heartbeat Ack 返回最新 License 状态、Feature Set 和到期时间。SDK 只接受经过
当前 Session AEAD 通道且 Sequence 合法的结果。

## 3. 最大并发 Session

- `max_sessions` 的检查与 Session 创建必须是一个原子操作。
- Session 绑定 OS IPC 对端身份；客户端自报 PID 仅用于诊断。
- 进程断开或超时后释放名额。
- SDK Grace 期间，旧 Service Session 可能仍占用名额；重连时 Service 应识别同一
  peer/product 的过期连接并避免永久锁死。

## 4. Service 重启与重连

MVP 的 Session 和会话密钥均不持久化。Service 重启后：

1. 旧连接和 Session 全部失效。
2. SDK 进入 Grace 并按退避策略重连。
3. SDK 执行新的握手和完整 License 授权。
4. 成功后替换本地 Session 并退出 Grace。

`ResumeSession` 不属于 v1。未来若增加，必须使用短期、单次、可撤销的恢复凭据，
不能只凭旧 Session ID 恢复。

## 5. 授权变化

Service 可发送：

- `LicenseChanged`
- `FeatureChanged`
- `LicenseExpired`
- `LicenseRevoked`

SDK 必须先原子更新本地快照，再通知应用。Feature 减少、到期和吊销默认 fail
closed；应用负责禁用功能、保存状态并安全退出。
