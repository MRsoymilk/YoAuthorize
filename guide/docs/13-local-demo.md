# 13. Local Demo

本地 Demo 会在临时目录生成两组 Ed25519 密钥、签名 License 和 Service TOML 配置，
然后启动真实 `yoauthorize-service`，通过 C++ TestApp 完成身份认证、授权、Heartbeat
和 Session 关闭。临时私钥和 License 会在退出时删除。

## 构建

```bash
cmake -S . -B build \
  -DBUILD_TESTING=ON \
  -DYOAUTHORIZE_BUILD_SERVICE=ON \
  -DYOAUTHORIZE_BUILD_SDK=ON \
  -DYOAUTHORIZE_BUILD_TOOLS=ON
cmake --build build
```

## 运行

```bash
bash tools/run_local_demo.sh build 3
```

成功时 TestApp 输出：

```text
authorization valid
feature: capture
feature: export
```

同一链路也注册为 CTest：

```bash
ctest --test-dir build -R LocalDemoTest --output-on-failure
```

该脚本只用于本地开发和集成测试。生产环境必须由安装程序创建专用账户、目录、密钥
和权限，不应使用临时 Demo 密钥。
