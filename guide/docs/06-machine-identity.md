# 06. Machine Identity

## 1. 目标

License 可选择绑定特定设备，但必须避免因单个硬件变化导致授权频繁失效。

## 2. 可选指纹来源

### Windows

- MachineGuid
- SMBIOS UUID
- 主板 UUID
- 系统盘序列号
- TPM Identity（如适用）
- CPU 信息

### Linux

- /etc/machine-id
- DMI product UUID
- 主板信息
- 系统盘标识
- TPM（如适用）

### macOS

- Platform UUID
- Hardware UUID

## 3. 不推荐单独使用

- MAC Address
- CPU 型号字符串
- hostname
- IP 地址

这些信息可变或容易伪造。

## 4. 标准化

所有字段必须：

- trim
- uppercase/lowercase 统一
- 去除无意义分隔符
- 处理缺失值

然后再 Hash。

## 5. 基础 Machine ID

例如：

```text
SHA256(
    platform_uuid
    || machine_guid
    || disk_id
)
```

## 6. 容错匹配

推荐评分策略，而非所有字段必须完全一致。

示例：

```text
BIOS / Platform UUID    40%
Machine GUID            30%
Disk Serial             20%
CPU / Board             10%
```

阈值：

```text
>= 70%
```

认为同一设备。

## 7. 迁移策略

应设计：

- 重装系统
- 更换系统盘
- 主板维修
- VM 克隆
- 云主机变化

对应的：

- 手工解绑
- 管理员迁移
- 在线重新激活
- 有限次数换机

## 8. 虚拟机

需要明确产品政策：

- 是否允许 VM
- 是否允许克隆
- 是否绑定 VM UUID
- 是否要求在线校验

## 9. 安全定位

Machine Fingerprint 主要用于授权策略，不应被视为强密码学身份。
