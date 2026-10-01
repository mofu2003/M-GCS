# QGC `.plan` 任务文件互操作

> 格式优先级见 [ADR 0008](adr/0008-mission-file-format-priority.md)。
> `.plan` 不删除：它用于 PX4/QGC 工作流和不能由 `.waypoints` 无损表达的复杂规划数据。

本文定义 M-GCS 当前对 QGroundControl `.plan` JSON 的导入和导出范围。实现依据是
QGC 的 `MissionController`、`MissionItem` 和 `SimpleMissionItem`，不是从样例文件
反推格式。

## 当前支持范围

| QGC 字段 | M-GCS 映射 | 当前行为 |
| --- | --- | --- |
| 根 `fileType` | 无 | 导入时若非空必须是 `Plan`；缺失时按旧文件容忍 |
| 根 `groundStation` | 无 | 导入忽略；导出写 `M-GCS` |
| 根 `version` | 无 | 当前导出为 1 |
| `mission.version` | 无 | 当前导出为 2；其他版本导入时产生 warning |
| `mission.firmwareType` | `metadata.firmware_profile` | ArduPilot / PX4 可映射；未知值告警 |
| `mission.vehicleType` | `metadata.vehicle_type` | 常见 copter / plane / VTOL 可映射；未知值告警 |
| `mission.cruiseSpeed` | `defaults.speed_ms` | 正值可映射；非正值产生 warning |
| `mission.hoverSpeed` | 无独立字段 | 导入产生 warning，不静默改写 cruise speed |
| `mission.globalPlanAltitudeMode` | 无独立字段 | 导入产生 warning；每个任务项的 frame 仍是权威 |
| `mission.plannedHomePosition` | `home` | `[lat, lon, alt]`；null 或缺失时 home 为未设置 |
| `mission.items[]` | `entities[]` | 只接受 `SimpleItem`，顺序映射为 `order` |
| 根 `geoFence` | `geo_fence` | version 2 的 polygons/circles/breachReturn 映射为正式模型；version 1 忽略并告警 |
| 根 `rallyPoints.points` | `rally_points[]` | version 2 映射为正式 Rally 点；旧 version 1 忽略并告警 |

## `SimpleItem` 映射

当前格式使用 7 值 `params`：

```text
params = [param1, param2, param3, param4, latitude, longitude, altitude]
```

旧版 QGC 文件可能使用 4 值 `params` 加 `coordinate` 数组。导入端同时兼容两者；
当 7 值格式存在时优先使用 `params[4..6]`。

| QGC 字段 | M-GCS 映射 |
| --- | --- |
| `type` | `SimpleItem` 映射为实体；其他类型提升为正式 `MissionComplexItem` |
| `frame` | `entity.frame`，导入时归一化 `_INT` 帧 5/6/11 |
| `command` | `entity.command`，实体类型由后端命令目录反推 |
| `autoContinue` | `entity.autocontinue` |
| `doJumpId` | 不保留；导出时按合并后的 item 顺序对简单项和复杂项统一重编号 |
| `params[0..3]` | `entity.params[0..3]` |
| `params[4..6]` | `entity.latitude/longitude/altitude_m`；null 表示未提供位置 |
| `coordinate` | 旧格式位置；存在时覆盖 `params[4..6]` 的缺失值 |
| `altitudeMode` / `altitude` / `AMSLAltAboveTerrain` | 不保留，导入产生 warning |

不在命令目录中的 MAV_CMD 可以进入文档，但会返回 warning；后续校验会把不支持的命令
标为错误，调用方不应忽略这项诊断。ComplexItem、GeoFence 和 Rally 不进入实体列表。
GeoFence 和 Rally 已进入正式文档模型；ComplexItem 以 `MissionComplexItem` 形式保存
`id`、`item_type`（取 `complexItemType`，回退 `type`）和原始 JSON，再次导出同一
`.plan` 时会按该 JSON 重建完整 ComplexItem。

复杂项只做无损保留，不参与编译：文档含 `complex_items` 时校验返回
`complex_item.unsupported` 错误，`validate`/`compile`/上传/回读全部失败。这是有意
设计，避免上传时只飞普通航点、与导入的 `.plan` 不一致却没有提示。

## 导出边界

`.plan` 不静默 flatten 设计意图：

- `survey_grid` / `corridor` 导出仍返回错误，不展开成普通航点；它们不是输入中保留的
  QGC ComplexItem。
- `complex_items` 导出时按原始 JSON 放回 `mission.items`，保持 Survey、CorridorScan
  等条目往返无损；M-GCS 目前不语义化编辑其内部字段。
- disabled 实体无法由 QGC `SimpleItem` 表达，导出返回错误。
- entity label / notes 无法无损写入 SimpleItem，导出返回 warning。
- 未设置 home 时导出 `plannedHomePosition: null`，并返回 warning。

调用方应同时读取返回 envelope 的 `warnings`；导入和导出都包含该数组，即使为空。

## Round-trip 保证

普通 SimpleItem 任务满足以下往返条件：

1. Home 经纬度、高度和设置状态保持一致。
2. 任务项数量、顺序、command、frame、autocontinue 和前四个参数保持一致。
3. 有位置的任务项经纬度、高度保持一致。
4. firmware profile、vehicle type 和 cruise speed 在受支持取值范围内保持一致。

不保证保留 `doJumpId`、QGC 地形/海拔模式、hover speed、label、notes 和参数化测绘
意图；这些字段必须通过错误或 warnings 显式暴露。ComplexItem、Rally 和 GeoFence 已
进入正式文档模型，参与文本 round-trip。ComplexItem 通过 `entity_insert_index`
记录它在任务序列中相对普通航点的位置，导出时与 SimpleItem 按原顺序交错输出，不再
固定追加到末尾。

## 参考实现

- QGC `src/MissionManager/MissionController.cc`：plan 根字段和 `mission.items`。
- QGC `src/MissionManager/MissionItem.cc`：SimpleItem 的 frame / command / params 读写。
- QGC `src/MissionManager/SimpleMissionItem.cc`：altitude / terrain 附加字段。

## FFI 入口

```text
mg_mission_doc_export_format(core, "plan")
mg_mission_doc_import_format(core, "plan", json_text)
```

导出响应字段：`success`、`format`、`item_count`、`text`、`warnings`。
导入响应字段：`success`、`revision`、`can_undo`、`can_redo`、`document`、`warnings`。
