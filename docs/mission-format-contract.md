# 任务文件格式契约

状态：当前事实
基准：2026-10-11
相关：`docs/adr/0003-mission-file-format-strategy.md`、`docs/constraints/safety.md`（S7）

本文说明 `MissionDocument`、`.waypoints` 与 QGC `.plan` 三种表示之间的单位、版本与
round-trip 规则。目标是：导入 → 编辑 → 导出不丢用户数据，也不能把不支持的字段
静默改写成另一个含义。

## 1. 唯一内部主状态：MissionDocument 原生 JSON

- 权威类型：`core/include/core/mission/cad/mission_document.h` 的 `MissionDocument`。
- 序列化：`serialize_mission_document` / `deserialize_mission_document`
  （`core/src/mission/cad/mission_document.cc`）。
- `schema_version` 由 `kMissionDocumentSchemaVersion` 定义，当前为 `1`。
  反序列化对 `schema_version <= 0` 或 `> 当前值` 一律拒绝，报
  `unsupported mission document schema version`。
- 版本策略：字段新增保持向后兼容（旧读取器忽略新键，新读取器给出默认值）。
  语义不兼容的改动必须提升 `kMissionDocumentSchemaVersion`，并提供显式迁移，
  不允许静默降级或猜测旧语义。

单位与坐标系：

| 量 | 单位 / 取值 |
| --- | --- |
| 经纬度 | 十进制度（`double`），无位置时为 JSON `null` |
| 高度 | 米（`float`，`alt_m`） |
| 速度 | 米/秒（`speed_ms`） |
| 距离 / 尺寸 | 米 |
| 角度 | 度 |
| 时间戳 | Unix 毫秒（`updated_at_ms`，`int64`） |
| `frame` | MAVLink `MAV_FRAME`，导入时把 `_INT` 系列归一：5→0、6→3、11→10 |

`defaults.frame` 决定文档层默认高度基准，每个实体的 `frame` 覆盖它。任何导出格式都
必须显式写 frame，不能依赖接收方的默认值。

保留未知数据：

- `MissionDocument::extensions`：`key -> 原始 JSON` 的映射，用于整体性的格式片段。
- `MissionEntity::extensions_json`：单个任务项上 M-GCS 没有原生建模的字段，按紧凑
  JSON 对象原文保存，导出时逐字回填。

## 2. 交换格式 .waypoints

- 生成/解析：`core/src/mission/cad/mission_waypoints.cc`。
- 表达不了 Survey / Corridor / GeoFence / Rally 等结构化任务；遇到时按 ADR-0003
  推荐 `.plan` 或明确警告，绝不静默丢弃。
- 该格式没有可携带未知字段的位置，因此不提供 round-trip 保留承诺。

## 3. 交换格式 QGC .plan

- 生成/解析：`core/src/mission/cad/mission_qgc_plan.cc`。
- 导出写 `fileType="Plan"`、`groundStation="M-GCS"`、根 `version=1`、`mission.version=2`
  （QGC 现行主版本）。
- 导入接受任意 `mission.version`：非 2 时给出警告并按版本 2 解析，不失败。
  `firmwareType` 是必需字段，缺失即报错。

M-GCS 原生建模、导出时重新生成的键（不参与保留）：

- 根：`fileType`、`groundStation`、`version`、`mission`、`geoFence`、`rallyPoints`
- `mission`：`version`、`firmwareType`、`vehicleType`、`cruiseSpeed`、
  `plannedHomePosition`、`items`
- 任务项：`type`、`frame`、`command`、`autoContinue`、`params`、`doJumpId`

（三张已知键表见 `core/src/mission/cad/mission_qgc_plan.cc:75-85` 的
`kSimpleItemKnownKeys`/`kMissionKnownKeys`/`kPlanRootKnownKeys`，导入时由
`collect_extra_fields()`（定义 `:99`，模板签名 `:98`）按表取补集，导出时由
`merge_object_fields()`（`:123`）回填。）

**关于 `coordinate`：** 它在 `kSimpleItemKnownKeys` 里（`core/src/mission/cad/mission_qgc_plan.cc:77`），
所以**不会**被 `collect_extra_fields()` 收进 `extensions_json` 走原文保留通道；但它是**只读键**——
M-GCS 从它取值，从不写回它。

