# 08. License SDK 设计

## 1. 目标

SDK 是业务程序与授权系统之间的唯一推荐接口。

业务程序不直接处理：

- IPC
- Protobuf
- Challenge
- Session
- Heartbeat
- Reconnect

## 2. SDK Core API

逻辑接口：

```text
initialize(product_id)
connect()
authorize()
is_valid()
has_feature(feature)
get_license_info()
get_session_info()
shutdown()
```

## 3. C ABI

为了支持多语言，推荐提供稳定 C ABI。

```c
typedef void* license_handle_t;

int license_create(
    const char* product_id,
    license_handle_t* handle
);

int license_connect(
    license_handle_t handle
);

int license_authorize(
    license_handle_t handle
);

int license_is_valid(
    license_handle_t handle
);

int license_has_feature(
    license_handle_t handle,
    uint64_t feature
);

void license_destroy(
    license_handle_t handle
);
```

## 4. SDK Wrapper

```text
SDK Core
    │
   C ABI
    │
 ┌──┼───────────────┐
 │  │               │
C++ C#            Python
```

可扩展：

- Rust native API
- Java JNI
- Go cgo

## 5. C++ Wrapper

例如：

```cpp
class LicenseClient {
public:
    bool initialize(std::string_view productId);
    bool isValid() const;
    bool hasFeature(uint64_t feature) const;
    LicenseInfo licenseInfo() const;
    void shutdown();
};
```

## 6. Event

SDK 应支持事件通知：

```text
onLicenseChanged
onFeatureChanged
onLicenseExpired
onLicenseRevoked
onServiceDisconnected
onServiceRecovered
```

## 7. Heartbeat

Heartbeat 应由 SDK 内部线程或 async task 维护，不要求业务程序手动定时调用。

## 8. 自动重连

SDK 负责：

- Transport 重连
- 重新握手
- Session 恢复/重建
- Grace Period 状态

## 9. 业务接入

业务程序：

```cpp
LicenseClient license;

if (!license.initialize("PRODUCT_A"))
    return -1;
```

功能：

```cpp
if (license.hasFeature(FEATURE_EXPORT)) {
    exportData();
}
```

## 10. Qt 的定位

如果业务程序是 Qt：

```text
Qt UI
  ↓
C++ License SDK
  ↓
C ABI / SDK Core
```

Qt 不进入授权系统核心。

## 11. SDK 不应承担

SDK 不应该：

- 保存签发私钥
- 决定完整 License 策略
- 成为唯一授权判断点
