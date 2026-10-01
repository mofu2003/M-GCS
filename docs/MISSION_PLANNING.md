# 任务规划后端契约

本文是任务规划的当前权威说明。任务规划由“任务文档”和“编译后的 MissionPlan”两层组成：

- `MissionDocument`：前端可编辑、带稳定实体 ID 和 revision 的任务源模型。
- `MissionPlan`：后端编译出的 MAVLink 任务，只用于传输和回读比较。

编辑、校验、保存、撤销和估算不写飞控；上传和从飞控读取必须由用户显式触发。

## 文档结构

```json
{
  "schema_version": 1,
  "metadata": {"name": "Area A", "firmware_profile": "ardupilot", "vehicle_type": "copter"},
  "home": {"set": true, "lat": 39.9, "lon": 116.4, "alt_m": 20},
  "defaults": {"frame": 3, "altitude_m": 50, "speed_ms": 12},
  "revision": 4,
  "entities": [
    {
      "id": "wp-1",
      "type": "waypoint",
      "enabled": true,
      "order": 0,
      "command": 16,
      "frame": 3,
      "autocontinue": 1,
      "lat": 39.9,
      "lon": 116.4,
      "alt_m": 50,
      "params": [0, 2, 0, 0]
    }
  ]
}
```

实体支持稳定 `id`、`type`、`command`、`frame`、位置、高度、四个命令参数和 enabled。
`mg_mission_doc_apply` 支持 `add`、`update`、`remove`、`move`、`clear`、`replace`；
`expected_revision` 不匹配时拒绝修改，避免旧快照覆盖新编辑。

## 命令目录

`mg_mission_get_command_catalog` 是唯一的命令语义来源，返回：

- MAV_CMD 符号名 `name`。
- 中文显示名 `label`。
- 文档实体类型 `entity_type`。
- 分类、说明、是否需要位置/高度。
- 参数的 index、label、unit、type、default、范围、required 和 enum options。

当前目录覆盖航点、起飞、降落、RTL、盘旋、速度、相机触发、舵机、延时和距离条件。
前端下拉和编辑表单必须根据目录生成，不复制 MAV_CMD 参数表。

## 参数化测绘

`survey_grid` 和 `corridor` 是任务实体，不是前端专属对象。实体保存设计意图，
`SurveyCalculator` 在编译期展开为多个任务项；`source_entity_ids` 将每个展开项指回
源实体。`estimate` 也使用展开结果统计距离、时间、航点和照片数。

`survey_grid` 的核心字段：`width_m`、`length_m`、`angle_deg`、`cross_grid`、`alt_m`、
`speed_ms`、`return_to_start`、相机参数和重叠率。

`corridor` 额外使用实体起点与 `survey.end_lat` / `survey.end_lon`、
`survey.corridor_width_m`。

## 上传、读取和验证

```text
编辑 -> validate -> compile -> upload -> 轮询 operations
回读 -> mission_download -> 轮询 operations
验证 -> mission_doc_verify
读取 -> mission_download -> 轮询 operations -> import_from_vehicle
```

上传的单次返回只表示传输已启动，必须轮询 operation 快照（兼容入口
`mg_mission_get_progress`）到完成。读取任务
使用 `mg_mission_doc_import_from_vehicle`，要求一次完整下载已成功完成；调用方应在覆盖
本地任务前确认。导入会保留 command/frame/参数/位置，但不能还原参数化测绘意图。
飞控回读的 `_INT` frame 5/6/11 会归一化为文档使用的 0/3/10。

### ArduPilot Home 语义

ArduPilot 的 mission 传输列表把 Home 放在第 0 项，后续才是可见任务项；PX4 不使用
这个约定。M-GCS 的上传和读取都必须遵守这条边界：

- 上传：`MissionDocument.home` 优先作为第 0 项 Home；没有规划 Home 时，才退回飞控
  当前 `HOME_POSITION`，最后退回当前位置。Home 不会出现在 `entities` 中。
