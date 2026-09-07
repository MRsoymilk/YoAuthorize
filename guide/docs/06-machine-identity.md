# 06. Machine Identity

Machine Identity 是授权策略，不是密码学身份。采集失败不得静默降级为 Unbound。

## 1. MVP 数据源

- Windows：MachineGuid、SMBIOS/System UUID、系统盘标识。
- Linux：`/etc/machine-id`、DMI product UUID、系统盘标识。
- macOS（后续）：IOPlatformUUID。

MAC 地址、IP、hostname 和 CPU 型号字符串变化频繁或易伪造，不作为独立绑定依据。

## 2. 规范化与 Hash

每个组件按类型独立处理：

1. 读取原始值并验证来源与权限。
2. trim，统一 ASCII 大小写，移除该类型规定的分隔符。
3. 空值、默认厂商值和读取错误标记为 Missing，不写入空字符串。
4. 计算 `SHA-256("YOAUTHORIZE-MACHINE-COMPONENT-V1" || type || length || value)`。

Exact Machine ID 对排序后的组件 Hash 做长度前缀拼接，再使用独立 domain separator
计算 SHA-256。输出编码固定为小写十六进制。

类型、排序、长度编码和缺失值策略必须形成测试向量，不能依赖容器遍历顺序或本地
区域设置。

## 3. 策略

- MVP 的 `Exact` 要求当前 Machine ID 与 License 中的 opaque ID 完全相同。
- 重装系统、换盘和主板维修通过重新签发或受控迁移处理。
- `Tolerant` 模式后续实现，必须基于 License 中已签名的组件 Hash、权重和阈值。
- VM 是否允许、是否绑定 VM UUID、是否允许克隆是产品级显式策略。

## 4. 隐私与日志

- 原始硬件标识只在采集和 Hash 期间存在，不写入普通日志。
- License Generator、诊断工具和 Service 展示 Machine ID 前应提示其稳定标识属性。
- 错误日志只记录组件类型和状态，不记录原始序列号。
