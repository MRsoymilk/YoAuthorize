# 目标源代码结构

这是在现有仓库中逐步落地的目标结构，不是当前文件树清单。

```text
YoAuthorize/
├── CMakeLists.txt
├── schema/
│   ├── protocol.fbs
│   └── license.fbs
├── src/
│   ├── ya_license_core/       # License、机器、Session、密码学、存储
│   ├── ya_license_protocol/   # Frame、FlatBuffers 校验与生成代码适配
│   ├── ya_license_service/    # IPC、Dispatcher、Handler、服务入口
│   └── ya_license_sdk/        # 原生 C++ SDK
├── tools/
│   ├── license_generator/
│   ├── license_inspector/
│   └── machine_id/
├── test/
│   ├── unit/
│   ├── protocol/
│   ├── integration/
│   └── vectors/
└── guide/
```

## 边界

- `ya_license_protocol` 只处理 Frame、Schema 和序列化，不包含授权策略。
- `ya_license_core` 不依赖 IPC、UI 或 Service 生命周期。
- `ya_license_service` 组合 Protocol、Core 和平台 Transport。
- `ya_license_sdk` 隐藏 IPC、FlatBuffers、握手和 Heartbeat。
- 工具可以依赖 Core 和 Protocol，生产库不得反向依赖工具。

## 生成代码

- `.fbs` 是协议源文件；生成代码不得手工修改。
- CMake 负责调用固定版本的 `flatc`。
- 发布 SDK 时应携带生成产物，使用者不需要安装 `flatc`。
- CI 必须检查 Schema 生成产物与 `.fbs` 一致。

## 后续语言 SDK

协议稳定后，在 `src/ya_license_sdk/c/` 增加稳定 C ABI；C#、Python、Rust
优先包装该 ABI，而不是各自重复实现握手和密码学。