- 读取：第 0 项 Home 同步回 `MissionDocument.home`，并从可见实体列表中移除。界面显示
  的任务数量不包含 Home，验证也要单独核对 Home 坐标和高度。

这一点和 QGC `MissionController` 读取 fake home、Mission Planner 的
`processToScreen` 读取后删除 Home 行一致。缺少该处理会导致 Home 被当作第一个航点，
任务数量、距离、高度校验和重新上传的 Home 全部错误。

进度优先通过 `mg_operations_get_snapshot` 的 `mission` 项读取，状态为
`downloading`、`uploading`、`waiting_ack` 或 `idle`，并包含百分比、计数、错误和取消
终态。`mg_operations_cancel("mission")` 会终止当前任务传输、把状态标为
`MAV_MISSION_OPERATION_CANCELLED`，并向飞控发送对应的 `MISSION_ACK`；没有活动传输时
返回 `canceled:false`，不会把已经完成的传输追认为取消。

`mg_mission_doc_verify` 只接受完成后的全量下载结果，比较数量、command、frame、位置、
高度、参数、autocontinue 和 mission_type；ArduPilot 任务还会单独比较第 0 项 Home。
验证时会把 `GLOBAL_INT/GLOBAL_RELATIVE_ALT_INT/GLOBAL_TERRAIN_ALT_INT` 分别归一为
对应的普通坐标帧，避免飞控使用 `MISSION_ITEM_INT` 表示同一语义时产生误报。位置和高度
字段按命令目录的 `requires_position/requires_altitude` 判断；Takeoff、RTL 和 DO/CONDITION
命令的无意义坐标不会被当成差异；命令目录声明的可选参数将 `NaN` 与默认的 `0` 视为
“未设置”的等价表示，但非零参数仍然严格比较。若仍有差异，结果会返回
`index/field/expected/actual`，前端任务页应显示这些明细并支持定位到对应任务项。
可接受的协议表示归一化单独返回在 `normalizations`，并带有 `reason`；它们不让
`matches` 变为 false，但可以用于诊断和状态提示。

验证失败后不能静默覆盖任一侧任务。界面应提供两个明确恢复动作：

- **重新上传本地任务**：继续以当前 `MissionDocument` 为准，重新执行上传和回读验证。
- **采用飞控任务**：在用户确认后，使用最近一次完整回读缓存替换本地文档，并清除本地脏状态。

未点击其中任一动作前，本地文档保持 dirty，启动任务按钮继续禁用。

## 启动任务

启动任务按 QGC `APMFirmwarePlugin::startMission` / `PX4FirmwarePlugin::startMission`
实现，入口是前端 `ConnectionController.startMission()`：

- ArduPilot 固定翼和 VTOL：先切 `AUTO`，确认模式已生效，再解锁。
- ArduPilot 多旋翼：先切 `GUIDED`，解锁后发送 `MAV_CMD_MISSION_START`。
- 飞行中的 ArduPilot：只切任务模式，不重复解锁。
- PX4：切 `AUTO_MISSION` 后解锁。

`MAV_CMD_DO_SET_MODE` 必须带 `MAV_MODE_FLAG_CUSTOM_MODE_ENABLED`。模式切换和解锁不能
只依据 `COMMAND_ACK`，必须轮询心跳摘要确认飞行模式和 `armed` 状态；超时后向上抛出
真实失败原因，由界面展示。

## 文件互操作

任务文件格式按飞控生态分层，详见
[ADR 0008](adr/0008-mission-file-format-priority.md)：

| 场景 | 推荐格式 |
| --- | --- |
| ArduPilot / Mission Planner | `.waypoints` |
| PX4 / QGroundControl | `.plan` |
| 简单航点跨 MP/QGC | `.waypoints` |
| ComplexItem / GeoFence / Rally / 保留 QGC 规划意图 | `.plan` |
| M-GCS 本地主保存 | `MissionDocument` JSON |

