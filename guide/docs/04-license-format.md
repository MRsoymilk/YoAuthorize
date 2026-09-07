# 04. License 文件格式

## 1. 目标

License 文件用于描述：

- 产品
- 客户
- 机器绑定
- 有效期
- 功能权限
- 最大 Session
- License 类型

License 必须可验证完整性与真实性。

## 2. 逻辑 Payload

示例：

```json
{
  "version": 1,
  "license_id": "LIC-2026-000001",
  "product_id": "PRODUCT_A",
  "customer": "Demo",
  "machine_policy": {
    "mode": "bound",
    "machine_id": "..."
  },
  "issue_time": 1788700000,
  "not_before": 1788700000,
  "expire_time": 1820236000,
  "features": [
    "basic",
    "fft",
    "export"
  ],
  "max_sessions": 1
}
```

## 3. License 类型

建议：

```text
Trial
Basic
Professional
Enterprise
Permanent
Subscription
```

实际权限不要只由名称决定，应转化为 Feature Set。

## 4. Feature

例如：

```text
basic
capture
save
fft
filter
export
advanced
remote
```

SDK 内可以映射为 bitmask。

## 5. 签名结构

推荐：

```text
LicensePackage
├── payload
├── key_id
├── signature_algorithm
└── signature
```

算法推荐：

> Ed25519

## 6. 密钥原则

LicenseGenerator / LicenseServer：

```text
Private Key
```

LicenseService：

```text
Public Key
```

客户端不得保存签发私钥。

## 7. 文件格式

可以使用：

- Protobuf binary
- CBOR
- JSON + detached signature

正式版更推荐 Protobuf/CBOR。

## 8. 版本兼容

必须包含：

```text
version
```

新增字段应保证旧版本可忽略。

## 9. License 修改检测

任何 Payload 修改都会导致：

```text
Signature Verify Failed
```

因此不依赖：

- MD5
- CRC
- 自定义混淆

实现防篡改。

## 10. License 热更新

Service 应能够：

```text
Reload License
    ↓
Verify
    ↓
Re-evaluate Session
    ↓
Push LicenseChanged / Revoked
```
