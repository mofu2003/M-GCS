# 任务规划后端契约

本文描述当前已经实现的 Mission Planning Engine。前端可以把它当作黑盒：
编辑本地文档、读取诊断、编译后显式上传，不与 MAVLink 消息或 `MissionPlan`
内部状态直接耦合。

## 两层模型

- `MissionDocument`：可编辑源文档，保存设计意图、实体顺序和 revision。
- `MissionPlan`：编译后的 MAVLink 任务，仅供传输和回读比对。

编辑、校验、保存和撤销永远不会写飞控。`mg_mission_doc_upload` 是唯一的写入入口。

## 文档 JSON

```json
{
  "schema_version": 1,
  "metadata": {
    "id": "mission-1",
    "name": "Area A",
    "description": "",
    "firmware_profile": "ardupilot",
    "vehicle_type": "copter"
  },
  "home": {
    "set": true,
    "lat": 39.9,
    "lon": 116.4,
    "alt_m": 20
  },
  "defaults": {
    "frame": 3,
    "altitude_m": 50,
    "speed_ms": 12
  },
  "revision": 4,
  "entities": [
    {
      "id": "takeoff-1",
      "type": "takeoff",
      "enabled": true,
      "order": 0,
      "command": 22,
      "frame": 3,
      "autocontinue": 1,
      "alt_m": 30,
      "params": [15, 0, 0, 0]
    }
  ]
}
```

实体公共字段：

| 字段 | 含义 |
| --- | --- |
| `id` | 稳定实体 ID，编辑和诊断都使用它，不使用数组下标作为身份 |
| `type` | UI 分类，例如 `waypoint`、`takeoff`、`land`、`rtl`、`loiter` |
| `enabled` | 编译时是否包含该实体 |
| `order` | 文档顺序，后端归一化为从零开始的连续值 |
| `command` | MAV_CMD 数值 |
| `frame` | MAV_FRAME 数值 |
| `autocontinue` | 是否自动继续 |
| `lat`、`lon` | WGS84 经纬度，非位置命令可省略 |
| `alt_m` | 高度，省略时使用文档默认高度 |
| `params` | 固定四项命令参数，含义由命令目录定义 |

## 编辑命令

`mg_mission_doc_apply` 每次处理一个 JSON 操作：

```json
{"operation":"add","expected_revision":1,"entity":{"type":"waypoint","command":16,"lat":39.9,"lon":116.4,"alt_m":50}}
```

支持的 `operation`：

- `add`：追加实体。
- `update`：按实体 `id` 更新。
- `remove`：按 `entity_id` 删除。
- `move`：按 `entity_id` 和 `to_index` 移动。
- `clear`：清空实体。
- `replace`：用 `document` 整体替换源文档。

除 `replace` 外的编辑不会上传飞控。`expected_revision` 不匹配时后端拒绝操作，
防止前端基于旧快照覆盖新编辑。

## 参数化测绘实体

测绘区域和走廊不是几十个手摆的航点，而是"设计意图"。文档里只存参数，
`mg_mission_doc_compile` 在编译期调用 `SurveyCalculator` 展开成具体航点，
所以前端不需要（也不允许）复制测绘几何。

```json
{
  "id": "survey-1",
  "type": "survey_grid",
  "lat": 39.9,
  "lon": 116.4,
  "survey": {
    "width_m": 200,
    "length_m": 300,
    "angle_deg": 30,
    "cross_grid": false,
    "alt_m": 80,
    "speed_ms": 8,
    "return_to_start": true,
    "forward_overlap_pct": 70,
    "side_overlap_pct": 60,
    "focal_length_mm": 35,
    "sensor_width_mm": 23.5,
    "sensor_height_mm": 15.6,
    "image_width_px": 6000,
    "image_height_px": 4000
  }
}
```

走廊实体用起点（实体 `lat` / `lon`）+ 终点（`survey.end_lat` / `survey.end_lon`），
宽度字段是 `survey.corridor_width_m`：

```json
{
  "id": "corridor-1",
  "type": "corridor",
  "lat": 39.9,
  "lon": 116.4,
  "survey": {
    "end_lat": 39.905,
    "end_lon": 116.4,
    "corridor_width_m": 60,
    "alt_m": 60,
    "speed_ms": 6
  }
}
```

