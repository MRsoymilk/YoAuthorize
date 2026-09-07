# 03. IPC Transport 设计

## 1. 原则

Transport 只负责字节传输，不负责 License 业务判断。

统一抽象：

```cpp
class ITransport {
public:
    virtual bool connect() = 0;
    virtual bool send(const uint8_t* data, size_t len) = 0;
    virtual bool receive(std::vector<uint8_t>& data) = 0;
    virtual void close() = 0;
    virtual ~ITransport() = default;
};
```

## 2. Windows

首选：

> Named Pipe

推荐原因：

- 本机 IPC
- 不占 TCP 端口
- 支持 ACL
- 支持多客户端
- Win32 原生支持

主要 API：

```text
CreateNamedPipe
ConnectNamedPipe
CreateFile
ReadFile
WriteFile
DisconnectNamedPipe
```

## 3. Linux / macOS

首选：

> Unix Domain Socket

Linux 推荐路径：

```text
/run/<product>/license.sock
```

不建议默认使用全局可写 `/tmp`，除非正确处理：

- 文件权限
- socket owner
- sticky bit
- symlink 风险

## 4. TCP Loopback

作为：

- 调试模式
- 测试模式
- 特殊兼容模式

可以支持：

```text
127.0.0.1:<port>
```

但需要更强的：

- Server Authentication
- Session Authentication
- 防重放
- 端口访问控制

## 5. 推荐平台策略

```text
Windows → Named Pipe
Linux   → Unix Domain Socket
macOS   → Unix Domain Socket
Debug   → TCP 127.0.0.1
```

## 6. Connection 生命周期

```text
Disconnected
    │
    ▼
Connecting
    │
    ├── success
    ▼
Connected
    │
    ▼
Authenticated
    │
    ▼
SessionActive
    │
    ├── timeout
    ├── service restart
    └── transport error
    ▼
Reconnecting
```

## 7. 自动重连

推荐指数或阶梯退避：

```text
1s
2s
5s
10s
```

授权运行期间允许 Grace Period。

## 8. 权限控制

Windows Named Pipe：

- 限定当前用户
- 或限定指定 Service SID / 用户组
- 不允许 Everyone 全权限

Unix Socket：

- 合理 owner/group
- 推荐 0600 / 0660
- socket 所在目录必须限制写权限

## 9. 多客户端

Service 应支持：

```text
Client A → Session A
Client B → Session B
Client C → Session C
```

是否允许同时存在由 License：

```text
max_sessions
```

控制。

## 10. Transport 与协议解耦

同一套消息：

```text
AuthRequest
Heartbeat
FeatureQuery
```

应能够在 Named Pipe / Unix Socket / TCP 上原样使用。
