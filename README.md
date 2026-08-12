# M-GCS

M-GCS 是面向 ArduPilot / PX4 生态的跨平台地面站，由 C++ 后端核心、C FFI 桥接层和 Flutter 前端组成。

当前版本：`0.0.2`

## 当前状态

- C++17 后端和 C FFI bridge 包含 macOS、Windows、Linux 实现，并提供离线、模拟、硬件和手动测试分层。
- Flutter 已启用 macOS 和 Windows 桌面工程，业务 UI 通过应用契约调用 MgCore，不直接访问 MAVLink。
- 前端已接入真实串口连接、遥测、参数同步与参数读写；UDP、TCP 和蓝牙目前仍未接入 Dart 应用层。
- macOS 已完成真实 ArduPilot Copter 的只读连接验证；危险写操作不属于默认验收范围。
- 当前仓库未包含 MAVLink C 生成头；首次构建前需要按开发指南生成 `third_party/mavlink/include/mavlink/v2.0`。
- Android 和 iOS 尚未恢复平台工程。macOS/Windows 桌面工程会在构建时嵌入 native library，Windows 最终包仍待真实环境验证。

详细完成度和已知边界见 [项目状态](docs/PROJECT_STATUS.md)。

## 目录

```text
M-GCS/
|- app/                  Flutter 跨平台前端
|- bridge/ffi/           C ABI 与 MgCore 适配层
|- core/                 C++ 后端领域模型与 MAVLink 引擎
|- docs/                 长期开发文档和 ADR
|- tests/                集成测试、硬件测试和手动测试
|- tools/                开发与硬件测试辅助脚本
|- third_party/          MAVLink 定义、cJSON 等第三方源码
|- CMakeLists.txt        后端构建入口
\- .github/workflows/    CI 工作流
```

## 快速验证

使用 Docker（macOS、Windows、Linux 均可）：

```bash
./tools/docker-dev.sh check
```

Windows PowerShell：

```powershell
.\tools\docker-dev.ps1 check
```

Docker 负责统一后端编译、MAVLink 生成和 Flutter analyze/test；桌面发布包
仍需在对应原生系统构建。

后端：

先按 [开发指南](docs/DEVELOPMENT.md) 生成 MAVLink C 头文件，然后执行：

```bash
cmake -S . -B build_check -DMGCORE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_check
ctest --test-dir build_check --output-on-failure \
  -L "offline|simulation" -LE "hardware|manual"
```

前端：

```bash
source ~/.zprofile
cd app
flutter --no-version-check pub get
flutter --no-version-check analyze
flutter --no-version-check test
flutter --no-version-check build macos
```

真实硬件测试前必须确认飞控未解锁，并明确测试会发送的命令。未经确认不得执行解锁、起飞、切模式、写参数、任务启动、电机测试或危险校准。

## 文档

- [文档索引](docs/README.md)
- [项目状态](docs/PROJECT_STATUS.md)
- [项目结构](docs/PROJECT_LAYOUT.md)
- [开发指南](docs/DEVELOPMENT.md)
- [架构说明](docs/ARCHITECTURE.md)
- [开发约束](docs/CONSTRAINTS.md)
- [故障排查](docs/TROUBLESHOOTING.md)
- [更新日志](CHANGELOG.md)
- [ADR](docs/adr/)

## 许可证

[MIT](LICENSE)
