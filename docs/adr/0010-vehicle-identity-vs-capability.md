# ADR-0010: 机型身份与能力分层，能力按运行时求值

> 历史记录：本文记录特定时间点的架构决策。当前行为以代码、测试和根目录 `AGENTS.md` 为准。

**状态**: Accepted
**日期**: 2026-10-13
**作者**: Architecture Team

## 问题

ArduPilot 的 QuadPlane（四旋翼固定翼混合机型）在上报心跳时使用 `MAV_TYPE_FIXED_WING`（1），而不是任何 `MAV_TYPE_VTOL_*` 值。这是上游事实（`ArduPlane/quadplane.cpp:4042-4049` 的 `QuadPlane::get_mav_type()` 在 `Q_MAV_TYPE` 为 0 时返回 `MAV_TYPE_FIXED_WING`，而 `Q_MAV_TYPE` 默认为 0）。

这导致三个层面的问题，性质各不相同：

1. **身份判定错误（bug）**：VTOL 家族在 MAVLink 枚举中是 19–25（含 `MAV_TYPE_VTOL_RESERVED5` = 25），但代码里六处硬编码写成 `19..24`，且 `classify_ap_vehicle()` 的 `default` 分支把 `MAV_TYPE >= 26` 归为 `Copter`，使 GIMBAL(26)/ADSB(27)/PARAFOIL(28) 这类非飞行器拿到 `CapTakeoff | CapGuidedLand`。QGC 在同样结构的 switch 里 `default` 返回 `VehicleClassGeneric`，不给任何能力。

2. **缺少 QuadPlane 的识别途径（能力缺口）**：只靠 `MAV_TYPE` 无法识别 QuadPlane，必须有第二个信息源。Ground station 上游都这么做——QGC 用逐任务项的 `vtolMode` 轴（`MissionCommandTree.cc:84-92,118-122`），MP 用每次访问现算的 `isQuadPlane` getter（`GCSViews/InitialSetup.cs:94-101`）。

3. **固件参数名散落在调用点（S6 违反）**：`Q_ENABLE` 这个字符串在 `bridge/ffi/` 下被手写了四遍（`ffi_commands.cc`、`ffi_firmware.cc`、`ffi_internal.h`、`ffi_mission_catalog.cc`），而该层没有资格知道固件参数名。

另外存在一个循环依赖：`mg_config_get_param_roles` → `ffi_param_profile` → `ffi_ardupilot_vehicle_class` → 读 `Q_ENABLE`。即后端查自己的角色表之前要先读 `Q_ENABLE`，而它正是要问角色表「`Q_ENABLE` 叫什么」。角色表因此不能作为这个参数的来源。且 `plane` 与 `vtol` 两个 profile 的角色集逐字节相同（各 103 个角色），只有 `plane` 定义了 `frame.quadplane_enable`，所以读角色表永远无法区分二者。

## 方案选项

### A. 让命令目录依赖参数推导出的机型，并在参数到达时刷新

- ✅ 目录精确反映机型
- ❌ 命令目录每会话只取一次（`MissionDocumentController.initialize()` 的 `initialized` 门控），参数到达前挂载就会把错误结果锁死整个会话
- ❌ 需要新增刷新机制，且刷新时机难以界定

### B. 目录对固定翼一律放行 VTOL 命令

- ✅ 不会锁死
- ❌ 对真正的固定翼暴露不该有的命令，是「用错误换稳定」

### C. 身份与能力分层：身份来自 `MAV_TYPE`，能力是运行时求值的纯函数

- ✅ 与 QGC/MP 的实际做法一致
- ✅ 目录只依赖 `MAV_TYPE`（第一条心跳起就稳定），从设计上消除锁死，不需要刷新机制
- ✅ QuadPlane 识别有明确的归属层（插件），S6 由构造保证
- ❌ 引入了「身份」与「能力」两个概念，需要使用者区分

## 决策

**选择方案 C。**

1. **身份（identity）** 只由 `MAV_TYPE` 决定，是一个**谓词**而非区间字面量。收敛到单一定义，使用官方 `MAV_TYPE_VTOL_*` 符号，覆盖 19–25。`default` 必须返回「未知」，**不得**回退到 `Copter`——未识别机型不声明任何飞行动作能力。

2. **能力（capability）** 是对「当前连接状态」求值的纯函数，**不是连接时固化的快照**。同一机型在参数就绪前后求值不同是允许的，因为下一次查询会得到正确结果。禁止把能力结果缓存成会话级字段。

3. **固件参数名只由固件插件持有**。QuadPlane 的判定（读 `Q_ENABLE`）由 `IFirmwarePlugin` 的实现承担，对通用代码暴露一个语义化接口。`bridge/ffi` 与 `core` 通用代码中不得出现 `Q_ENABLE` 等固件参数名字面量（S6）。

4. **命令目录只按 `MAV_TYPE` 判定适用性**，不依赖参数推导出的机型。未归一化/未知机型保持放行（与 `mission_command_supports_vehicle` 现有的空串语义一致）。

5. **「当前是否处于 VTOL 形态」是运行时状态**，由 `MAV_VTOL_STATE`（`mg_vehicle_get_extended_sys_state` 的 `vtol_state`）表达，与机型身份分离——沿用 QGC 把 VTOL 模式作为任务内演进状态的做法。

## 后果

**正面**

- QuadPlane 能被识别，引导起飞/降落能力正确下发。
- 非飞行器的 `MAV_TYPE`（≥26）不再获得飞行动作能力，消除一条 S1–S3 关注的隐患路径。
- `Q_ENABLE` 有唯一归属，参数改名只需改插件一处。
- 命令目录不再有「参数未到 → 整个会话错误」的时序缺陷，无需刷新机制。
- `MAV_TYPE` 区间只有一处定义，不再有六处各自漂移。

**负面 / 代价**

- 「身份」与「能力」的区分需要被后续维护者理解；混用会导致把能力当身份缓存。
- 能力按请求求值意味着对同一问题可能得到不同答案（参数就绪前后），调用方必须能接受这一点。
- `IFirmwarePlugin` 增加接口方法，属架构变更（A6），未来固件插件需各自实现。

**中立**

- `plane` 与 `vtol` 两个参数角色 profile 内容完全相同的问题依然存在，但不再是正确性的依赖点。

## 相关

- 约束：[architecture.md](../constraints/architecture.md) A1–A7、[safety.md](../constraints/safety.md) S6
- 上游：`QGCMAVLink.cc:188-222`（`vehicleClass`，含 19–25 与 `default → Generic`）、`MissionCommandTree.cc:84-92,118-122`（`vtolMode`）、`APMFirmwarePlugin.cc:565-585`（VTOL 命令集）、`MissionPlanner/GCSViews/InitialSetup.cs:94-101`（`isQuadPlane` 活 getter）
- 代码：`core/src/firmware/firmware_adapter.cc`、`core/src/firmware/firmware_plugin_ardupilot.cc`、`core/include/core/firmware/firmware_plugin.h`、`bridge/ffi/ffi_internal.h`
- 已知偏差：KD-64、KD-65
