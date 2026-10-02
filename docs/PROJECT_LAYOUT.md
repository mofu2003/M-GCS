# 项目结构

当前版本基线：`0.0.67`。本文件只描述代码组织，具体接口和状态以
[`ARCHITECTURE.md`](ARCHITECTURE.md) 与 [`STATUS_AUDIT.md`](STATUS_AUDIT.md) 为准。

## 顶层目录

```text
M-GCS/
|- core/                 C++17 后端核心和测试
|- bridge/ffi/           对外 C ABI、JSON 适配和 API Catalog
|- flutter_shell/        Flutter/macOS/Windows 外壳
|- tests/                跨模块、硬件只读和手动测试
|- tools/                MAVLink 生成、预检、发布和硬件脚本
|- docker/               Linux 开发容器入口
|- docs/                 当前文档、ADR、审计快照和研究资料
|- third_party/          MAVLink 定义、cJSON 等第三方源码
|- CMakeLists.txt        后端构建入口
|- .github/workflows/    自动构建和测试工作流
```

## Core

- `core/src/transport/`：串口、TCP、UDP、内存链路、端口发现和连接看门狗。
- `core/src/mavlink/`：MAVLink adapter、内部状态容器、消息处理器、命令队列、参数缓存
  和任务传输；`mavlink_adapter_state.h`、`mavlink_adapter_handlers.cc` 和
  `mavlink_adapter_mission.cc` 等协议文件已从 `mavlink_adapter_internal.h` 分离。
- `core/src/vehicle/`：单飞控会话模型与多会话管理。
- `core/src/mission/`：任务文档、命令目录、任务编译、survey 生成和旧任务兼容路径。
- `core/src/calibration/`：陀螺仪、加速度计、磁力计、气压计和遥控器校准。
- `core/src/firmware/`：ArduPilot/PX4 profile、飞行模式和参数映射。
- `core/src/telemetry/`：遥测缓存、波形缓冲和 freshness。
- `core/src/settings/`、`log/`、`ftp/`：设置、飞行日志、MAVLink 日志和文件传输。
- `core/src/geofence/`、`terrain/`、`camera/`、`video/`、`sitl/`：围栏、地形、载荷、视频和仿真。
- `core/tests/`：offline/simulation 测试；默认门禁不运行 hardware/manual。

## Bridge / FFI

`bridge/ffi/ffi_api.h` 是公开 C ABI 的 umbrella header；具体接口放在对应的
`ffi_api_*.h`，实现按领域拆分为 `ffi_*.cc`。所有 `char*` 和事件 JSON 都由调用方
通过 `mg_free_string()` 释放。

`bridge/ffi/` 仍有少量兼容转发头，但 core 不得反向 include bridge；架构守卫会检查这一点。

## Flutter

- `flutter_shell/lib/connection/`：Dart FFI、连接控制器、遥测/参数/校准/飞行命令后端适配。
- `flutter_shell/lib/mission/mission_document_ffi_backend.dart`：任务文档 FFI 适配器。
- `flutter_shell/lib/map/`：地图、WGS-84/GCJ-02 转换、标记和地图交互。
- `flutter_shell/lib/mission/`：任务文档模型、控制器、任务面板和编辑弹窗。
- `flutter_shell/lib/configuration_workspace.dart`：配置、校准、遥测和飞控参数界面。
- `flutter_shell/lib/theme/`：`MgFonts`、`MgTypography`、`MgMetrics`、`MgColors` 单一来源。
- `flutter_shell/test/`：不依赖真实飞控的 Flutter 测试。

## 生成物和禁止提交内容

`build/`、`build_check/`、`build_docker/`、`.venv/`、生成 MAVLink C 头、IDE 状态、
硬件 dump 和临时日志都不进入提交。
