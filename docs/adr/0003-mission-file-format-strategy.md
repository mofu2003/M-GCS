# ADR-0003：任务文件格式策略

状态：已接受
日期：2026-10-09

## 背景

任务需要与 Mission Planner / QGroundControl 交换。两种格式的表达能力不同：`.waypoints` 无法无损表达 GeoFence、Rally 等复杂任务，`.plan` 可以。

## 决定

- `MissionDocument` 是唯一内部主状态。
- `.waypoints` 与 `.plan` 都是交换格式。
- 默认推荐 `.waypoints`（Mission Planner 工作流）；复杂任务推荐 `.plan`（QGroundControl 工作流）。
- `.waypoints` 无法表达的复杂任务，必须推荐 `.plan`，或明确警告。
- 不根据格式或飞控静默覆盖用户文件。上传飞控始终从 `MissionDocument` 编译 `MissionPlan`。

## 后果

- 格式转换需要显式提示用户，不能静默降级。
- 需要维护两种格式的导入导出测试。

## 相关

- `docs/constraints/safety.md` 的 S7
