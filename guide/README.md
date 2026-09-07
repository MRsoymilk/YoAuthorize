# YoAuthorize 设计文档

`guide/` 描述 YoAuthorize 的目标架构，不代表仓库当前已经实现这些能力。
当前根目录仍是 C++/CMake 原型；实施状态以代码、CMake 和测试为准。

## 已确定的技术基线

- Core、License Service 和首个 SDK 使用 C++20。
- Wire Protocol 使用 FlatBuffers，协议与 IPC Transport 分离。
- Windows 使用 Named Pipe，Linux/macOS 使用 Unix Domain Socket。
- TCP Loopback 仅用于调试和测试，发布配置默认禁用。
- License 使用 Ed25519 签名。
- Session 使用 X25519、HKDF-SHA256 和 ChaCha20-Poly1305。
- C++ 密码学实现统一使用 OpenSSL 3.x 的高层 EVP API。
- MVP 先提供原生 C++ SDK；协议稳定后再提供 C ABI 和其他语言包装。

## 设计目标

- 授权逻辑与业务逻辑解耦，应用只依赖 SDK。
- 支持离线 License、机器绑定、功能授权、Session 和 Heartbeat。
- 协议、密码学和 IPC 不依赖 Qt 或其他 UI 框架。
- 最终支持 Windows、Linux 和 macOS；首个 MVP 优先 Windows/Linux。
- 后续扩展在线激活、吊销、可信时间和多语言 SDK。

## 阅读顺序

1. [项目概述](docs/00-overview.md)
2. [系统架构](docs/01-architecture.md)
3. [Wire Protocol](docs/02-protocol.md)
4. [IPC Transport](docs/03-ipc.md)
5. [License 格式](docs/04-license-format.md)
6. [Session 与 Heartbeat](docs/05-session.md)
7. [Machine Identity](docs/06-machine-identity.md)
8. [密码学设计](docs/07-crypto.md)
9. [C++ SDK](docs/08-sdk.md)
10. [License Service](docs/09-service.md)
11. [威胁模型](docs/10-security.md)
12. [测试方案](docs/11-testing.md)
13. [开发路线图](docs/12-roadmap.md)

目标目录见 [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md)。
