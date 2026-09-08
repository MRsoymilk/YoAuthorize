# 00. 项目概述

## 1. 范围

YoAuthorize 是本地软件授权系统。独立的 License Service 验证 License，
维护授权 Session，并通过 SDK 向一个或多个应用提供功能权限。

首个可交付版本包括：

- 离线 License 加载与 Ed25519 验签
- 产品、有效期和机器绑定检查
- Feature Set 和最大并发 Session
- 本地 IPC、服务身份认证、Session 与 Heartbeat
- 原生 C++ SDK、License Generator 和测试应用

在线激活、远程吊销、可信时间、多语言 SDK 和客户端加固不属于首个 MVP。

## 2. 核心原则

### SDK 是应用边界

```text
Application -> C++ SDK -> Wire Protocol -> IPC -> License Service
```

应用不得直接解析 License、构造 FlatBuffers 消息或管理 Session Key。

### Protocol 与 Transport 分离

- Transport 只传输有边界的字节帧。
- Frame 负责长度、版本、序列号和记录保护。
- FlatBuffers Payload 表达请求、响应和事件。
- Core 决定 License、机器、Feature 和 Session 策略。

### 授权是持续状态

授权结果不是一个启动时计算后永久有效的 `bool`。SDK 持有 Session，内部维护
Heartbeat，并向应用暴露 `Valid`、`Grace`、`Expired`、`Revoked` 等状态。

### 明确安全边界

本地用户可能控制应用进程和机器。系统目标是提高伪造、重放和篡改成本，而不是
承诺客户端不可破解。序列化格式不提供安全性，安全属性来自签名、认证密钥交换、
AEAD、操作系统 IPC 权限和业务侧的多点 Feature 检查。

## 3. 系统角色

- License Generator：离线生成并签名 License，只在受控环境持有签发私钥。
- License Service：验证 License 和机器，管理 Session、Heartbeat 与 IPC。
- C++ SDK：连接 Service，执行握手，维护 Session，向应用提供稳定 API。
- Application：根据 SDK 状态和 Feature Set 控制业务能力。
- License Server：后续提供在线激活、吊销和可信时间，不属于 MVP。

## 4. 平台与语言

- 当前主实现语言：C++20。
- MVP 平台：Linux。
- 后续平台：Windows Named Pipe，然后是 macOS Unix Domain Socket。
- 后续语言 SDK：先提供稳定 C ABI，再提供语言包装。
- Qt 只能用于示例 UI，不进入 Core、Protocol 或 SDK Core。
