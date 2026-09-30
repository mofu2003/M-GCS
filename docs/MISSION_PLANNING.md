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

进度优先通过 `mg_operations_get_snapshot` 的 `mission` 项读取，状态为
`downloading`、`uploading`、`waiting_ack` 或 `idle`，并包含百分比、计数、错误和取消
终态。`mg_operations_cancel("mission")` 会终止当前任务传输、把状态标为
`MAV_MISSION_OPERATION_CANCELLED`，并向飞控发送对应的 `MISSION_ACK`；没有活动传输时
返回 `canceled:false`，不会把已经完成的传输追认为取消。

`mg_mission_doc_verify` 只接受完成后的全量下载结果，比较数量、command、frame、位置、
高度、参数、autocontinue 和 mission_type。

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

`.plan` 当前不会静默降级无法表达的内容：参数化测绘实体或禁用实体不能无损导出；
Survey、CorridorScan 等复杂 `mission.items`，以及非空 GeoFence / Rally 会在导入时返回
明确错误。可继续导入但无法完整保留的字段通过响应中的 `warnings` 返回，例如
`hoverSpeed`、地形高度模式、null 参数和不在命令目录中的 MAV_CMD。围栏和 Rally 的
统一编辑文档仍未实现。

完整字段映射和往返保证见 [QGC `.plan` 任务文件互操作](QGC_PLAN_INTERCHANGE.md)。

## FFI 入口

优先入口：`mg_mission_doc_create/get/apply/validate/compile/estimate/upload/verify`、
`mg_mission_doc_import_from_vehicle` 和 `mg_mission_get_command_catalog`。
旧 `mg_mission_*` 直接读改 `MissionPlan` 的兼容入口不用于维护编辑草稿。