要点：

- `type` 取 `survey_grid`（含 `cross_grid: true` 的交叉网格）或 `corridor`。
- 这两个实体的 `command` / `frame` / `params` 不参与编译，展开结果由算法决定；
  字段留默认值即可。
- 校验只检查参数自洽：经纬度有效、尺寸为正、高度与速度为正、重叠率在 `[0, 100)`、
  相机参数为正；走廊还要求起终点不相同。
- 编译后每个展开项在 `source_entity_ids` 里都指回同一个源实体，前端据此做
  "一个测绘块 ↔ 多个航点"的映射。
- `mg_mission_doc_estimate` 用展开结果统计距离、时间、航点数和照片数，
  不会只按一个实体计数。

## 命令目录


前端通过 `mg_mission_get_command_catalog` 获取：

- 命令名称、分类和说明。
- 是否需要位置和高度。
- 每个参数的 MAVLink 参数序号 `index`、名称、标签、单位、类型、默认值、
  范围、必填状态和枚举选项。`index` 直接对应 `params[index - 1]`。

当前目录覆盖航点、起飞、降落、RTL、悬停、速度、相机触发、舵机、延时和距离条件。
新增命令时只扩展后端目录和校验规则，前端不需要复制参数语义。

## 校验和编译

`mg_mission_doc_validate` 返回结构化诊断：

```json
{
  "success": true,
  "valid": false,
  "diagnostics": [
    {
      "severity": "error",
      "code": "entity.latitude",
      "message": "Mission latitude is invalid or missing",
      "entity_id": "waypoint-1",
      "field": "lat"
    }
  ]
}
```

`mg_mission_doc_compile` 在无 error 时返回：

- `plan`：编译后的 MAVLink 任务。
- `source_entity_ids`：每个编译任务的源实体映射。
- `diagnostics`：即使编译成功也保留 warning。

`mg_mission_doc_estimate` 返回总距离、预计时间、高度范围、照片数、航点数和命令数。

## 上传与回读

```text
mg_mission_doc_validate
    -> mg_mission_doc_compile
    -> mg_mission_doc_upload(expected_revision)
    -> mg_mission_download
    -> mg_mission_doc_verify(tolerance_m)
```

`mg_mission_doc_upload` 会重新校验并编译，拒绝无效文档和过期 revision。
返回中的 `document_item_count` 是用户可见任务项数量，`transport_item_count`
包含传输层为了 ArduPilot 兼容而注入的 home item。
`mg_mission_doc_verify` 比较飞控回读任务和本地编译任务，返回：

```json
{
  "success": true,
  "matches": false,
  "difference_count": 1,
  "expected_count": 3,
  "vehicle_count": 3,
  "differences": [
    {
      "index": 1,
      "field": "z",
      "expected": "50.000000",
      "actual": "65.000000"
    }
  ]
}
```

verify 只接受一次成功完成的全量任务下载结果。上传成功后必须先调用
`mg_mission_download` 并等待下载完成，否则返回 `invalid_state`，避免把上传缓存
误判为飞控回读结果。

ArduPilot 的 home item 注入仍由现有传输适配层处理，不进入任务规划文档。

## 当前边界

- 已实现单文档、本地编辑、命令目录、校验、编译、估算、上传和 verify。
- 当前只允许编译命令目录中已经定义语义的命令；未知命令会被校验拒绝。
- 测绘区域（`survey_grid`）和走廊（`corridor`）已作为参数化实体接入文档，编译期由
  `SurveyCalculator` 展开；环绕（orbit）仍用命令目录里的参数表达。

## 与旧任务的边界

- `MissionManager` 和 `mg_mission_add_item/update_item/remove_item/move_item`
  属于旧兼容路径，会直接读改写 `MissionPlan` 并立即上传飞控。
- 新前端不得用旧接口维护编辑草稿。编辑统一使用 `mg_mission_doc_apply`，
  上传统一使用 `mg_mission_doc_upload`。
- `SurveyCalculator` 是唯一的测绘几何实现，由 `survey_grid` / `corridor` 编译实体调用，
  前端不得复制它的测绘计算。
- QGC `.plan`、Mission Planner `.waypoints` 和地理围栏/Rally 的统一文档尚未实现。
