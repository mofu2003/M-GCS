# 开发指南

## 当前基线

- 当前版本：`0.0.56`。
- 代码分层：`flutter_shell -> bridge/ffi -> core -> third_party`。
- 默认测试只包含 `offline` 和 `simulation`；hardware/manual 必须显式运行。
- 默认预检：`tools/preflight.sh`。
- 当前已复核门禁：后端 `35/35`、Flutter `22/22`、UI token 和 macOS debug 构建通过。

判断“还有多少没做完”时，先看
[完成定义与后端维护收口](DEFINITION_OF_DONE.md) 和
[现状审计](STATUS_AUDIT.md) 的“当前剩余项”，不要凭感觉继续扩大范围。

## 上游实现策略

M-GCS 采用“MP 负责 ArduPilot 语义、QGC 负责通用框架、官方 MAVLink 负责协议”的组合策略：

- ArduPilot 的参数、机型差异、校准动作、飞行模式和现场兼容优先参考 Mission Planner。
- 任务对象模型、链路生命周期、超时/重试/取消、跨平台结构和测试治理优先参考 QGroundControl。
- MAVLink 消息、枚举、坐标单位和任务协议优先使用官方生成定义，不自行发明协议。
- 上游实现进入 M-GCS 前，必须改写到 `core -> bridge/ffi -> flutter_shell` 边界内，不能把
  QGC 的 Qt/QML/QObject 或 MP 的 WinForms/全局单例直接搬入项目。
- MP 与 QGC 行为冲突时，先回到官方协议和目标飞控行为，再记录选择依据；不能按项目整体站队。

详细规则见 [UPSTREAM_CODE_SELECTION.md](UPSTREAM_CODE_SELECTION.md) 和
[ADR 0009](adr/0009-hybrid-upstream-implementation-strategy.md)。

## 第一次构建

仓库不提交生成后的 MAVLink C 头。先生成：

```bash
./tools/generate-mavlink.sh
```

然后配置和构建：

```bash
cmake -S . -B build_check \
  -DMGCORE_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build_check --config Release
```

共享库构建：

```bash
cmake -S . -B build \
  -DMGCORE_BUILD_SHARED=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target mgcore
```

产物：macOS `build/mgcore.dylib`、Windows `build/mgcore.dll`、Linux `build/libmgcore.so`。

## 默认门禁

提交前必须运行：

```bash
tools/preflight.sh
```

它依次执行 `git diff --check`、UI token 检查、Flutter analyze/test、后端构建和
offline/simulation CTest；macOS 还会执行 `flutter build macos --debug`。

快速迭代可以缩短：

```bash
tools/preflight.sh --frontend-only
tools/preflight.sh --backend-only
tools/preflight.sh --no-macos-build
```

缩短模式不能替代提交前完整预检。

## 后端测试

```bash
ctest --test-dir build_check --output-on-failure --no-tests=error \
  -L "offline|simulation" -LE "hardware|manual"
```

标签：

- `offline`：不需要链路。
- `simulation`：内存链路或 `MockFlightController`。
- `hardware`：真实串口/飞控。
- `manual`：需要人工安全确认。

硬件测试前必须确认飞控未解锁、串口未被其他地面站占用，并说明将发送的命令。

## Flutter

```bash
cd flutter_shell
flutter pub get
flutter analyze lib test
flutter test
cd ..
dart tools/check_ui_tokens.dart
```

macOS 本地运行：

```bash
cd flutter_shell
flutter run -d macos
```

热重载使用运行窗口的 `r`；原生 `mgcore` 改动必须重启 app，Dart 热重载不会替换
已经加载的 dylib。

## Windows

PowerShell：

```powershell
.\tools\generate-mavlink.ps1
cmake -S . -B build_check -DMGCORE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_check --config Release --parallel
.\tools\preflight-windows.ps1

cd flutter_shell
flutter pub get
flutter build windows --debug
flutter run -d windows
```

运行时库是可执行文件旁的 `mgcore.dll`；连接 URI 例如：
`serial://COM7?baud=115200`。Windows 串口是排他资源。

## Docker/Linux

```bash
./tools/docker-dev.sh check
./tools/docker-dev.sh shell
./tools/docker-dev.sh configure
./tools/docker-dev.sh build
./tools/docker-dev.sh test
```

容器验证不能替代 Windows/macOS 原生构建。

## 版本发布

```bash
tools/release.sh <x.y.z>
```

脚本只更新 CMake、pubspec、README、CHANGELOG；随后补 CHANGELOG、跑完整预检、本地
提交和打标签。只有用户明确要求时才 push。

## 变更规则

- 实现前先查 QGC、MP 和现有成熟依赖，并按 `docs/UPSTREAM_CODE_SELECTION.md` 比较
  模块质量；能引用就引用，不能直接引用就按质量更高的实现和 ArduPilot 真实语义
  落地，不重复造轮子，也不按项目整体站队。
- FFI 契约变更必须同时改头文件、实现、测试、API Catalog、文档和前端绑定。
- 任务/参数/校准/连接/命令变更必须有聚焦 C++ 测试。
- UI 只使用 `MgTypography`、`MgColors`、`MgMetrics` 和现有 macos_ui 组件。
- 不提交 build 目录、生成 MAVLink 头、硬件 dump 或临时日志。
