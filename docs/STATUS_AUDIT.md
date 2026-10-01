# 当前状态审计

> 本文只记录当前代码的可复现事实。历史冻结审计在 `docs/audits/`；发布变化在
> `CHANGELOG.md`。当前代码基线版本：`0.0.25`。本文件只记录当前 checkout 已复核的事实。

## 门禁基线

| 检查 | 当前结果 |
| --- | --- |
| `git diff --check` | 通过 |
| `dart tools/check_ui_tokens.dart` | 通过 |
| Flutter analyze | 通过 |
| Flutter test | 19/19 |
| 后端 offline/simulation | 34/34 |
| macOS debug 构建 | 通过 |
| Windows build/preflight | workflow 统一调用预检脚本；脚本会先生成 MAVLink 头，再构建 bundle 并校验关键 FFI 导出。未在本机 Windows 复核 |
| Linux Flutter bundle | CI 安装 GTK/CMake/Ninja，生成 MAVLink 头，构建 debug bundle，并校验 `libmgcore.so`、数据文件和关键 FFI 导出。未在本机 Linux 复核 |
| Android APK bridge | Gradle/CMake 构建 `libmgcore.so`，数据 JSON 作为 APK assets 打包；Java USB manager 按 QGC 结构使用 `usb-serial-for-android`，native 侧通过 JNI 回调接入 `ILink`。CI 构建 debug APK 并检查产物；权限交互和真机 USB 仍需 Android 设备验证 |
| 远端 CI | 不跟踪（用户要求 `CI-001` WONTFIX） |

可重复命令：

```bash
tools/preflight.sh
bash tools/audit_status.sh
```

## FFI 消费统计

统计脚本当前输出：

| 项目 | 数量 |
| --- | ---: |
| FFI exports | 285 |
| Flutter 使用 | 65 |
| 测试触达 | 198 |
| 前端和测试都未触达 | 85 |

这些数字必须由 `tools/audit_status.sh` 生成，不手工维护。无人区不等于可删除，公开
兼容 ABI 仍需遵守 API Catalog 的冻结规则。

## 已关闭的问题

- `ARCH-001`：遥测缓存与序列化器已下沉到 `core/include/core/telemetry/`，core 不再依赖 bridge。
- `CAL-001` / `CAL-003`：校准读回失败不再误报成功；历史写盘移出锁内。
- `CAL-002`：guided point 的 COMMAND_LONG 经纬度单位修正为度。
- `CAL-004`：磁力计校准按 QGC/MP 流程先清理旧状态并显式开启
  `MAG_CAL_PROGRESS`/`MAG_CAL_REPORT`；前端修复罗盘进度 JSON 编码导致状态更新中断，
  取消时立即结束本地校准态并在真机上验证进度和取消都有效。
- `HOME-001`：识别飞控后请求 `HOME_POSITION` 和 `EXTENDED_SYS_STATE` 1Hz。
- `HOME-002`：新增 `mg_command_set_home`，设置 Home 前由 UI 确认。
- `LOG-001` / `LOCK-001`：飞行日志接入 STATUSTEXT/连接事件，回调移出锁内。
- `EVENT-001`：`MG_EVENT_WAYPOINT_REACHED` 已接线并带 `seq`。
- `SURVEY-001`：`survey_grid` / `corridor` 已成为参数化任务实体。
- `INTEROP-001`：支持 MP/QGC `QGC WPL 110` `.waypoints` 导入导出。
- `MISSION-002`：支持从飞控读取任务并导入任务文档，归一化 `_INT` frame。
- `MISSION-003`：Flutter 任务规划 UI、地图单击、新增/编辑航点、引导操作已接入。
- `TEST-001` / `TEST-002` / `TEST-003`：真实 UI FFI、飞行命令、批量参数变更集已有模拟测试。
- `CONTRACT-001` / `DEPRECATION-001`：参数入口和兼容入口规则已登记。
- `TESTGUARD-001` / `TOOL-001` / `TOOL-002`：架构守卫和审计统计不再静默空转。
- `CLEANUP-001`：组件画廊已明确为开发工具，不进入主流程。
- `CI-001`：按用户要求标记为 WONTFIX，不再列为待办。

