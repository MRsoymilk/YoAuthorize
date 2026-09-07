# 05. Session 与 Heartbeat

## 1. 为什么需要 Session

不应该只在启动时执行一次：

```text
license == valid
```

而应建立一个持续存在的授权会话。

## 2. Session 数据

建议：

```text
session_id
product_id
process_id
create_time
last_heartbeat_time
expire_time
feature_mask
session_key / key material
state
```

## 3. 生命周期

```text
Created
   │
   ▼
Authenticated
   │
   ▼
Active
   │
   ├── Heartbeat
   ├── Feature Query
   ├── License Query
   └── Push Event
   │
   ▼
Expired / Revoked / Closed
```

## 4. Heartbeat

默认建议：

```text
Heartbeat Interval = 5s
Timeout = 15s
```

不要因为单次 IPC 失败立即退出。

## 5. Grace Period

Service 暂时不可用：

```text
Heartbeat Failed
    ↓
Reconnect
    ↓
Grace Period
    ↓
Recovery / Safe Shutdown
```

第一版可设置：

```text
Grace Period = 30s
```

实际值按业务场景配置。

## 6. Session 与 PID 绑定

可记录：

```text
PID
Process Start Time
Executable Identity
```

仅 PID 不够，因为 PID 会复用。

## 7. 多实例

License：

```text
max_sessions = 1
```

则第二个实例应被拒绝。

可扩展：

- per-machine sessions
- per-user sessions
- per-product sessions

## 8. Session 恢复

Service 重启后可支持：

```text
ResumeSession
```

但必须重新验证：

- Client identity
- License
- Machine
- Session freshness

不能只凭旧 Session ID 恢复。

## 9. 授权状态变化

Service 可主动推送：

```text
LicenseChanged
FeatureChanged
LicenseExpired
LicenseRevoked
```

业务程序响应：

- 禁用功能
- 停止设备
- 保存状态
- 安全退出
