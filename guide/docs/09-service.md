# 09. License Service

License Service 是 C++20 本地守护进程，组合 IPC、Protocol 和 Core。它不提供 UI，
也不持有 License 签发私钥。

## 1. 内部边界

```text
TransportServer -> Connection -> FrameCodec -> ProtocolDispatcher
                                             -> AuthHandler
                                             -> LicenseManager
                                             -> SessionManager
                                             -> FeatureManager

MachineIdentity -> LicenseManager
KeyStore        -> AuthHandler / LicenseManager
StateStorage    -> LicenseManager
```

- Connection 只维护 Transport、握手和每方向加密状态。
- Dispatcher 只接受当前连接状态允许的消息。
- Core Manager 不依赖 Transport 类型。
- Session 创建与 `max_sessions` 检查必须原子化。

## 2. 启动与关闭

启动顺序：

```text
Load Config -> Load Trust Keys -> Load Protected State
-> Load/Verify License -> Collect Machine Identity
-> Bind IPC Endpoint -> Accept Connections
```

任何信任密钥、状态或 License 解析失败都不得以 Unbound/Valid 状态继续。关闭时停止
Accept，通知连接，停止 Session，原子刷新状态，然后关闭端点。

## 3. 文件与权限

默认系统级位置：

| Data | Windows | Linux |
|---|---|---|
| Config/keys | `%ProgramData%\YoAuthorize\` | `/etc/yoauthorize/` |
| License/state | `%ProgramData%\YoAuthorize\data\` | `/var/lib/yoauthorize/` |
| Runtime endpoint | Named Pipe | `/run/yoauthorize/` |

- 安装程序创建目录和 ACL；Service 不依赖当前工作目录。
- 私有状态使用临时文件、flush/fsync 和原子 rename/replace 写入。
- 写入使用进程锁，崩溃后可识别旧版本或损坏记录。
- License 目录不得允许普通客户端用户替换文件或创建符号链接。

## 4. 运行账户与对端身份

Service 使用最低必要权限的专用账户。平台 Transport 获取的 SID、UID/GID 和 PID
是对端身份来源；消息中的同名字段仅用于诊断。若产品要求校验可执行文件，必须在
获得进程句柄后校验签名/路径，并明确 TOCTOU 限制。

## 5. 配置与限制

必须可配置且有安全默认值：

- License、trust key 和 state 路径
- IPC endpoint 与允许的用户/组
- 最大连接、握手、Session、Frame 和队列数量
- connect/read/write/idle deadline
- Heartbeat、timeout、Grace 参数的允许范围
- 日志级别和审计输出

测试用 TCP 和调试绕过项必须由构建或独立配置显式开启，发布配置默认不存在。

## 6. License 更新

LicenseManager 对候选文件完整验证后才原子切换。切换与 Session 重算应在同一逻辑
事务中产生有序事件。无效候选保留当前有效 License，并记录不包含敏感数据的错误。
