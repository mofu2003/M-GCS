# 当前状态审计

> 本文只记录当前代码的可复现事实。历史冻结审计在 `docs/audits/`；发布变化在
> `CHANGELOG.md`。更新时间：2026-09-30。当前版本：`0.0.12`。

## 门禁基线

| 检查 | 当前结果 |
| --- | --- |
| `git diff --check` | 通过 |
| `dart tools/check_ui_tokens.dart` | 通过 |
| Flutter analyze | 通过 |
| Flutter test | 12/12 |
| 后端 offline/simulation | 34/34 |
| macOS debug 构建 | 通过 |
| Windows build/preflight | 有 workflow 和脚本，未在本机复核 |
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
| Flutter 使用 | 60 |
| 测试触达 | 198 |
| 前端和测试都未触达 | 86 |

这些数字必须由 `tools/audit_status.sh` 生成，不手工维护。无人区不等于可删除，公开
兼容 ABI 仍需遵守 API Catalog 的冻结规则。

## 已关闭的问题

- `ARCH-001`：遥测缓存与序列化器已下沉到 `core/include/core/telemetry/`，core 不再依赖 bridge。
- `CAL-001` / `CAL-003`：校准读回失败不再误报成功；历史写盘移出锁内。
- `CAL-002`：guided point 的 COMMAND_LONG 经纬度单位修正为度。
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

### `UI-002`：长任务取消尚未接入 Flutter

后端已有 `mg_operations_get_snapshot` / `mg_operations_cancel`。参数同步、任务上传
目前仍由各页面等待，后续统一接入取消和进度显示。

### `DEAD-001`：固件刷写占位

`FirmwareFlasher` 仍是未接通的占位实现；`mg_firmware_flash` 诚实返回 not supported。
除非要实现完整下载和 bootloader 流程，否则不只改状态接口。

### `VIDEO-001`：FFmpeg 回调并发风险

启用 FFmpeg 时，`FFmpegDecoder` 的 frame callback 需要进一步做并发保护；当前默认构建
未启用 FFmpeg，因此尚未改动和验证。

### 任务规划边界

QGC `.plan`、地理围栏/Rally 的统一文档尚未实现；当前 `.waypoints`、任务实体、survey
编译、上传/下载/验证已可用。任务规划页面已接后端，但拖拽排序尚未实现，当前使用上移/下移。

## 当前工作区

提交前必须确保：

```bash
git status --short --branch
git diff --check
tools/preflight.sh
```

不要把 `build/`、`build_check/`、生成头、硬件日志和本地记忆提交。
