# License System Documentation

这是一套面向多技术栈的软件授权系统设计文档。

## 核心目标

- 授权逻辑与业务逻辑解耦
- 目标程序低侵入接入
- 不绑定 Qt、C++ 或某一种 IPC 技术
- 支持 Windows / Linux / macOS
- 支持 C / C++ / Rust / C# / Python 等客户端
- 支持离线授权、设备绑定、功能授权、Session、Heartbeat
- 后续可扩展在线激活、远程吊销与可信时间

## 推荐总体技术路线

- License Service：Rust（推荐，可替换为 C++ / Go / C#）
- Protocol：Protocol Buffers
- Windows IPC：Named Pipe
- Linux/macOS IPC：Unix Domain Socket
- Debug/Fallback：TCP 127.0.0.1
- License Signature：Ed25519
- Session Key Exchange：X25519
- Message Authentication：HMAC-SHA256 或 AEAD
- SDK Core：Rust 或 C++
- Cross-language ABI：C ABI
- Test App：不限技术栈，Qt 仅可作为示例 UI

## 文档目录

1. [00-overview.md](docs/00-overview.md)
2. [01-architecture.md](docs/01-architecture.md)
3. [02-protocol.md](docs/02-protocol.md)
4. [03-ipc.md](docs/03-ipc.md)
5. [04-license-format.md](docs/04-license-format.md)
6. [05-session.md](docs/05-session.md)
7. [06-machine-identity.md](docs/06-machine-identity.md)
8. [07-crypto.md](docs/07-crypto.md)
9. [08-sdk.md](docs/08-sdk.md)
10. [09-service.md](docs/09-service.md)
11. [10-security.md](docs/10-security.md)
12. [11-testing.md](docs/11-testing.md)
13. [12-roadmap.md](docs/12-roadmap.md)

## 推荐仓库结构

```text
license-system/
├── README.md
├── docs/
├── protocol/
├── core/
├── service/
├── sdk/
├── tools/
├── examples/
├── tests/
└── server/
```
