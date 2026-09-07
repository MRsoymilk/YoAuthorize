# 10. Threat Model 与安全设计

## 1. 安全目标

本系统目标是：

> 提高绕过授权的成本。

不是：

> 声称客户端软件绝对不可破解。

用户完全控制本地机器时，理论上仍可能：

- Debug
- Patch
- Hook
- DLL 注入
- 内存修改
- Fake Service
- 二进制重写

## 2. 威胁：修改 License

攻击：

```text
修改 expire_time / features
```

防护：

```text
Ed25519 Signature
```

## 3. 威胁：复制 License

攻击：

```text
复制到另一台机器
```

防护：

```text
Machine Binding
```

## 4. 威胁：Fake LicenseService

攻击：

```text
自己实现 IPC Server
永远返回 Valid
```

防护：

- Server Authentication
- Challenge-Response
- 服务端签名
- Session Key

## 5. 威胁：IPC Replay

攻击：

```text
录制一次合法 Heartbeat/Auth
重复发送
```

防护：

- Session
- Sequence
- Nonce
- MAC
- Timestamp（辅助）

## 6. 威胁：Patch 单个 bool

攻击：

```cpp
if (!licensed) exit();
```

改成永远通过。

防护：

- 多点 Feature 检查
- Session 长期参与
- 授权数据参与关键资源解锁
- 不使用唯一单点布尔判断

## 7. 威胁：系统时间回拨

防护：

- last_valid_time
- server trusted time
- rollback detection
- 在线同步

## 8. 威胁：多实例

防护：

```text
max_sessions
```

并绑定：

- PID
- process start time
- client identity

## 9. 威胁：Service 被终止

防护不是“立即杀业务程序”，而是：

```text
Heartbeat timeout
→ Reconnect
→ Grace Period
→ Safe Shutdown
```

## 10. 威胁：SDK 被 Hook

不能完全避免。

可提高成本：

- 关键校验分散
- 完整性检查
- 关键资源加密
- Session 派生数据参与算法
- 发布版符号控制
- 代码混淆（后期）

## 11. 客户端加固属于第二层

授权协议先保证正确。

后续再考虑：

- Anti-Debug
- Anti-Hook
- Obfuscation
- Integrity Check
- VM Protection
- Hardware Dongle

这些不能替代密码学与协议设计。
