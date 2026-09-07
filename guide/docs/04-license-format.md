# 04. License 文件格式

## 1. 格式

License 文件使用 FlatBuffers 根类型 `LicensePackage`，文件标识为 `YALC`：

```text
LicensePackage
├── format_version: uint16
├── key_id: string
├── payload: [ubyte]       # 完整、原始的 LicensePayload FlatBuffer
└── signature: [ubyte]     # v1 固定 64 字节 Ed25519 签名
```

`LicensePayload` 包含：

```text
license_id, product_id, customer_id
issue_time, not_before, expire_time
license_type, features:[string], max_sessions
machine_policy
```

- 时间均为 UTC Unix seconds。
- Permanent License 使用 `expire_time = 0`；其他类型必须大于 `not_before`。
- `features` 使用稳定的 ASCII 标识符，例如 `capture`、`fft`、`export`，不使用
  受 64 位限制的 Wire bitmask。
- Feature ID 和 key ID 区分大小写，发布后不得改变既有标识的语义。
- 未知 Feature 保留但默认不授予当前应用能力。
- v1 文件最大 1 MiB，字符串和数组另设合理上限。

## 2. 签名

签名输入是以下字节的无歧义拼接：

```text
"YOAUTHORIZE-LICENSE-V1" ||
u16be(format_version) ||
u16be(key_id_length) || key_id_utf8 ||
u32be(payload_length) || payload
```

验证签名时直接使用文件中的原始 `payload`，不得解析后重新序列化。签名通过且
FlatBuffers Verifier 验证 `LicensePayload` 后，才能读取授权字段。

- License 签发私钥只存在于受控的 Generator/Server 环境。
- Service 只保存按用途标记为 `license-signing` 的公钥。
- v1 算法固定为 Ed25519，不接受文件自行声明其他算法。
- 未知、禁用或已吊销的 `key_id` 必须拒绝。

## 3. 验证顺序

```text
文件边界与 Package Verifier
-> format_version / key_id / signature 长度
-> Ed25519 signature
-> Payload Verifier
-> product_id
-> not_before / expire_time / rollback policy
-> revocation state（如可用）
-> machine_policy
-> features / max_sessions
```

所有失败都返回稳定错误码；日志不得输出完整 License、机器标识或密钥材料。

## 4. Machine Policy

MVP 支持：

- `Unbound`：不绑定机器。
- `Exact`：License 保存规范化组件计算出的 opaque Machine ID。

容错匹配属于后续版本，不能用单个组合 Hash 实现。若增加 `Tolerant` 模式，License
必须签署组件类别、组件 Hash、权重和阈值，不能由本地配置单方面放宽。

## 5. 原子更新

Service 监测到新文件时：

1. 从受限目录读取到内存，并拒绝符号链接和超限文件。
2. 完整验证候选 License，不覆盖当前有效状态。
3. 验证成功后原子切换，并重新评估所有 Session。
4. 按顺序发送 `LicenseChanged`、`FeatureChanged` 或终止事件。

无效或读取一半的替换文件不得使当前有效 License 失效。