- **导入时是 lat/lon/alt 的读取来源之一**：`:252-262` 与 `:1193-1203` 先读 `params`（`:240-251`/`:1181-1191`），
  再被随后读到的 `coordinate` 覆盖；两者都只在「全局帧 + 该命令承载坐标」时生效
  （`is_global_coordinate_frame` 与 `mission_command_uses_location` 双条件）。
- **导出从不写它**：SimpleItem 导出只写 `params`（`simple_item_to_qgc_json` `:316-344`、
  实体导出 `:937-965`），两处都没有 `AddItemToObject(..., "coordinate")`。全文对 `coordinate` 的
  写入都是 `cJSON_AddItemToArray(coordinate, ...)`——即把值**追加进数组元素**，而不是把该键挂到
  条目对象上；读取则一律是 `cJSON_GetArrayItem(coordinate, ...)`。二者方向相反，互不相干。
- **因此它不在上方 :59-64 那份「导出时重新生成」的清单里**：那份清单是「会被重新生成写回的键」，
  而 `coordinate` 属于「不被重新生成」的一侧。它与下面 :87 起那批键的差别在于路径不同、
  结果也不同——extensions 里的键导入后被 `merge_object_fields()` **原样写回**（`:962`/`:988`/`:992`）；
  `coordinate` 因为在已知键表里而**根本不进** extensions 通道，于是导出时无人写回它。

除上述键以外的字段按原样保留：

- 任务项：存进 `MissionEntity::extensions_json`，例如 `altitudeMode`、`altitude`、
  `AMSLAltAboveTerrain` 以及第三方扩展键；导出时回填到对应 SimpleItem。
- `mission` 对象：存进 `extensions["qgc_plan.mission_fields"]`，例如 `hoverSpeed`、
  `globalPlanAltitudeMode`。**导出顺序是先无条件写默认值、再被导入原值覆盖**：导出先把
  `cruiseSpeed` 与 `hoverSpeed` 都写成 `defaults.speed_ms`（`:836-837`），随后
  `merge_object_fields()`（`:992`）把 `qgc_plan.mission_fields` 里的原值逐个
  `cJSON_DeleteItemFromObjectCaseSensitive` 后写回，所以导入过的 `hoverSpeed` 会以原值
  胜出、每个键只出现一次；**只有文档里没有该键时才留下 `defaults.speed_ms`**。
  （不是「缺省才补齐」——默认值总是先写，只是随后被覆盖。）
- 顶层：存进 `extensions["qgc_plan.root_fields"]`，导出时同样由 `merge_object_fields()`
  （`:988`）覆盖写回。

ComplexItem：

- 导入时 `type != "SimpleItem"` 的条目整体保存为 `MissionComplexItem`
  （`json` 原文 + `entity_insert_index` 位置），不解析几何。
- 导出时按位置插回 `mission.items`，`doJumpId` 重新编号。
- 上传/编译时用内部自带的 `TransectStyleComplexItem.Items` 展开，不重算几何。

## 4. Round-trip 保证与验证

以下路径由测试钉住，改动格式相关代码时必须保持通过：

- 原生 round-trip：`core/tests/test_mission_document.cc` 的
  `test_document_roundtrip`、`test_mission_format_recommendation`。
- QGC 未知字段 round-trip：`test_qgc_plan_preserves_unknown_fields`
  （未知任务项字段、`qgc_plan.mission_fields`、`qgc_plan.root_fields`、
  `hoverSpeed` 只出现一次、非 2 版本只告警不失败）。
- ComplexItem：`test_qgc_complex_item_export`、`test_complex_item_expansion`。
- 前端模型透传：`flutter_shell/test/mission_document_controller_test.dart` 的
  "mission document models keep opaque QGC fields on round-trip"。

前端 `MissionDocument` / `MissionEntity` 模型必须保留 `complex_items`、`extensions`
与实体级 `extensions`，否则 `replace` / `update` 操作会把 QGC 导入的数据丢掉。
