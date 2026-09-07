# 11. 测试方案

## 1. 测试目标

验证：

- 正确性
- 兼容性
- 稳定性
- 异常恢复
- 基本安全性

## 2. 测试层级

```text
Unit Test
Protocol Test
Crypto Test
IPC Test
Integration Test
Failure Test
Security Test
Cross-platform Test
```

## 3. TestApp

建议创建独立 TestApp，显示：

```text
Service State
License State
Product
Machine ID
Expire Time
Session ID
Heartbeat State
Feature List
Last Error
```

## 4. 正常授权

条件：

```text
License valid
Machine match
Not expired
```

期望：

```text
Session created
Heartbeat OK
Features correct
```

## 5. License 缺失

期望：

```text
LICENSE_NOT_FOUND / INVALID_LICENSE
```

## 6. License 被修改

修改：

```text
expire_time
features
machine_id
```

期望：

```text
INVALID_SIGNATURE
```

## 7. License 过期

期望：

```text
EXPIRED_LICENSE
```

## 8. Machine 不一致

复制 License 到另一设备。

期望：

```text
MACHINE_MISMATCH
```

## 9. Service 崩溃

步骤：

1. 启动 TestApp
2. Session 正常
3. 强制终止 LicenseService
4. 观察 Heartbeat
5. 重启 LicenseService

验证：

- 超时
- Grace Period
- 自动重连
- Session 重建
- 不发生业务数据损坏

## 10. Feature 动态变化

运行时：

```text
FFT ON → OFF
```

期望：

```text
FeatureChanged
```

业务立即禁用功能。

## 11. 多实例

License：

```text
max_sessions = 1
```

启动两个 App。

期望第二个：

```text
SESSION_LIMIT_EXCEEDED
```

## 12. Replay Test

重复发送旧：

- Auth
- Heartbeat
- Session message

期望被：

```text
sequence / nonce
```

拒绝。

## 13. Fake Service Test

构造假的 IPC Server。

若 SDK 开启 Server Authentication，应拒绝。

## 14. Fuzz Test

重点 Fuzz：

- Frame Header
- Length
- Command
- Protobuf Payload
- Invalid MAC
- Huge Payload
- Truncated Frame

Rust Service 也必须认为 IPC 输入不可信。

## 15. 性能

授权系统不属于高吞吐业务。

建议关注：

- IPC connect latency
- auth latency
- heartbeat CPU
- service memory
- 100/1000 client 并发（按产品需求）
