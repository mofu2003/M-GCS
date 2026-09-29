# M-GCS

这是 M-GCS 的**后端、接口与 Flutter 外壳**。仓库保留 C++ 后端核心、C FFI 契约、后端测试、构建工具和 `flutter_shell/`，提供可运行的跨平台地面站外壳和稳定接口。

当前版本：`0.0.12`

## 仓库范围

本分支包含：

```text
M-GCS/
|- core/                  C++ 后端核心：传输、MAVLink、飞控、任务、参数、遥测、校准、日志
|- bridge/ffi/            对外 C ABI 与 JSON 契约（唯一稳定接口边界）
|- flutter_shell/         Flutter 地面站外壳
|- tests/                 跨模块测试、硬件只读工具和手动测试
|- tools/                 MAVLink 生成、Docker 开发和硬件测试脚本
|- docker/                Linux 开发容器
|- docs/                  项目文档、协作流程、架构决策和开发指南
|- third_party/           MAVLink 定义、cJSON 等第三方源码
|- CMakeLists.txt         后端构建入口
\- .github/workflows/     CI（后端与 Flutter 构建、测试）
```

仓库不包含旧的完整 Flutter 应用；当前只保留 `flutter_shell/` 中的精简外壳（地图、连接、遥测、传感器波形和校准工作区）。macOS 已完成本地构建验证；Windows、Linux 和 Android 仍需真机验收。旧的完整前端保留在历史提交和 `v0.0.5` 标签中。

Windows 桌面构建会同时生成并嵌入 `mgcore.dll`，串口入口使用
`serial://COMx?baud=115200`。本地 Windows 预检见
[开发指南](docs/DEVELOPMENT.md#windows-桌面运行)。

## 前端如何接入

前端不直接依赖 `core/` 的 C++ 类型，只通过：

```text
bridge/ffi/ffi_api.h
```

该头文件聚合了各领域接口：

- `ffi_api_core.h`：生命周期、错误码、事件回调。
- `ffi_api_connection.h`：连接会话的打开/状态/关闭、设备发现；另保留旧焦点选择接口供兼容调用。
- `ffi_api_telemetry.h`：遥测、健康、传感器、RC 和 servo 快照。
- `ffi_api_params.h`：参数下载、读取、写入和参数变更集。
- `ffi_api_mission.h`：任务上传下载、围栏、Rally 和测绘。
- `ffi_api_flight.h`：飞控命令、模式、上锁/解锁和引导。
- `ffi_api_configuration.h`：参数元数据、机架和配置页面数据。
- `ffi_api_firmware.h`：固件类型、能力和版本。
- `ffi_api_geofence.h`、`ffi_api_logging.h`、`ffi_api_payload.h`、`ffi_api_environment.h`：围栏、日志、载荷和环境能力。

连接流程使用 session ID，而不要求前端先管理 active vehicle：

```text
mg_connection_open("serial:///dev/tty...?..." 或 "udp://0.0.0.0:14550")
    -> session_id
mg_connection_get_session_state(session_id)
mg_connection_close(session_id)
```

`mg_connection_open()` 只建立会话、不改变 UI 焦点；连接状态查询和断开均按 session ID 操作。旧 `mg_connection_connect*()` 仍作为单机兼容包装保留，内部会选中其创建的会话。

契约约定：

1. 所有返回的 `char*` 都由调用方通过 `mg_free_string()` 释放。
2. 事件回调的 `json` 参数也由回调方释放。
3. 复杂结果使用 JSON envelope，`success` / `error` 语义稳定。
4. 生命周期为 `mg_create()` → `mg_*()` → `mg_destroy()`。
5. `mg_vehicle_open()` 只创建独立飞控会话，不改变当前焦点；只有 `mg_vehicle_select()` 才改变焦点。旧的 `mg_connection_connect()`、TCP/UDP 便捷入口会保持单机兼容行为，连接成功后选中它创建的会话。

完整分组、兼容入口和新增/弃用规则见 [`bridge/ffi/API_CATALOG.md`](bridge/ffi/API_CATALOG.md)。

## 快速验证

### Docker

```bash
./tools/docker-dev.sh check
```

```powershell
.\tools\docker-dev.ps1 check
```

### 本地构建

先生成 MAVLink C 头文件：

```bash
./tools/generate-mavlink.sh
```

```powershell
.\tools\generate-mavlink.ps1
```

然后编译并运行默认门禁：

```bash
cmake -S . -B build_check -DMGCORE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_check --config Release
ctest --test-dir build_check --output-on-failure --no-tests=error \
  -L "offline|simulation" -LE "hardware|manual"
```

## 安全边界

真实硬件测试前必须确认飞控未解锁，并明确测试会发送的命令。未经确认不得执行解锁、起飞、切模式、写参数、任务启动、电机测试或危险校准。

`hardware` 和 `manual` 标签的测试默认不进入 CTest；需要显式开启 `MGCORE_REGISTER_HARDWARE_TESTS=ON`。

## 文档

- [文档索引](docs/README.md)
- [项目结构](docs/PROJECT_LAYOUT.md)
- [开发指南](docs/DEVELOPMENT.md)
- [任务规划后端契约](docs/MISSION_PLANNING.md)
- [架构说明](docs/ARCHITECTURE.md)
- [开发约束](docs/CONSTRAINTS.md)
- [故障排查](docs/TROUBLESHOOTING.md)
- [更新日志](CHANGELOG.md)
- [ADR](docs/adr/)

## 许可证

[MIT](LICENSE)
