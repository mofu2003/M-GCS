# 项目结构

## 顶层

```text
M-GCS/
|- bridge/ffi/           对外 C ABI 与后端适配（唯一稳定接口边界）
|- core/                 C++ 后端核心
|- docs/                 当前维护文档与 ADR
|- tests/                集成、硬件和手动测试
|- tools/                开发与硬件测试工具
|- docker/               Linux 开发容器入口
|- third_party/          第三方源码、协议定义和许可证
|- CMakeLists.txt        后端构建入口
\- .github/workflows/    CI
```

本分支不包含前端应用；前端版本在 `main` 分支。

## 后端核心

- `core/include/core/`：后端公共 C++ 头文件。
- `core/src/transport/`：串口、TCP、UDP、内存链路、端口发现和连接管理。
- `core/src/mavlink/`：MAVLink adapter、命令队列、参数和任务传输。
- `core/src/vehicle/`：飞控模型与多机管理。
- `core/src/firmware/`：ArduPilot/PX4 profile、模式和参数映射。
- `core/src/mission/`、`geofence/`、`calibration/`：任务、围栏与校准。
- `core/src/telemetry/`、`settings/`、`log/`、`ftp/`：遥测、日志与文件传输。
- `core/src/camera/`、`video/`、`terrain/`、`sitl/`：载荷、视觉、地形和仿真。
- `core/tests/`：后端离线与模拟测试。

## FFI 桥接层

`bridge/ffi/` 是外部消费者唯一稳定的 native 契约边界。公共函数放入对应的 `ffi_api_*.h`，由 `ffi_api.h` 聚合：

- 声明：`ffi_api_core.h`、`ffi_api_connection.h`、`ffi_api_telemetry.h` 等。
- 实现：`ffi_*.cc` 按领域拆分。
- 契约目录：[`API_CATALOG.md`](../bridge/ffi/API_CATALOG.md)。

## 测试与第三方源码

- `tests/`：跨模块测试和硬件只读工具。
- `tests/manual/`：需要人工确认的高风险测试。
- `third_party/mavlink/`：MAVLink XML 定义和上游源码；当前不含 C 生成头。
- `third_party/cjson/`：vendored cJSON 源码。

## 不进入仓库

- 构建产物和包管理器缓存。
- IDE、本机系统状态和临时备份。
- 飞控 dump、硬件日志和一次性调试输出。
- 临时计划、已完成迁移记录和无实现约束作用的历史规格。
