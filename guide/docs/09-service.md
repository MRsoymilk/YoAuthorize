# 09. License Service 设计

## 1. 职责

LicenseService 是授权系统的本地核心。

负责：

- IPC Server
- Protocol Decode/Encode
- License 验证
- Machine Identity
- Session
- Heartbeat
- Feature
- 本地状态
- 在线同步（后续）

## 2. 内部模块

```text
LicenseService
├── TransportServer
├── ConnectionManager
├── ProtocolDispatcher
├── AuthHandler
├── LicenseManager
├── MachineManager
├── SessionManager
├── FeatureManager
├── CryptoManager
├── StateStorage
└── OnlineClient
```

## 3. Dispatcher

负责：

```text
Frame
  ↓
Decode
  ↓
Command
  ↓
Handler
```

例如：

```text
HELLO       → AuthHandler
HEARTBEAT   → SessionManager
QUERY_FEATURE → FeatureManager
```

## 4. LicenseManager

负责：

- Load License
- Verify Signature
- Verify Product
- Verify Expiration
- Evaluate Feature
- License Reload

## 5. SessionManager

负责：

- Create
- Validate
- Heartbeat
- Timeout
- Max Session
- Destroy

## 6. MachineManager

负责：

- 收集平台身份
- 标准化
- 计算指纹
- 容错匹配

## 7. StateStorage

保存：

- Last valid time
- Activation state
- License metadata
- Sync metadata

不应明文保存：

- 私钥
- Session key 日志
- 用户密码

## 8. 运行方式

Windows：

第一版：

```text
LicenseService.exe
```

正式部署可扩展：

```text
Windows Service
```

Linux：

```text
systemd / OpenRC service
```

macOS：

```text
launchd
```

## 9. Service 启动

```text
Load Config
   ↓
Load Public Keys
   ↓
Load License
   ↓
Load State
   ↓
Verify Environment
   ↓
Start IPC
```

## 10. Service Shutdown

应：

- Stop Accept
- Notify Clients
- Flush State
- Destroy Sessions
- Close Transport
