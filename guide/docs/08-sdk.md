# 08. C++ SDK

MVP 只提供原生 C++ SDK。SDK 是应用访问授权系统的唯一公开边界，应用不直接接触
IPC、FlatBuffers、握手、Session Key 或 Heartbeat。

## 1. API 语义

```cpp
class LicenseClient {
 public:
  Result<void> initialize(const ClientConfig& config);
  LicenseSnapshot snapshot() const;
  bool hasFeature(std::string_view feature) const;
  Subscription subscribe(EventCallback callback);
  void shutdown() noexcept;
};
```

- `initialize()` 完成 IPC 连接、Service 身份认证、授权和 Heartbeat 启动；不再公开
  要求调用者按顺序调用 `connect()`、`authorize()`。
- `initialize()` 同步等待首次授权成功或 deadline 到期；异步接入由应用在线程或
  后续 async wrapper 中完成。
- 所有失败返回稳定状态码和可记录的非敏感消息，不能只返回 `bool`。
- `snapshot()` 返回同一时刻的状态、Feature、License 信息和 Session 信息副本。
- `hasFeature()` 使用稳定 ASCII Feature ID，并从同一原子快照读取。
- 重复调用 `shutdown()` 安全；析构函数会停止后台任务，但显式关闭便于控制时序。

## 2. 状态

```text
Uninitialized, Connecting, Valid, Grace, Expired, Revoked, Error, Closed
```

- `Valid` 才允许使用受控功能。
- `Grace` 是否暂时保留既有功能由 `ClientConfig` 的产品策略决定；默认 fail closed。
- `Expired`、`Revoked` 和 Grace 超时必须原子清空 Feature Set。
- SDK 不缓存超出当前 Session 生命周期的“最后一次有效”结果。

## 3. 线程与回调

- SDK 内部线程负责读取、Heartbeat 和重连；公开查询 API 可并发调用。
- 状态先更新，再在专用串行回调线程通知应用。
- 回调不得持有 SDK 内部锁；允许回调读取快照，但不得在回调中阻塞等待关闭。
- `Subscription` 控制注册生命周期；取消后不再开始新的回调。
- `shutdown()` 等待后台任务退出，并定义从其他线程调用时的确定行为。

## 4. 配置与错误

`ClientConfig` 至少包含 product ID、端点覆盖、连接超时、Grace 策略和日志回调。
发布构建不得允许应用绕过 Service 身份验证或切换到未认证协议。

错误分为：

- 配置/协议错误：不自动重试。
- 暂时 Transport 错误：进入 Grace 并退避重试。
- License/机器/产品错误：不自动循环重试，等待 LicenseChanged 或显式重载。
- Service 身份错误：立即 fail closed，并以高优先级记录。

## 5. 后续语言 SDK

Wire Protocol 稳定后增加 C ABI，必须定义 ABI version、调用约定、导出宏、结构体
size 字段、字符串/内存所有权、错误获取、回调线程和 handle 生命周期。C#、Python
和 Rust 优先包装 C ABI，不复制 C++ SDK 内的密码学状态机。

Qt、.NET 和 Python 类型不得进入 SDK Core 的公开数据模型。
