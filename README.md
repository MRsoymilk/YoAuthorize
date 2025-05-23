# Under Development

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

git module add

```bash
git submodule add https://github.com/gabime/spdlog vendor/spdlog
git submodule add https://github.com/nanomsg/nng.git vendor/nng
git submodule add https://github.com/marzer/tomlplusplus.git vendor/tomlplusplus
git submodule add https://github.com/google/googletest.git vendor/gtest
git submodule add https://github.com/apache/brpc.git vendor/brpc
git submodule add https://github.com/simdjson/simdjson.git vendor/simdjson
```

switch version:

```bash
cd vendor/nng
git checkout v1.10.1

cd vendor/spdlog
git checkout v1.15.2

cd vendor/tomlplusplus
git checkout v3.4.0

cd vendor/gtest
git chekcout v1.16.0
```

### config

- [tomlplusplus](https://github.com/marzer/tomlplusplus)

### driver

- [sqlite](https://www.sqlite.org/download.html)
- [mysql-connector-cpp](https://github.com/mysql/mysql-connector-cpp)
- [mongo-cxx-driver](https://github.com/mongodb/mongo-cxx-driver)
- [libpqxx](https://github.com/jtv/libpqxx)


#### mysql-connector-cpp

```bash
cd vendor/mysql-connector-cpp
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=./ ../ # default is Debug
make -j7
make install
```

#### mongo-cxx-driver

```bash
cd vendor/mongo-cxx-driver
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=./ ../ -DCMAKE_BUILD_TYPE=Debug
make -j7
make install
```

## Function

### auth

### connect

### disconnect

### heartbeat

## Architecture

```markdown
- api
  - auth
  - basic
- log
- driver
  - mysql
  - mongodb
  - sqlite
- utils
  - exe
  - timer
  - platform
- communicate
  - arch
    - single
    - p2p
  - module
    - http
    - server
    - client
```

