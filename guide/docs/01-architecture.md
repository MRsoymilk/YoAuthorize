# 01. 系统架构

## 1. 总体架构

```text
                     ┌───────────────────────┐
                     │    License Server     │
                     │       Optional        │
                     └───────────┬───────────┘
                                 │ HTTPS
                                 ▼
┌─────────────────────────────────────────────────────┐
│                  License Service                    │
│                                                     │
│  License Engine                                     │
│  Session Engine                                     │
│  Machine Identity                                   │
│  Feature Control                                    │
│  Crypto                                             │
│  Storage                                            │
└───────────────────────┬─────────────────────────────┘
                        │
                  IPC Transport
                        │
          ┌─────────────┼─────────────┐
          │             │             │
          ▼             ▼             ▼
     Named Pipe    Unix Socket    TCP Loopback
          │             │             │
          └─────────────┼─────────────┘
                        │
                License Protocol
                        │
          ┌─────────────┼──────────────┐
          │             │              │
          ▼             ▼              ▼
       C++ SDK       Rust SDK       C# SDK
          │             │              │
          ▼             ▼              ▼
        App A         App B          App C
```

## 2. 模块拆分

```text
license-system/
├── protocol/
├── core/
├── service/
├── sdk/
├── tools/
├── examples/
├── tests/
└── server/
```

### protocol

负责：

- 消息定义
- 帧定义
- 命令号
- 错误码
- 协议版本
- 序列化格式

禁止包含业务逻辑。

### core

负责：

- License 验证
- Machine Identity
- Session
- Feature
- Crypto
- Storage

尽量不依赖 UI 与具体 IPC。

### service

负责：

- IPC Server
- Connection
- Dispatcher
- Handler
- 调用 Core
- 返回 Response / Event

### sdk

负责：

- 连接服务
- 协议封装
- Session
- Heartbeat
- Feature API
- 自动重连

### tools

建议包含：

```text
tools/
├── license-generator/
├── machine-id/
├── protocol-debugger/
└── license-inspector/
```

### examples

用于验证不同技术栈：

```text
examples/
├── cpp-test-app/
├── rust-test-app/
├── csharp-test-app/
└── python-test-app/
```

## 3. 依赖方向

推荐：

```text
Protocol
   ▲
   │
Core
   ▲
   │
Service
```

以及：

```text
Protocol
   ▲
   │
SDK
   ▲
   │
Application
```

禁止形成：

```text
Core → Qt
Protocol → Windows API
SDK → Service内部实现
```

## 4. 运行时流程

```text
Application
    │
    │ initialize
    ▼
License SDK
    │
    │ IPC connect
    ▼
License Service
    │
    ├── Challenge
    ├── Verify License
    ├── Verify Machine
    ├── Verify Product
    ├── Verify Expire
    └── Create Session
    │
    ▼
License SDK
    │
    ├── Session
    ├── Feature Mask
    └── Heartbeat
    │
    ▼
Application
```

## 5. 控制策略

授权失效后优先：

1. 通知 Application
2. 禁用授权功能
3. 停止采集/设备
4. 保存数据
5. Flush 日志
6. 正常退出

必要时由独立进程监控做强制终止兜底，但不应将强杀进程作为正常授权控制方式。
