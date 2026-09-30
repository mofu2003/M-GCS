# M-GCS

M-GCS 是一款面向 ArduPilot / PX4 的跨平台地面站工程，当前由 C++ 后端、C FFI
契约和 Flutter 外壳组成。

当前版本：`0.0.18`

## 仓库范围

```text
core/                 C++17 后端：连接、MAVLink、飞控、任务、参数、校准、遥测、日志
bridge/ffi/           稳定 C ABI 与 JSON 契约
flutter_shell/        Flutter 地面站外壳
core/tests/           offline / simulation 后端测试
tests/                跨模块、硬件只读和手动测试
tools/                MAVLink 生成、预检、发布和硬件脚本
docs/                 当前文档、任务规划契约、ADR 和审计
third_party/          MAVLink 定义、cJSON 等第三方源码
```

旧的完整 Flutter 应用不在当前产品树中；当前外壳是 `flutter_shell/`。

## 当前能力

- macOS / Windows / Linux 的 C++ 后端与 FFI 构建路径。
- 串口、TCP、UDP、内存链路和 session ID 连接模型。
- ArduPilot-first、PX4-compatible 的 heartbeat、遥测、参数、校准、日志和命令。
- 任务文档编辑、undo/redo、命令目录、参数化测绘、上传、下载、验证，以及 `.waypoints` / QGC `.plan` 互操作。
- Flutter 地图、Home/飞机标记、引导操作、任务规划面板、配置和校准页面。
- 统一 MiSans 字体、macOS 风格组件和设计 token 检查。

## 快速验证

第一次构建先生成 MAVLink 头：

```bash
./tools/generate-mavlink.sh
```

默认本地门禁：

```bash
tools/preflight.sh
```

等价的后端测试：

```bash
cmake -S . -B build_check -DMGCORE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_check --config Release
ctest --test-dir build_check --output-on-failure --no-tests=error \
  -L "offline|simulation" -LE "hardware|manual"
```

Flutter：

```bash
cd flutter_shell
flutter analyze lib test
flutter test
cd ..
dart tools/check_ui_tokens.dart
```

macOS 调试运行：

```bash
cd flutter_shell
flutter run -d macos
```

## FFI 使用

外部前端只依赖：

```text
bridge/ffi/ffi_api.h
```

所有 `char*` 和事件回调 JSON 都由调用方调用 `mg_free_string()` 释放。连接优先使用
`mg_connection_open()` 返回的 session ID；旧单机接口保留作兼容。完整分组和错误语义见
[`bridge/ffi/API_CATALOG.md`](bridge/ffi/API_CATALOG.md)。

## 安全

真实飞控测试前必须确认飞控未解锁、机体安全、串口没有被其他地面站占用。未经明确授权，
不执行解锁、起飞、降落、任务启动、切模式、电机测试、写参数或危险校准。

## 文档

从 [`docs/README.md`](docs/README.md) 开始。任务规划的当前契约是
[`docs/MISSION_PLANNING.md`](docs/MISSION_PLANNING.md)，当前审计是
[`docs/STATUS_AUDIT.md`](docs/STATUS_AUDIT.md)。

## 发布

```bash
tools/release.sh 0.0.13
```

脚本更新版本文件，不提交、不打标签、不访问远端。push 需要用户明确要求。

## 许可证

[MIT](LICENSE)