## 坐标精度与距离

任务文档内部使用 WGS-84 十进制度 `double` 保存位置。上传/AX 编译时按 ArduPilot 和
MAVLink `MISSION_ITEM_INT` 的 `degE7` 格式转换为 `int32`，分辨率约为
`1e-7` 度，纬度方向约 1.1 cm，不是米级取整。地图瓦片使用 GCJ-02 时，只在显示
和点击换算时做 WGS-84/GCJ-02 转换，文档和距离计算始终使用 WGS-84。

航点连线距离采用 Mission Planner 同类的 Haversine 公式，地球半径取 6371 km。
这与 MP 的 `PointLatLngAlt.GetDistance` 一致；QGC 使用 Qt
`QGeoCoordinate::distanceTo` 的大圆距离近似。两者对普通任务航段的差异远小于航线
精度本身，但 M-GCS 的默认显示值以 MP 公式为准。

`.waypoints` 和 `.plan` 都不作为 M-GCS 的内部主状态。保存草稿必须使用任务文档 JSON；
文件格式只用于导入、导出和外部地面站交换，不能绕过 `MissionDocument -> MissionPlan`
编译链直接上传。

`mg_mission_doc_export_format/import_format` 当前支持两种格式：

- `waypoints`：MP/QGC 的 `QGC WPL 110` 文本。第 0 行为 home，任务项从 seq 1
  开始，lat/lon 为十进制度，alt 为米；读取端兼容注释和空格/制表符/逗号分隔。
  导入后的航点不能恢复为参数化测绘实体。
- `plan`：QGroundControl `.plan` JSON。当前映射 `mission.items` 中的
  `SimpleItem`，兼容当前 7 值 `params` 格式和旧版 `coordinate` 数组；`plannedHomePosition`
  映射为文档 home，`cruiseSpeed` 映射为默认速度，`firmwareType` / `vehicleType`
  映射为文档 metadata。

`.plan` 对复杂区段采用分级处理：Survey、CorridorScan 等复杂 `mission.items` 由
`MissionComplexItem` 正式模型保存 `id`、`item_type`、原始 JSON 和
`entity_insert_index`，再次导出 `.plan` 时按插入位置与普通航点交错放回；Rally
version 2 已提升为正式的 `rally_points` 模型；GeoFence 已提升为正式模型，支持
polygon、circle、inclusion 和 breach-return。ComplexItem 导入会产生“只读保留、
暂不可语义化编辑”的 warning，因为 M-GCS 暂时不能编辑其内部字段。

复杂项不参与编译。文档含 `complex_items` 时，`validate`、`compile`、上传和回读都会
返回 `complex_item.unsupported` 错误，强制用户在 QGroundControl 处理或移除复杂项，
避免静默上传一份缺少 Survey/Corridor 的任务。

GeoFence 与 Rally 是独立的 MAVLink mission type，不计入航点任务上传，也不会因为存在
围栏/Rally 而阻断上传。`validate` 会检查其几何：多边形至少 3 个顶点、经纬度合法、
圆半径为正值、breach-return 点位于有效范围，问题分别以 `geo_fence.*` / `rally.*`
错误码返回。

参数化测绘实体或禁用实体仍不能无损导出为普通 `.plan`；`hoverSpeed`、地形高度模式、
null 参数和不在命令目录中的 MAV_CMD 会通过 `warnings` 报告。围栏和 Rally 的统一编辑
文档仍未实现。

完整字段映射和往返保证见 [QGC `.plan` 任务文件互操作](QGC_PLAN_INTERCHANGE.md)。

## FFI 入口

优先入口：`mg_mission_doc_create/get/apply/validate/compile/estimate/upload/verify`、
`mg_mission_doc_import_from_vehicle` 和 `mg_mission_get_command_catalog`。
旧 `mg_mission_*` 直接读改 `MissionPlan` 的兼容入口不用于维护编辑草稿。
