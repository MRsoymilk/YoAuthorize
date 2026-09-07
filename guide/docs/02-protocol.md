# 02. License Protocol

## 1. 目标

License Protocol 必须与：

- UI 框架
- 编程语言
- IPC 类型
- 操作系统

解耦。

推荐序列化：

> Protocol Buffers

## 2. 协议分层

```text
Application Message
├── Auth
├── Session
├── Heartbeat
├── Feature
└── License
        ↓
Protocol Buffers
        ↓
Secure Message Layer
├── Sequence
├── Nonce
└── MAC
        ↓
Frame Layer
        ↓
IPC Transport
```

## 3. 建议 Frame Header

```text
┌──────────────┐
│ Magic        │ 4 Bytes
├──────────────┤
│ Version      │ 2 Bytes
├──────────────┤
│ Flags        │ 2 Bytes
├──────────────┤
│ Command      │ 2 Bytes
├──────────────┤
│ Reserved     │ 2 Bytes
├──────────────┤
│ Sequence     │ 8 Bytes
├──────────────┤
│ Payload Len  │ 4 Bytes
├──────────────┤
│ Payload      │ N Bytes
├──────────────┤
│ MAC          │ Optional
└──────────────┘
```

建议：

```text
Magic = 0x4C494345
```

逻辑含义为 `LICE`。

## 4. 命令分类

```text
System
├── Hello
├── Ping
└── Error

Authentication
├── Challenge
├── Authenticate
└── AuthResult

Session
├── CreateSession
├── ResumeSession
├── CloseSession
└── SessionState

License
├── QueryLicense
├── LicenseChanged
├── LicenseExpired
└── LicenseRevoked

Feature
├── QueryFeatures
└── FeatureChanged

Heartbeat
├── Heartbeat
└── HeartbeatAck
```

## 5. Hello

建议包含：

```text
product_id
app_version
protocol_version
process_id
client_nonce
sdk_version
```

## 6. Challenge

Service 返回：

```text
server_nonce
challenge_id
server_timestamp
server_proof
```

## 7. Authenticate

Client 返回：

```text
challenge_id
product_id
process_id
client_nonce
server_nonce
auth_proof
```

## 8. AuthResult

成功：

```text
result = SUCCESS
session_id
session_token / session_material
feature_mask
expire_time
heartbeat_interval
```

失败：

```text
result = ERROR
error_code
error_message
```

## 9. Heartbeat

Request：

```text
session_id
sequence
timestamp
nonce
mac
```

Response：

```text
sequence
server_time
license_state
feature_mask
expire_time
mac
```

## 10. Error Code

建议分类：

```text
1000 Protocol
2000 License
3000 Session
4000 Authentication
5000 Internal
```

例如：

```text
1001 INVALID_REQUEST
1002 UNSUPPORTED_VERSION

2001 INVALID_LICENSE
2002 EXPIRED_LICENSE
2003 MACHINE_MISMATCH
2004 PRODUCT_MISMATCH
2005 REVOKED_LICENSE

3001 INVALID_SESSION
3002 SESSION_EXPIRED
3003 SESSION_LIMIT_EXCEEDED

4001 AUTHENTICATION_FAILED

5001 INTERNAL_ERROR
```

## 11. 协议兼容

必须在所有消息或 Frame 中保留：

```text
protocol_version
```

Protocol Buffers 字段只追加，不复用已发布字段编号。

## 12. 不允许的实现

禁止直接发送 C/C++ struct 内存：

```cpp
send(fd, &obj, sizeof(obj), 0);
```

因为会受到：

- ABI
- Padding
- Endianness
- Compiler
- Language

影响。
