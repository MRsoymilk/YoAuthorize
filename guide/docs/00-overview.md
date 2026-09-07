# 00. 项目概述

## 1. 项目目标

设计一套独立的软件授权系统，对一个或多个目标应用程序实施授权控制，同时尽可能降低业务程序的接入侵入性。

目标包括：

- 本地 License 校验
- License 数字签名
- 机器绑定
- 有效期控制
- 功能级授权
- 多实例限制
- Session 管理
- Heartbeat
- 授权状态动态变化
- 授权吊销
- 离线授权
- 后续在线激活
- 多语言 SDK
- 多平台 IPC

## 2. 核心设计原则

### 2.1 协议优先

业务程序只依赖统一的 License Protocol 与 SDK，不依赖具体 IPC 技术。

```text
Application
    ↓
License SDK
    ↓
License Protocol
    ↓
Transport
```

### 2.2 Transport 与 Protocol 分离

Transport 负责“怎么传”：

- Windows Named Pipe
- Unix Domain Socket
- TCP Loopback

Protocol 负责“传什么”：

- Hello
- Challenge
- Authenticate
- Session
- Heartbeat
- Feature
- License State
- Error

### 2.3 授权结果不是简单 bool

不推荐：

```cpp
if (!licenseCheck())
    exit(0);
```

推荐由授权服务创建持续存在的 Session，并返回：

- Session ID
- Session Key
- Feature Mask
- Expire Time
- Server Nonce
- Signature / MAC

授权信息参与后续业务运行。

### 2.4 业务低侵入

业务侧最好只需要：

```cpp
LicenseClient client;
if (!client.initialize("PRODUCT_A"))
    return -1;
```

功能授权：

```cpp
if (client.hasFeature(FEATURE_FFT)) {
    enableFFT();
}
```

## 3. 系统角色

```text
LicenseGenerator
    │
    └── 生成并签名 License

LicenseService
    │
    ├── 验证 License
    ├── 管理 Session
    ├── 处理 Heartbeat
    ├── 管理 Feature
    └── 处理 IPC

LicenseSDK
    │
    └── 为应用提供统一 API

Application
    │
    └── 业务逻辑

LicenseServer（后续）
    │
    ├── 激活
    ├── 设备管理
    ├── 吊销
    └── 在线同步
```

## 4. 推荐技术路线

第一阶段推荐：

- Protocol Buffers
- Rust LicenseService
- Windows Named Pipe
- Linux/macOS Unix Domain Socket
- Ed25519
- Machine ID
- Session + Heartbeat
- C ABI
- C++ TestApp

Qt 可以继续用于 TestApp，但不进入授权系统核心依赖。
