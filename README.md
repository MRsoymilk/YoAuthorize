## TODO

1. wrap spdlog
2. wrap tomlplusplus
3. wrap json
4. wrap nng

## Depencence

gflags

C++ 20

- nng
- gRpc ? brpc（RPC适合于内网调用，IO密集型服务）
- spdlog
- json
- gtest

### communicate

- 自动节点发现

使用`pub/sub`协议广播节点加入:

```cpp
void broadcast_presence();
```

实现外部注册服务（如Redis）存储节点列表

- 状态广播

使用`pub/sub`定期推送状态更新:

```cpp
class P2PNode {
    void subscribe_status(std::function<void(NodeStatus)> callback);
    void publish_status();
};
```


- 加密通信

使用NNG的TLS传输:

```cpp
nng_socket_set_string(socket, NNG_OPT_TLS_CONFIG, "tls+tcp://127.0.0.1:5555");
```

- 节点管理

维护动态节点列表，定期刷新

```cpp
void update_peers();
```

- 异步操作

使用 NNG 的 `nng_aio` 实现非阻塞查询

```cpp
void query_all_status_async(std::function<void(std::vector<NodeStatus>)> callback);
```

### config

- tomlplusplus

### driver

- [mysql-connector-cpp](https://github.com/mysql/mysql-connector-cpp)
- [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp)
- [mongo-cxx-driver](https://github.com/mongodb/mongo-cxx-driver)

git module add

```bash
git submodule add https://github.com/grpc/grpc.git vendor/grpc
git submodule add https://github.com/gabime/spdlog vendor/spdlog
git submodule add https://github.com/nlohmann/json vendor/json
git submodule add https://github.com/nanomsg/nng.git vendor/nng
git submodule add https://github.com/marzer/tomlplusplus.git vendor/tomlplusplus
```

switch version:

```bash
cd vendor/nng
git checkout v1.10.1

cd vendor/spdlog
git checkout v1.15.2

cd vendor/json
git checkout v3.12.0

cd vendor/tomlplusplus
git checkout v3.4.0

cd vendor/gtest
git chekcout v1.16.0
```

## Function

### auth

### connect

### disconnect

### heartbeat

## Architecture

```
- api
    - auth
    - basic
- log
- driver
  - mysql
  - mongodb
  - sqlite
utils
  - 
```

