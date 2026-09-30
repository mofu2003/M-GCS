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
编辑 -> validate -> compile -> upload -> 轮询 progress
读取 -> mission_download -> 轮询 progress -> import_from_vehicle
验证 -> mission_doc_verify
```

上传的单次返回只表示传输已启动，必须轮询 `mg_mission_get_progress` 到完成。读取任务
使用 `mg_mission_doc_import_from_vehicle`，要求一次完整下载已成功完成；调用方应在覆盖
本地任务前确认。导入会保留 command/frame/参数/位置，但不能还原参数化测绘意图。
飞控回读的 `_INT` frame 5/6/11 会归一化为文档使用的 0/3/10。

`mg_mission_doc_verify` 只接受完成后的全量下载结果，比较数量、command、frame、位置、
高度、参数、autocontinue 和 mission_type。

## 文件互操作

`mg_mission_doc_export_format/import_format` 当前支持 `waypoints`，即 MP/QGC 的
`QGC WPL 110` 文本：第 0 行为 home，任务项从 seq 1 开始，lat/lon 为十进制度，alt 为米。
读取端兼容注释和空格/制表符/逗号分隔。导入后的航点不能恢复为参数化测绘实体。

QGC `.plan`、地理围栏和 Rally 的统一文档尚未实现。

## FFI 入口

优先入口：`mg_mission_doc_create/get/apply/validate/compile/estimate/upload/verify`、
`mg_mission_doc_import_from_vehicle` 和 `mg_mission_get_command_catalog`。
旧 `mg_mission_*` 直接读改 `MissionPlan` 的兼容入口不用于维护编辑草稿。
