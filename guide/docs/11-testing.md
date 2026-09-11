# 11. 测试方案

本文件定义目标授权系统的验收范围，不是当前根目录 CMake 测试的运行说明。

## 1. 必需层级

- Unit：Core 策略、状态机、时间和错误映射。
- Protocol：Frame、FlatBuffers、版本兼容和 Golden Vectors。
- Crypto：标准向量、握手 transcript、密钥派生、AEAD 和签名。
- Transport：partial IO、超时、ACL/peer credentials 和资源限制。
- Integration：Generator -> Service -> SDK -> TestApp 完整链路。
- Failure/Security：崩溃、重放、Fake Service、畸形输入和时间回拨。
- Platform：Linux MVP；Windows 和 macOS 支持后加入同一兼容矩阵。

## 2. Golden Vectors

`test/vectors/` 应保存并版本化：

- 每种消息的合法 Frame 和 FlatBuffer Payload
- License 签名输入、合法/错误签名和 key rotation 样本
- Machine ID 规范化输入与结果
- X25519、HKDF、nonce、ChaCha20-Poly1305 和 transcript 样本
- 前一协议版本生成的数据

测试必须验证精确字节，不只验证解析后的字段相等。

## 3. Protocol 与输入安全

覆盖：

- partial/coalesced Frame、EOF 和 deadline
- 错误 Magic、版本、Flags、Sequence 和长度溢出
- Payload 超限、截断、错误 file identifier 和 Verifier 失败
- 未知消息、错误状态下的消息、重复 request ID
- 降级尝试、Tag 修改、重放、乱序和 Sequence 边界
- FlatBuffers Schema 向前/向后兼容

对 Frame Header、FlatBuffers Payload 和 LicensePackage 持续 Fuzz；任何输入都不得
导致越界、未受控分配、断言退出或泄露密码学错误细节。

## 4. License 与机器

- 缺失、损坏、超限、错误 key ID、错误签名和错误产品
- `not_before`、到期、Permanent 和时间回拨
- Exact Machine ID 匹配/不匹配及组件缺失
- Feature 增减、未知 Feature 和 `max_sessions` 边界
- 文件写到一半、符号链接替换、无效热更新和原子切换

## 5. Session 与恢复

- 正常授权、Heartbeat、主动关闭和超时清理
- 并发创建 Session 时不突破 `max_sessions`
- Service 崩溃后 SDK 进入 Grace、退避重连并重新授权
- Grace 到期 fail closed，恢复后状态和 Feature 原子更新
- 慢客户端、发送队列溢出和连接/握手上限

## 6. 身份与平台

- SDK 拒绝未知或错误 Service 身份签名。
- Named Pipe ACL 拒绝未授权 token；Unix Socket 验证 UID/GID/PID。
- TCP 调试模式只绑定 Loopback，发布配置无法意外启用。
- 所有支持平台读取相同 Golden Vectors 并产生相同协议结果。

## 7. MVP 验收

MVP 完成必须同时满足：

- 修改 License 任意签名字段后验证失败。
- 机器、产品、时间和 Feature 策略符合测试向量。
- Fake Service、Replay 和畸形 Frame 被拒绝。
- Service 重启后 SDK 可在 Grace 内重新授权；失败时安全降级。
- 并发 Session 限制没有竞态超发。
- Linux 集成测试和 Sanitizer 构建通过。
