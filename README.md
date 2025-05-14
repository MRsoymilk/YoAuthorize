## Depencence

gflags

C++ 20

- brpc（RPC适合于内网调用，IO密集型服务）
- gflags
- gtest
- nng
- simdjson
- spdlog
- tomlplusplus

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

