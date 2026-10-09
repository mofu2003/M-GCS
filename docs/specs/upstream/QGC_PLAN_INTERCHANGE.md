# QGC `.plan` 任务文件互操作

当前版本基线：`0.0.80`。

> 格式优先级见 [ADR 0008](../../architecture/adr/0008-mission-file-format-priority.md)。
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
params = [param1, param2, param3, param4, param5_or_latitude, param6_or_longitude, param7_or_altitude]
```

旧版 QGC 文件可能使用 4 值 `params` 加 `coordinate` 数组。导入端同时兼容两者；
当 7 值格式存在时优先使用 `params[4..6]`。

`params[4..6]` 的语义由 `frame` 和命令共同决定：全球坐标帧下的定位命令按
纬度/经度/高度处理；`MAV_FRAME_MISSION`（2）下它们是原始 `param5/param6/param7`。
QGC 的 `DO_*`、`CONDITION_*` 和 `DO_MOUNT_CONTROL` 等非定位命令不能因为
`coordinate: [0,0,2]` 就被画成 `(0,0)` 航点。固定翼 `NAV_TAKEOFF`（22）的位置
仍然保留。

| QGC 字段 | M-GCS 映射 |
| --- | --- |
| `type` | `SimpleItem` 映射为实体；其他类型提升为正式 `MissionComplexItem` |
| `frame` | `entity.frame`，导入时归一化 `_INT` 帧 5/6/11 |
| `command` | `entity.command`，实体类型由后端命令目录反推 |
| `autoContinue` | `entity.autocontinue` |
| `doJumpId` | 不保留；导出时按合并后的 item 顺序对简单项和复杂项统一重编号 |
| `params[0..3]` | `entity.params[0..3]` |
| `params[4..6]` | 定位命令映射为 `entity.latitude/longitude/altitude_m`；`MAV_FRAME_MISSION` 的 param7 映射到 `altitude_m`，param5/6 不生成位置，任意 param5/6 仍不进入文档模型 |
| `coordinate` | 旧格式位置；仅定位命令按经纬度导入，非定位命令不生成位置 |
| `altitudeMode` / `altitude` / `AMSLAltAboveTerrain` | 不保留，导入产生 warning |

不在命令目录中的 MAV_CMD 可以进入文档，但会返回 warning；后续校验会把不支持的命令
标为错误，调用方不应忽略这项诊断。ComplexItem、GeoFence 和 Rally 不进入实体列表。
GeoFence 和 Rally 已进入正式文档模型；ComplexItem 以 `MissionComplexItem` 形式保存
`id`、`item_type`（取 `complexItemType`，回退 `type`）和原始 JSON，再次导出同一
`.plan` 时会按该 JSON 重建完整 ComplexItem。

复杂项在 M-GCS 中保留原始 JSON，并在 `TransectStyleComplexItem.Items` 存在时复用
QGC 已算好的内部 MissionItem，展开成普通任务项参与编译、估算和上传，而不是自己重推
Survey 网格。只有缺少可展开区段的复杂项才返回 `complex_item.unsupported` 并阻断，
避免上传时静默丢掉 Survey/Corridor。M-GCS 当前仍不能语义化编辑 ComplexItem 内部字段。

GeoFence 和 Rally 在文档里是独立模型，`validate` 会校验其几何（多边形至少 3 个顶点、
经纬度在合法范围、圆半径为正、breach-return 点有效），但不会因为存在围栏/Rally 而
阻断任务上传：它们是独立 MAVLink mission type，与航点任务分开处理。

围栏可以单独下发：`compile_geo_fence_plan` 把 `geo_fence` 编译成 fence mission 项，
编码与 QGC `GeoFenceManager` 一致（多边形 5001/5002，`param1` 为顶点总数；圆形
5003/5004，`param1` 为半径；breach-return 5000 + `MAV_FRAME_GLOBAL_RELATIVE_ALT`），
顺序为多边形 → 圆形 → breach-return。FFI 入口是 `mg_mission_doc_compile_fence`、
`mg_mission_doc_upload_fence` 和 `mg_mission_doc_clear_fence`。

Rally 同样可单独下发：`compile_rally_plan` 按 QGC `RallyPointManager` 编码把
`rally_points` 编译成 `MAV_CMD_NAV_RALLY_POINT`（5100）+ `MAV_FRAME_GLOBAL_RELATIVE_ALT`
的任务项，FFI 入口是 `mg_mission_doc_compile_rally`、`mg_mission_doc_upload_rally` 和
`mg_mission_doc_clear_rally`。

回读方向同样支持：`geo_fence_from_plan` / `rally_points_from_plan` 是编译的逆过程，
围栏分组规则与 QGC `GeoFenceManager` 相同（连续且 `param1` 顶点数一致的顶点属于同一
多边形，中途改变顶点数或 inclusion 会被拒绝）。在一次已完成下载后，
`mg_mission_doc_import_fence_from_vehicle` / `mg_mission_doc_import_rally_from_vehicle`
把飞控围栏/Rally 读回文档。

校验方向：`mg_mission_doc_verify_fence` / `mg_mission_doc_verify_rally` 在一次成功下载后
比较文档与飞控，返回 `matches`、`difference_count` 和结构化 `differences`。

## 导出边界

`.plan` 不静默 flatten 设计意图：

- `survey_grid` / `corridor` 导出为可重新导入的 QGC `Survey` / `CorridorScan`
  ComplexItem，并嵌入编译后的任务项；无法无损表达时才退回 SimpleItem 并返回 warning。
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
