# 03. IPC Transport

Transport 提供可靠、有序的双向字节流。它不解析 FlatBuffers，不判断 License，
也不自行增加加密层。

## 1. 统一接口语义

```cpp
class ITransport {
 public:
  virtual Status connect(Deadline deadline) = 0;
  virtual Status readExact(std::span<std::byte> out,
                           Deadline deadline) = 0;
  virtual Status writeAll(std::span<const std::byte> data,
                          Deadline deadline) = 0;
  virtual void close() noexcept = 0;
  virtual ~ITransport() = default;
};
```

Frame 层先读取固定 20 字节 Header，校验长度后再精确读取 Payload。实现必须正确
处理 partial read/write、多个 Frame 合并到一次读取、EOF、取消和超时。

## 2. 平台端点

### Windows

- 默认 Named Pipe：`\\.\pipe\YoAuthorize\license-v1`。
- 使用 byte mode，与其他 Transport 保持相同流语义。
- ACL 只授予运行 Service 的账户、管理员和配置的应用用户/组。
- 禁止 `Everyone` 写权限；Service 应读取并记录可信的客户端 token/SID。

### Linux

- 默认 Socket：`/run/yoauthorize/license-v1.sock`。
- Service 创建 `/run/yoauthorize`，目录建议 `0750`，Socket 建议 `0660`。
- owner/group 由安装配置确定；创建时使用限制性 umask。
- 使用 `SO_PEERCRED` 获取 PID/UID/GID，不信任客户端消息中的 PID。

### macOS

- 系统服务默认 Socket：`/var/run/yoauthorize/license-v1.sock`。
- 使用 `getpeereid` 获取对端 UID/GID。
- macOS 在 Linux MVP 和 Windows Transport 稳定后补充。

### TCP Loopback

- 仅调试和测试允许，必须绑定 `127.0.0.1` 或 `::1`，不得绑定通配地址。
- 发布配置默认禁用，端口通过显式配置给出。
- 即使是 Loopback，也必须执行完整的 Service 身份认证和记录保护。

## 3. 生命周期

```text
Disconnected -> Connecting -> Handshaking -> SessionActive
      ^              |              |              |
      +--------------+--------------+--------------+
                    error / timeout
```

- connect、握手、读写和空闲状态分别设置 deadline。
- SDK 重连退避建议为 1s、2s、5s、10s，加入随机抖动并设置上限。
- 每次重连必须创建新的 ephemeral key、nonce 和 Sequence 空间。
- MVP 不跨 Service 重启恢复旧 Session，而是重新授权。

## 4. 资源限制

Service 必须配置以下上限并在达到上限时快速失败：

- Frame/Payload 大小
- 同时连接数、握手中连接数和 Session 数
- 每个连接的待发送队列与事件数量
- 握手频率、读取时间和空闲时间

慢客户端不得无限占用线程或内存。事件队列溢出时关闭该连接，让 SDK 重连并重新
查询完整状态，不静默丢弃授权事件。