## 当前剩余项

### `UI-001`：聚合诊断尚未接入 Flutter

后端已有 `mg_connection_get_diagnostics`、`mg_telemetry_get_dashboard_snapshot`、
`mg_system_get_diagnostics`。当前 Flutter 仍以多个快照拼接状态，后续可统一接入。

### `UI-002`：参数同步的长任务取消尚未接入 Flutter

任务上传、下载已经通过 `mg_operations_get_snapshot` 显示进度并支持真实取消；参数同步
后端也已支持 `mg_operations_cancel("params")` 和 cancelled/failed 终态，但 Flutter
页面仍由各页等待。operations 快照现在还包含 `calibration` 和 `log`，对应的校准与日志
下载取消也已接入；`mg_system_get_diagnostics` 还包含最近参数写入的 requested/actual/
state/error 历史。前端后续可统一消费同一套 operation 状态。

### `DEAD-001`：固件刷写占位

`FirmwareFlasher` 仍是未接通的占位实现；`mg_firmware_flash` 诚实返回 not supported。
除非要实现完整下载和 bootloader 流程，否则不只改状态接口。

### `VIDEO-001`：FFmpeg 回调并发风险

启用 FFmpeg 时，`FFmpegDecoder` 的 frame callback 需要进一步做并发保护；当前默认构建
未启用 FFmpeg，因此尚未改动和验证。

### 任务规划边界

`.waypoints` 与 QGC `.plan` 的任务文件互操作已可用：`.plan` 支持普通 SimpleItem、
Home、默认速度和固件/机型元数据，并对复杂项、非空 GeoFence/Rally 和无法无损表达
的字段返回明确错误或 warnings。地理围栏/Rally 的统一文档仍未实现；任务规划页面已
接入回读验证、脏状态提示、校验诊断定位、地图/列表选中和拖动排序。

### `REFACTOR-001`：任务 FFI 适配器已从 Flutter 主控制器迁出

`MissionDocumentFfiBackend` 现在独立负责任务文档相关的动态符号绑定、JSON envelope、
任务传输进度和文件互操作；`ConnectionController` 只提供共享 Core、动态库和连接状态
回调。公开 `MissionDocumentBackend` 契约和任务页面行为不变。当前仅完成任务领域，
遥测相关绑定也已迁移到 `TelemetryFfiBackend`；连接控制器继续负责状态缓存、轮询和
通知。参数动态绑定也已迁移到 `ParameterFfiBackend`，参数缓存、元数据批处理和写入
确认仍在连接控制器中。校准和飞行命令适配器仍在主控制器中，后续逐域迁移。
校准动态绑定也已迁移到 `CalibrationFfiBackend`，校准状态机、取消收敛和进度轮询仍由
连接控制器负责。飞行命令动态绑定也已迁移到 `FlightCommandFfiBackend`，QGC/MP 的
模式门控、解锁确认和安全流程仍由连接控制器负责。

## 当前工作区

提交前必须确保：

```bash
git status --short --branch
git diff --check
tools/preflight.sh
```

不要把 `build/`、`build_check/`、生成头、硬件日志、SITL 生成物和本地记忆提交。

## 架构维护结论

当前架构方向保留，不进行推倒重写。后续维护遵循以下顺序：

1. 先冻结公开 FFI 和 JSON 契约，兼容入口只修正确性，不承载新功能。
2. 先拆内部职责，再改变协议行为：优先拆 `ConnectionController` 和
   `MavlinkAdapter::Impl` 的职责，不改变现有调用方契约。
3. 任务、参数、连接和校准分别记录 MP/QGC 的选择依据；ArduPilot 语义参考 MP，
   通用状态机参考 QGC。
4. 新的“支持”必须同时有代码、聚焦测试、文档和实际平台验收记录；只有构建路径
   不能宣称平台已支持。
