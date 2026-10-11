# ADR-0007：PX4 参数元数据的来源、许可与降级策略

状态：已接受
日期：2026-10-11

## 背景

`core/src/config/ParameterDescriptors.json` 只有 ArduPilot 描述库
（`sources` 仅 `ardupilot_4_8`，`profiles` 覆盖 copter/plane/vtol）。PX4 车辆
连接后描述库查不到条目：改动前 `ffi_param_profile()` 对非 ArduPilot 固件返回空串，
参数描述库按「default」查表，绝大多数 PX4 参数名没有条目，参数页只能显示裸参数名，
`found=false`。

QGC 对 PX4 的处理不同：`FirmwarePlugin::_cachedParameterMetaDataFile()`
（`src/FirmwarePlugin/FirmwarePlugin.cc:572-600`）会优先使用**飞控通过
MAVLink COMPONENT_INFORMATION 下载的元数据**（`ComponentInformationManager`），
只有拿不到飞控元数据时才回落到随仓库分发的
`src/FirmwarePlugin/PX4/PX4ParameterFactMetaData.json`
（`PX4FirmwarePlugin.h:57`）。该文件是 QGC 用 PX4 上游生成的参数元数据快照，
顶层结构 `{parameters, translation, version:1}`，1836 个参数。

`docs/constraints/upstream.md` 的 P4 要求：引用上游代码或数据前必须记录所选
许可、再分发义务，并写入 ADR。

## 决定

1. **引入 PX4 参数元数据**，作为独立数据文件
   `core/src/config/PX4ParameterMetaData.json`，保持上游格式逐字不变（便于核对
   与重新生成），不与手工维护的 `ParameterDescriptors.json` 结构混在一起。
2. **来源与许可**：该 JSON 由 PX4-Autopilot（BSD-3-Clause，Copyright (c)
   2012-2025 PX4 Development Team）的参数定义生成；当前副本取自
   QGroundControl 仓库（Apache-2.0 / GPLv3 双许可，本仓库取 Apache-2.0 分支）
   的 `src/FirmwarePlugin/PX4/PX4ParameterFactMetaData.json`
   （QGC commit `bf0096d97`）。BSD-3-Clause 与 Apache-2.0 都允许在保留版权与
   许可声明的前提下再分发；M-GCS 保持 MIT，不引入 GPL 代码路径。许可原文与
   出处记录在 `core/src/config/PX4ParameterMetaData.LICENSE.txt`。
3. **加载与作用域**：由 `ParamDescriptors` 在加载主描述库后，从同一目录（或标准
   数据目录）读取该文件，解析到 profile `px4`。参数页查询路径用的
   `ffi_param_profile()` 对 PX4 返回 `px4`；任务命令能力门禁原先把同一个函数结果
   当成 ArduPilot 机型名复用，因此拆出 `ffi_ardupilot_vehicle_class()`——只认
   ArduPilot 机型，PX4 返回空串。两者职责分开后，参数元数据与任务字段存储语义
   不再互相牵制。
4. **不改飞控行为**：元数据只影响参数页的显示名、分组、描述、单位、枚举标签、
   默认值与只读/重启标记。写入前校验仍走同一 `validate()`；值为枚举的参数仍按
   数值类型（`float`/`int`）存储，`values[]` 只用于下拉候选与显示，不额外引入
   "必须在枚举表内"的写入约束。
5. **值域只取完整区间**：`ParamDescriptor` 没有单独的 has-min/has-max 位，而
   `validate()` 用 `min != max` 判断是否需要区间校验。因此只有快照同时给出
   `min` 与 `max` 的参数才落库区间；只给 `min` 的 146 个参数（例如
   `ASPD_FP_T_WINDOW`）不能写成 `max = 0`，否则大于 0 的合法值会被误判越界。
6. **快照 profile 不回落到 ArduPilot 默认描述**：PX4 的元数据是固件自带参数集的
   完整快照，不是"在 ArduPilot 基础上扩展"。`get_from_profile()` 对这类 profile
   （`standalone_profiles_`）在查不到条目时直接返回 `found=false`，避免把同名
   ArduPilot 参数（如 `BATT_MONITOR`）的显示名与选项套到 PX4 车辆上。
7. **诚实降级**：飞控未上报的参数、描述库未覆盖的参数，继续走现有
   `found=false` 路径（显示名=参数名、描述="No local metadata available"），
   `mg_config_get_metadata_coverage` 继续报告覆盖数。

## 已知限制

- 随仓库分发的是**快照**：所连 PX4 固件版本与快照不一致时，参数可能已改名、
  改义或删除。M-GCS 目前没有实现 COMPONENT_INFORMATION 下载，因此不能像 QGC
  那样用飞控自带元数据覆盖快照。该限制记录在 `docs/features/px4-parity.md`，
  并按方向 3 的后续切片评估。
- 上游 `bitmask` 与 `decimalPlaces` 字段暂不透传（M-GCS 描述结构没有对应位；
  位掩码参数按整数 min/max 呈现）。
- 只给 `min` 或只给 `max` 的参数不落库区间（原因见决定 5），因此这些参数在前端
  没有软性范围提示，写入仍由飞控自身校验。
- 文件是纯数据：其中不含 Qt 代码，也不链接 Qt，因此 ADR-0002 要求的 Qt 商业
  授权核对在此不适用；需要履行的只有 PX4（BSD-3-Clause）与 QGC 数据副本
  （Apache-2.0）的版权与许可声明保留义务。
- 更新快照是显式动作：重新从上游生成/复制后替换文件，并在 CHANGELOG 记录版本。

## 后果

- PX4 参数页获得与 QGC 同源的分组与描述，`G7`（PX4 参数元数据缺失）关闭。
- 新增 733 KB 生成数据与一次进程启动时的 JSON 解析（约 1836 条），只影响启动
  路径，不进入持锁路径。
- 必须维护许可声明文件与快照更新流程，避免数据来源不可追溯。

## 相关

- `docs/adr/0002-upstream-reference-and-license.md`
- `docs/constraints/upstream.md`（P3、P4）
- `docs/features/px4-parity.md`（S3）
- `core/src/config/PX4ParameterMetaData.LICENSE.txt`
- `docs/adr/0002-upstream-reference-and-license.md`（P4 的记录点）
