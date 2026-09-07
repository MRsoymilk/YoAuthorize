# 推荐源代码目录

```text
license-system/
├── README.md
├── docs/
├── protocol/
│   ├── proto/
│   │   ├── common.proto
│   │   ├── auth.proto
│   │   ├── session.proto
│   │   ├── heartbeat.proto
│   │   ├── feature.proto
│   │   └── license.proto
│   └── frame/
├── core/
│   ├── license/
│   ├── machine/
│   ├── crypto/
│   ├── session/
│   ├── feature/
│   └── storage/
├── service/
│   ├── ipc/
│   │   ├── named_pipe/
│   │   ├── unix_socket/
│   │   └── tcp/
│   ├── dispatcher/
│   ├── handlers/
│   └── config/
├── sdk/
│   ├── core/
│   ├── c/
│   ├── cpp/
│   ├── rust/
│   ├── csharp/
│   └── python/
├── tools/
│   ├── license-generator/
│   ├── machine-id/
│   ├── protocol-debugger/
│   └── license-inspector/
├── examples/
│   ├── cpp-test-app/
│   ├── rust-test-app/
│   ├── csharp-test-app/
│   └── python-test-app/
├── tests/
└── server/
```
