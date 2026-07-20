# 项目结构

本文件描述当前源码树的职责划分。目标是让新维护者打开目录时能快速判断：哪里放源码，哪里放测试，哪里只是构建产物或历史归档。

## 顶层目录

```text
mgroundcontrol/
|- app/                  Flutter 跨平台前端
|- bridge/ffi/           C ABI 和 C++ 后端适配层
|- core/                 C++ 后端核心
|- docs/                 长期文档、ADR、历史归档
|- tests/                顶层集成测试和真实硬件测试
|- tools/                开发工具和硬件测试脚本
|- third_party/          第三方依赖
|- prototypes/           实验原型预留
|- build/                后端共享库输出，生成物
\- build_check/          CMake/Ninja 检查构建目录，生成物
```

## 前端

`app/` 是 Flutter 跨平台前端工程。当前源码包已恢复 macOS/Windows 桌面骨架，长期目标平台按优先级为 Windows、Android、macOS、iOS：

- `app/lib/application/`：应用语义契约、控制器、模型和 MgCore 适配层。
- `app/lib/features/frontstage/`：前台任务工作台页面、前台 view model 和 controller adapter。
- `app/lib/features/shell/`：应用根 shell，负责把默认前台页面挂到 Flutter app。
- `app/lib/ui/skin/`：皮肤协议、token、scope 和编译时默认皮肤工厂。
- `app/lib/ui/layouts/`：稳定布局入口，例如 `MgFrontstageShell`。
- `app/lib/ui/skins/mac_glass/`：第一套浅色玻璃商业皮肤实现。
- `app/lib/platform/`：平台差异抽象预留。
- `app/test/`：前端单元测试和 widget smoke tests。
- `app/macos/`、`app/windows/`：Flutter 生成的平台工程。

后续重新启用 Android/iOS 时，应把平台工程视为 native packaging 和权限适配层，不应复制业务页面。

`app/build/`、`app/.dart_tool/`、`app/.idea/` 是生成物或本地 IDE 状态，不进入长期源码。

## 后端

`core/` 是 C++ 后端领域核心：

- `core/src/transport/`：串口、TCP、UDP、内存链路和端口发现。
- `core/src/mavlink/`：MAVLink 适配、发送、命令队列、参数和任务传输。
- `core/src/vehicle/`：飞控对象、多机管理和连接状态。
- `core/src/mission/`、`core/src/geofence/`、`core/src/calibration/`：任务、围栏和校准。
- `core/src/firmware/`：ArduPilot/PX4 profile、模式和能力差异。
- `core/src/telemetry/`、`core/src/log/`、`core/src/settings/`：遥测、日志和设置能力。
- `core/tests/`：后端内部测试辅助。

`bridge/ffi/` 是后端对 Flutter、测试工具和其他外部消费者暴露的 C ABI。所有外部可见函数应通过 `ffi_api_*.h` 和 `ffi_api.h` 暴露。

## 测试和工具

- `tests/`：离线、模拟、真实硬件和手动测试入口。
- `tests/manual/`：需要人工确认或真实硬件风险更高的测试。
- `tools/run-hardware-test.ps1`：Windows 真实硬件测试统一入口。

真实硬件输出、dump、日志默认不保留；需要归档时应放到明确的外部位置并说明用途。

## 文档

- `docs/`：长期有效文档。
- `docs/adr/`：架构决策记录。
- `docs/archive/`：历史临时计划和旧规格归档。

`docs/archive/` 中的内容不代表当前状态，也不作为实施依据。

## 不应进入源码树的内容

- `.DS_Store`、IDE 临时状态、本地缓存。
- `build/`、`build_check/`、`app/build/`、`app/.dart_tool/`。
- 真实飞控 dump、硬件日志、一次性调试输出。
- 未迁移为正式文档或 ADR 的临时计划。
