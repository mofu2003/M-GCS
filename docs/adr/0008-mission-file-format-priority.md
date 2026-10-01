# ADR 0008：任务文件格式按飞控生态分层

## 状态

已接受。2026-09-30。

## 背景

Mission Planner（MP）和 ArduPilot 生态主要使用 `.waypoints`，即 `QGC WPL 110` 文本。QGroundControl（QGC）和 PX4 生态主要使用 `.plan` JSON，后者可以保存 Home、固件/机型信息、Survey、GeoFence 和 Rally 等更丰富的规划数据。

M-GCS 同时支持 ArduPilot 和 PX4。如果只保留一种文件格式，会牺牲另一套生态的工作流；如果允许两种格式都可以作为内部主状态，又会导致保存、导出和上传语义混乱。

## 决策

### 内部事实来源

`MissionDocument` 的版本化 JSON 是 M-GCS 的唯一内部主状态。`.plan` 和 `.waypoints` 都只是导入/导出交换格式，不直接承担编辑草稿、revision、undo/redo 或参数化测绘意图。

### 对外格式优先级

| 场景 | 推荐格式 | 原因 |
| --- | --- | --- |
| ArduPilot + Mission Planner 工作流 | `.waypoints` | MP 主格式，简单、成熟、直接兼容 ArduPilot 任务上传 |
| PX4 + QGroundControl 工作流 | `.plan` | QGC 主格式，保留固件/机型、复杂任务和规划元数据 |
| 普通航点跨 MP/QGC 交换 | `.waypoints` | 两套工具都支持，作为最小公共格式 |
| 包含 Survey、Corridor、GeoFence、Rally 或需要保留 QGC 规划意图 | `.plan` | `.waypoints` 会展开或丢失复杂意图 |
| M-GCS 本地保存/自动恢复 | `MissionDocument` JSON | 无损失，保留 revision、稳定 ID 和领域模型 |

### 推荐规则

- 当前默认主推 ArduPilot / MP：新建任务的推荐导出格式是 `.waypoints`。
- 识别到 PX4 / QGC 工作流时，推荐格式切换为 `.plan`。
- 无法判断飞控或离线编辑时，默认推荐 `.waypoints`，但必须同时提供 `.plan`。
- 只要任务包含 `.waypoints` 不能无损表达的内容，推荐格式改为 `.plan`，并明确告知用户 `.waypoints` 会丢失哪些内容。
- M-GCS 不因导入格式自动改变飞控固件，也不因固件变化静默覆盖已有文件。
- 上传飞控始终从 `MissionDocument` 编译出 `MissionPlan`，不能直接把文件内容当成上传结果。

### 删除策略

不删除 `.plan`。它是 PX4/QGC 兼容、复杂规划意图保留和后续 GeoFence/Rally 支持的基础能力。删除 `.plan` 只能减少短期代码，不能解决长期互操作需求。

## 后果

- UI 必须区分“保存任务”“导出给 MP / ArduPilot”和“导出给 QGC / PX4”，不能只显示含糊的“保存”。
- 导出格式选择需要同时考虑 firmware profile、vehicle type、任务复杂度和用户选择。
- `.waypoints` 导出遇到 Survey/Corridor/GeoFence/Rally 时必须 warning 或拒绝，不能静默展开后覆盖原任务。
- `.plan` 导入把 QGC 复杂项保留为 M-GCS 领域模型（ComplexItem、GeoFence、Rally），
  不静默丢弃；ComplexItem 内部字段暂为只读，后续逐步语义化。
- 格式推荐逻辑和警告需要单元测试，至少覆盖 ArduPilot、PX4、未知固件、复杂测绘和简单航点五种场景。

## 实施项

1. 增加任务文件格式推荐接口或内部策略函数。
2. 前端保存/导出入口显示推荐格式和丢失数据警告。
3. 保持 `.waypoints` 与 `.plan` 两条导入/导出路径及其互操作测试。
4. GeoFence/Rally/ComplexItem 已提升为独立领域模型；ComplexItem 内部字段语义化仍待推进。
