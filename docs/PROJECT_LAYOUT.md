# 项目结构

## 顶层

```text
M-GCS/
|- app/                  Flutter 前端
|- bridge/ffi/           对外 C ABI 与后端适配
|- core/                 C++ 后端核心
|- docs/                 当前维护文档与 ADR
|- tests/                集成、硬件和手动测试
|- tools/                开发与硬件测试工具
|- third_party/          第三方源码、协议定义和许可证
|- CMakeLists.txt        后端构建入口
\- .github/workflows/    CI
```

## Flutter 前端

- `app/lib/application/`：应用契约、控制器、模型、JSON 解码和 MgCore FFI 适配。
- `app/lib/features/frontstage/`：地图工作台和前台交互。
- `app/lib/features/backstage/`：参数等后台页面。
- `app/lib/features/map/`：地图源、地图 surface 和定位封装。
- `app/lib/features/shell/`：应用根 shell。
- `app/lib/ui/skin/`：皮肤协议与 token。
- `app/lib/ui/layouts/`：业务页面使用的稳定布局入口。
- `app/lib/ui/skins/mac_glass/`：当前默认皮肤实现。
- `app/test/`：单元和 Widget 测试。
- `app/macos/`、`app/windows/`：Flutter 平台工程。

## 后端

- `core/include/core/`：后端公共 C++ 头文件。
- `core/src/transport/`：串口、TCP、UDP、内存链路、端口发现和连接管理。
- `core/src/mavlink/`：MAVLink adapter、命令队列、参数和任务传输。
- `core/src/vehicle/`：飞控模型与多机管理。
- `core/src/firmware/`：ArduPilot/PX4 profile、模式和参数映射。
- `core/src/mission/`、`geofence/`、`calibration/`：任务、围栏与校准。
- `core/src/telemetry/`、`settings/`、`log/`：遥测与日志。
- `core/tests/`：后端离线与模拟测试。

`bridge/ffi/` 是外部消费者唯一稳定的 native 契约边界。公共函数放入对应的 `ffi_api_*.h`，并由 `ffi_api.h` 聚合。

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
