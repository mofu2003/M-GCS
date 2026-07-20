# mgroundcontrol

`mgroundcontrol` 是面向 ArduPilot / PX4 生态的地面站项目，由 C++ 后端核心、C FFI 桥接层和 Flutter 跨平台前端组成。

当前可工作的主线：

- C++ 后端和 C FFI bridge 可在 macOS 上配置、编译并通过默认测试。
- Flutter 前端已恢复 macOS/Windows 桌面工程骨架，macOS 分析、测试和构建已通过。
- 真实飞控只读 dump 已验证：ArduPilot Copter 可连接，参数总数 1027，`SERVO_OUTPUT_RAW` / `RC_CHANNELS` 遥测请求已补齐。
- 长期目标平台按优先级为 Windows、Android、macOS、iOS；Windows 和 Android 是主力验收平台。
- 前端 UI 硬约束：Windows/macOS 尽可能同屏一致，最大程度复刻 macOS/iOS 通用风格，同时避免任何商业侵权风险。

## 目录

```text
mgroundcontrol/
|- app/                  Flutter 跨平台前端
|- bridge/ffi/           C FFI 契约与 MgCore 适配层
|- core/                 C++ 后端领域模型与 MAVLink 引擎
|- docs/                 长期开发文档、ADR 和历史归档
|- tests/                后端集成测试、真实硬件测试和手动测试
|- tools/                开发与硬件测试辅助脚本
|- third_party/          MAVLink、cJSON 等第三方依赖
|- prototypes/           实验原型预留目录
|- CMakeLists.txt        后端构建入口
\- .github/workflows/    CI 工作流
```

更详细的目录职责见 [项目结构](docs/PROJECT_LAYOUT.md)。

## 快速验证

后端：

```bash
/Users/fansicheng/Library/Python/3.9/bin/cmake -S . -B build_check -G Ninja \
  -DCMAKE_MAKE_PROGRAM=/Users/fansicheng/Library/Python/3.9/bin/ninja \
  -DMGCORE_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release

/Users/fansicheng/Library/Python/3.9/bin/cmake --build build_check

/Users/fansicheng/Library/Python/3.9/bin/ctest --test-dir build_check \
  --output-on-failure \
  -L "offline|simulation" \
  -LE "hardware|manual"
```

前端：

```bash
cd app
flutter pub get
flutter analyze
flutter test
flutter build macos
```

真实飞控只读检查：

```bash
build_check/test_scan_connect /dev/cu.usbmodem1101
build_check/test_real_fc_dump /dev/cu.usbmodem1101 --quick
```

真实硬件测试前必须确认飞控未解锁，并明确本次测试会触发哪些命令。默认不要保留硬件日志。

## 文档入口

- [文档索引](docs/README.md)
- [项目状态](docs/PROJECT_STATUS.md)
- [项目结构](docs/PROJECT_LAYOUT.md)
- [开发指南](docs/DEVELOPMENT.md)
- [架构说明](docs/ARCHITECTURE.md)
- [开发约束](docs/CONSTRAINTS.md)
- [中文与编码规则](docs/ENCODING.md)
- [ADR](docs/adr/)

## 当前边界

- Flutter Windows 目录已生成，但仍需在 Windows 机器上执行 `flutter build windows`。
- Android/iOS 是长期目标平台，当前需要重新启用平台工程并补齐权限、native library 打包和平台适配。
- `mgcore.dylib` / `mgcore.dll` 尚未正式打包进 Flutter 桌面应用。
- 固件刷写、视频接收/解码、Android filesystem 仍不是生产验收能力。
- 当前目录不是 git 仓库，不能使用 `git status`、`git diff` 或校验子模块状态。
- 工作区仍有 CRLF/BOM 历史遗留，归一化应单独处理。

## 许可证

MIT
