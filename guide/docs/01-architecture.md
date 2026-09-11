# 01. 系统架构

## 1. 组件

```text
                       License Server (future)
                                  |
                                HTTPS
                                  |
Application -> C++ SDK -> Local IPC -> License Service
                                      |-- Protocol / Dispatcher
                                      |-- License Core
                                      |-- Machine Identity
                                      |-- Session / Feature
                                      |-- Crypto / State Storage
```

License Server 不在本地授权链路的可用性关键路径中。离线 License 必须在无网络时
完成验证。

## 2. 依赖方向

```text
Crypto <------- Protocol -------> Core
  ^                ^               |
  |                |               v
  +------ SDK ------+----------> Service
           ^                ^
           |                |
      Application       Transport
```

- Protocol 不得依赖授权策略、平台 API 或 UI。
- Crypto 不得依赖 Protocol、Core、SDK 或 Service。
- Transport 不得解析 Frame 或 FlatBuffers。
- Core 不得依赖 IPC 和 Service 进程管理。
- Service 和 SDK 可以依赖 Protocol，不得互相依赖内部实现。
- Application 只依赖 SDK 的公开类型。

## 3. 运行流程

```text
Application       SDK                 Service
    | initialize   |                     |
    |------------->| connect IPC         |
    |              |-------------------->|
    |              | authenticated       |
    |              | handshake           |
    |              |<------------------->|
    |              | authorize product   |
    |              |-------------------->|
    |              | session + features  |
    |              |<--------------------|
    | result        |                     |
    |<--------------|                     |
    |              heartbeat/event       |
    |              |<------------------->|
```

握手只认证 Service 并建立加密通道；License 授权发生在通道内。客户端提交的 PID、
路径和产品 ID 都不可信，Service 应优先使用 IPC 提供的操作系统对端身份。

## 4. 状态与故障

- IPC 短暂失败：SDK 进入 `Grace`，按退避策略重连。
- Service 重启：MVP 不恢复旧 Session；SDK 重新握手并授权。
- License 更新：Service 原子验证新文件，再重算所有 Session。
- Feature 减少或 License 失效：Service 先发送事件，SDK 更新本地状态。
- Grace 到期仍未恢复：应用执行可控降级、保存数据和安全退出。

强制终止应用只能作为独立的额外防护，不能替代上述正常控制流程。
