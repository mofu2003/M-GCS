# 任务命令能力表（机型 × 命令）

状态：当前事实
基准：2026-10-11
相关：`docs/mission-format-contract.md`、`docs/constraints/safety.md`（S6/S7）、
`docs/plans/mgcs-development-plan-2026-10-10.md`（阶段 2 工作包 2）

本文说明 M-GCS 任务规划里「哪条 `MAV_CMD` 在哪些机型上真正会被执行」，
以及这个判断在导入、编辑、上传三处如何生效。目的是：不支持的条目在上传前被
拒绝并解释原因，而不是静默降级成另一条命令，或让飞控自己忽略造成「任务看起来
上传成功但少执行一条」。

这份表是 ArduPilot 的能力表。PX4 或未声明 `firmware_profile` 的文档不做机型
判断（见第 5 节）。

## 1. 权威数据的位置

- 类型与命名空间：`core/include/core/mission/cad/mission_command_catalog.h`
- 表本体：`core/src/mission/cad/mission_command_catalog.cc`（`build_catalog()`）
- 判断逻辑：`mission_command_supports_vehicle()`（命令级）、
  `mission_command_parameter_applies()`（参数级），同头文件声明，cc 内实现
- 校验/门禁：`core/src/mission/cad/mission_validation.cc`
- FFI 暴露：`bridge/ffi/ffi_mission_catalog.cc`（每条命令 JSON 带 `vehicles` 数组）
- 前端过滤：`flutter_shell/lib/mission/mission_document_models.dart`
  （`filterCommandCatalogForVehicle()`）

## 2. 机型类别与 bit mask

`MissionCommandDescriptor::vehicles` 是一个 bit mask，取值来自
`namespace mission_vehicle`：

| 名称 | bit | 运行时字符串（`ffi_param_profile`） | 文档 metadata 常见写法 |
| --- | --- | --- | --- |
| `kCopter` | `1<<0` | `copter` | `copter` / `multirotor` |
| `kPlane` | `1<<1` | `plane` | `plane` / `fixedwing` |
| `kVtol` | `1<<2` | `vtol` | `vtol` |
| `kRover` | `1<<3` | `rover` | `rover` |
| `kSub` | `1<<4` | `sub` | —（未建模） |
| `kTracker` | `1<<5` | `tracker` | —（未建模） |

`mission_vehicle_class_bit()` 负责把字符串（含 `fixedwing`→plane、
`multirotor`→copter 等别名）归一到 bit；未知或空名返回 `0`。

判定规则（`mission_command_supports_vehicle`）：

- `vehicles == 0`（未标注）→ 返回 **true**，不拦。
- 命令不在目录里 → 返回 **true**（是否支持该命令由
  `entity.command_unsupported` 单独判定）。
- 机型名空、无法归一，或归一到 **Sub/Tracker**（表里没有任何命令引用它们的位）
  → 返回 **true**。没有数据就不拦，宁可放过也不误拒一条本来能跑的任务。
- 其余情况：仅当 `vehicles` 含该机型 bit 时返回 true。

## 3. 命令 × 机型矩阵

目录共 13 条命令（下表 16–206 这 13 行；`NAV_LOITER_UNLIM`(17) 未单列，其无限圈语义由飞控按 `NAV_LOITER_TURNS` 处理）。`●` = 该机型执行；空白 = 不执行，上传前会报
`command_vehicle_unsupported`。

| MAV_CMD | 名称 | Copter | Plane | VTOL | Rover | 参考实现 |
| --- | --- | :-: | :-: | :-: | :-: | --- |
| 16 | `NAV_WAYPOINT` | ● | ● | ● | ● | `ArduCopter/mode_auto.cpp:720`、`ArduPlane/commands_logic.cpp:55`、`Rover/mode_auto.cpp:516` |
| 18 | `NAV_LOITER_TURNS` | ● | ● | ● | ● | `ArduCopter/mode_auto.cpp:734`、`ArduPlane/commands_logic.cpp:73`、`Rover/mode_auto.cpp:527` |
| 19 | `NAV_LOITER_TIME` | ● | ● | ● | ● | `ArduCopter/mode_auto.cpp:738`、`ArduPlane/commands_logic.cpp:77`、`Rover/mode_auto.cpp:524` |
| 20 | `NAV_RETURN_TO_LAUNCH` | ● | ● | ● | ● | 各机型 `mode_auto`/`commands_logic` 均处理 |
| 21 | `NAV_LAND` | ● | ● | ● | | `ArduCopter/mode_auto.cpp:726`、`ArduPlane/commands_logic.cpp:59`；Rover 的 `start_command` 无 NAV_LAND |
| 22 | `NAV_TAKEOFF` | ● | ● | ● | | `ArduCopter/mode_auto.cpp:716`、`ArduPlane/commands_logic.cpp:45`；Rover 无 |
| 84 | `NAV_VTOL_TAKEOFF` | | | ● | | 仅 quadplane 执行；`ArduPlane/commands_logic.cpp` |
| 85 | `NAV_VTOL_LAND` | | | ● | | `ArduPlane/commands_logic.cpp:102`/`:281`，仅 quadplane |
| 112 | `CONDITION_DELAY` | ● | ● | ● | ● | `ArduCopter/mode_auto.cpp:783`、`ArduPlane/commands_logic.cpp:119`、`Rover/mode_auto.cpp:549` |
| 114 | `CONDITION_DISTANCE` | ● | ● | ● | ● | `ArduCopter/mode_auto.cpp:787`、`ArduPlane/commands_logic.cpp:123`、`Rover/mode_auto.cpp:553` |
| 178 | `DO_CHANGE_SPEED` | ● | ● | ● | ● | `ArduCopter/mode_auto.cpp:798`、`ArduPlane/commands_logic.cpp:129`、`Rover/mode_auto.cpp:558` |
| 183 | `DO_SET_SERVO` | ● | ● | ● | ● | 共享实现 `libraries/AP_Mission/AP_Mission_Commands.cpp:76` |
| 206 | `DO_SET_CAM_TRIGG_DIST` | ● | ● | ● | ● | 共享实现 `libraries/AP_Mission/AP_Mission_Commands.cpp:137`，各机型启用 `AP_Camera` 即可 |

注：84/85 的参数按官方 MAVLink `common.xml` 定义，不是 M-GCS 自造字段。
Copter 收到 84 会当普通起飞处理，但 QGC 同样只对 VTOL 类开放
（`MissionController.cc`：`TakeoffMissionItem(vtol ? 84 : 22)`），M-GCS 对齐这一行为。

## 4. 参数级机型适用性

命令级判断回答「这条命令在这台飞机上会不会执行」；参数级判断回答「这条命令的这
个字段飞控到底存不存」。两者用同一套机型字符串（`ffi_param_profile`），参数位
单独标注在 `MissionCommandParameter::vehicles`，判定入口是
`mission_command_parameter_applies(command, parameter_index, vehicle_class)`。

ArduPilot 把 `MISSION_ITEM_INT.p1` 当成 16 位打包字段，装不下「停留时间 + 接受
半径 + 通过半径」，所以按构建类型二选一（`libraries/AP_Mission/AP_Mission.cpp`
的 `mavlink_int_to_mission_cmd()` `:1092-1116`、`mission_cmd_to_mavlink_int()`
`:1649-1659`）：

| 构建 | `p1` 里存 | 不存 |
| --- | --- | --- |
| 固定翼（含 VTOL/quadplane） | 接受半径 `param2`、通过半径 `param3` | 停留时间 `param1`、偏航 `param4` |
| 其它（Copter/Rover/Sub） | 停留时间 `param1` | 接受/通过半径（由 `WP_RADIUS`/`WP_PASS_RADIUS` 等机型参数决定）、偏航 `param4` |

对 `NAV_WAYPOINT`(16)：

| 参数 | 含义 | 适用范围 |
| --- | --- | --- |
| `param1` | 停留时间（秒） | Copter、Rover |
| `param2` | 接受半径（米） | Plane、VTOL |
| `param3` | 通过半径（米） | Plane、VTOL |
| `param4` | 偏航（度） | `kArduPilotNone`：ArduPilot 任何机型都不存，偏航由 `WP_YAW_BEHAVIOR` 决定 |

判定规则与命令级一致：参数未标注、命令不在目录、机型名无法归一（含 PX4/未知）
→ 返回 `true`，不判。`mg_mission_doc_verify` 用它把「飞控不存这个字段」记进
`normalizations` 而不是 `differences`，避免把 ArduPilot 的正常存储行为当成上传
错误。

证据（`docs/sitl-scenario-evidence.md`、`docs/sitl-scenario-evidence-plane.md`）：

- `arducopter --model quad`：回读 `param2=0`，首轮核验 `normalization_count=3`
  （三条航点的接受半径字段被飞控丢弃归一），`difference_count=0`、`matches=true`。
- `arduplane --model plane`：回读 `param2=2`，首轮核验 `normalization_count=0`，
  说明固定翼确实存了接受半径字段。

与 QGC 对照：QGC 用 `MavCmdInfo*.json` 的 `paramRemove` 在显示/发送层面去掉字段
（`MavCmdInfoFixedWing.json` 给 id 16 写 `"paramRemove": "1,4"`），
`MavCmdInfoCommon.json` 里 16 的 `param2`/`param3` 是可选字段。M-GCS 对
ArduPilot 保留四个字段，但在回读核验时按机型解释差异，效果相同且不丢用户输入；
PX4 则在编辑目录里直接隐藏 `paramRemove` 列出的字段（见下）。

**固件级字段可见性（S4，2026-10-11）**：机型级 `vehicles` 之上的第二个维度是固件。
`mission_command_parameter_visible(command, parameter_index, firmware_profile)` 按
QGC `PX4-MavCmdInfoCommon.json` 的 `paramRemove` 决定某字段是否展示给用户：
PX4 下 `NAV_WAYPOINT`(16) 隐藏 `param2`/`param3`、`NAV_LAND`(21) 隐藏 `param1`、
`VIDEO_START_CAPTURE`(2500) 隐藏 `param2`；PX4 机型级覆盖文件为空，因此不需要
机型细分。`mg_mission_command_catalog` 在输出目录时跳过不可见字段，并给每条命令
附 `vehicles` 数组供前端与机型过滤共用。与 QGC 一致，这**只影响前端是否显示**，
不改变已经存储的值，也不参与上传前核验的门禁。`mission_command_parameter_applies()`
（机型级、解释回读差异）与它职责不同，两者不要混用。

`kArduPilotNone`（`1<<15`）只用于参数级标注「这个字段 ArduPilot 任何机型都不
存」，不要用在命令级 `vehicles` 上：命令级遇到未建模机型会走「不判」分支，语义
不同。

## 5. 判断在三处生效

1. **导入/编译校验**（`mission_validation.cc`）：文档声明了 `vehicle_type` 且
   `firmware_profile` 为 `ardupilot`（或空）时，主实体与 complex item 都按机型检查；
   不支持的产出 `entity.command_vehicle_unsupported` /
   `complex_item.command_vehicle_unsupported`（Error），编译不通过。
2. **上传运行时门禁**（`bridge/ffi/ffi_mission_document.cc`，`mg_mission_doc_upload`）：
   编译通过后、真正发给飞控前，用**已连接飞机**的参数档案
   （`ffi_param_profile`）再过一次 `validate_plan_for_vehicle()`，产出
   `mission.command_vehicle_unsupported`。这是防止「文档作者声明的机型」与
   「实际连上的机型」不一致的最后一道闸。
3. **前端下拉过滤**（`filterCommandCatalogForVehicle()`）：任务编辑面板按当前连接
   机型隐藏不适用命令，减少用户先选中再被拒绝的来回。规则与后端一致——`vehicles`
   为空保留、机型无法识别不过滤、正在编辑的实体命令强制保留
   （`keepCommands`），避免打开旧任务时把已有条目改掉。**2026-10-11 修正**：
   `mg_mission_get_command_catalog` 此前从未输出 `vehicles` 字段，Dart 模型读到的一直是
   空数组，所以这一层过滤对任何机型都不生效；现在 `vehicles` 随目录下发。

前端过滤只是体验优化，不做安全保证；真正的拒绝由第 1、2 步负责。

## 6. 未建模机型与固件

- **Sub / AntennaTracker**：`vehicles` 位已定义但目录里没有任何命令引用，
  因此它们走「没有数据就不拦」分支。这不是说这些机型已被支持，而是不用
  ArduPilot 的多旋翼/固定翼假设去误拒它们的任务。后续为 Sub/Tracker 建表时，
  需按 `ArduSub`、`AntennaTracker` 的 `mode_auto` 逐条补 `vehicles`。
- **PX4**：`capability_table_applies()` 只在 `firmware_profile` 为空或
  `ardupilot` 时判机型。PX4 任务完全跳过这张**机型**表，避免用 ArduPilot 能力误拒。
  PX4 另有两条独立规则：（1）字段可见性按 `paramRemove` 处理（见 §4，只影响展示）；
  （2）`DO_LAND_START`(189) 在 PX4 下不承载坐标（`mission_command_uses_location`
  的 profile 重载），因为 QGC 把 PX4 的 189 标成 `specifiesCoordinate:false`，
  而 ArduPilot `AP_Mission::stored_in_location()` 含 189。注意可编辑命令目录目前
  只有 14 条常用命令，**不含 189**，因此含 189 的文档会被 `validate_document()`
  判 `entity.command_unsupported`（与固件无关）；189 的固件差异只作用于导入/导出/
  序列化路径。

## 7. 与 QGroundControl 的对照

QGC 用「固件类 × 机型类」两级覆盖。参考源码在
`/Users/fansicheng/Projects/qgroundcontrol/src/MissionManager/`：

- `MissionCommandTree.h` 定义层级：Any/ArduPilot/PX4 × Any/FixedWing/MultiRotor/
  Rover/Sub/VTOL，子级覆盖父级。
- 命令元数据在 `MavCmdInfoCommon.json`、`MavCmdInfoFixedWing.json`、
  `MavCmdInfoMultiRotor.json`、`MavCmdInfoRover.json`、`MavCmdInfoSub.json`、
  `MavCmdInfoVTOL.json`。根键是 `mavCmdInfo` 数组。
- 实测各文件 id 覆盖：Common 覆盖绝大多数（含 16–25、81–85、112–115、
  176–185、200–212 等）；FixedWing 只覆盖 16/17/21/22；MultiRotor 覆盖
  17/18/19/22/31；Rover/Sub/VTOL 三个文件为空，靠 Common + 层级覆盖。

M-GCS 当前的差异：

- **粒度更粗**：M-GCS 是「命令 → 机型集合」一张平表，外加一个固件级
  `paramRemove` 覆盖（§4）；QGC 是固件×机型两级树，且每个机型文件还能覆盖参数
  默认值/枚举。M-GCS 只区分 ArduPilot 与「非 ArduPilot（不判）」，没有 PX4 的
  机型表——QGC 的 PX4 机型级 `MavCmdInfo*` 文件实测为空，因此当前不需要。
- **覆盖少**：M-GCS 只标注任务面板实际提供的 13 条命令；QGC 覆盖数百条
  `mavCmdInfo`。M-GCS 的定位是「只对已建模、已进 UI 的命令做机型判断」，
  未进 UI 的命令不在表内，也自然不会误报。
- **84/85 归属一致**：两边都只把它归给 VTOL 类，不归 Copter。
- **QGC 的 `MavCmdInfoCommon.json` 给 84/85 完整定义**（含
  `isTakeoffCommand`/`isLandCommand`、param2 Transition Heading 枚举、
  param3 Approach Alt），M-GCS 的 84/85 参数与之一致。
- **189 的坐标语义按固件区分**：QGC 的 ArduPilot `MavCmdInfo*.json` 里没有 189
  （走 MAVLink 默认，不带坐标），PX4 的 `PX4-MavCmdInfoCommon.json` 明确写
  `specifiesCoordinate:false`；ArduPilot 固件却把它存进 location。M-GCS 用
  `mission_command_uses_location(command, firmware_profile)` 表达这一差异，而不是
  把它塞进机型表。

## 8. 已知限制

- **`.plan` 导出机型**：`core/src/mission/cad/mission_qgc_plan.cc` 导出时只接受
  `copter` / `plane` / `fixedwing` / `vtol`，**`rover` 尚未支持**。Rover 文档
  可以规划、校验、上传，但导出为标准 QGC `.plan` 时会被拒绝。补法是按 QGC
  `.plan` 的 `vehicleType` 约定加 `rover` 分支，属于后续工作。
- **表内容靠人工维护**：`vehicles` 标注来自 ArduPilot 源码阅读，不是运行时探测。
  ArduPilot 新增/改动命令时需同步更新本表与测试。
- **只覆盖 ArduPilot 机型**：机型级 `vehicles` 只对 ArduPilot 生效；PX4 不做机型
  判断。PX4 侧已落地的是固件级 `paramRemove` 字段可见性与 189 坐标语义（S4，
  `docs/features/px4-parity.md`），PX4 机型表按 QGC 事实（PX4 机型级
  `MavCmdInfo*` 为空）暂不建设。

## 9. 如何新增一条命令

1. 在 `build_catalog()` 里加 descriptor，填 `command` / `name` / `label` /
   `entity_type` / 参数，并按 ArduPilot 源码确认命令级 `vehicles`；参数若像
   `NAV_WAYPOINT` 那样按飞控构建被丢弃，再补参数级 `vehicles`。
2. 在 `core/tests/test_mission_document.cc` 的
   `test_mission_vehicle_capability()` 里补该命令的机型断言。
3. 若命令进前端下拉，确认 `filterCommandCatalogForVehicle()` 无需改动（它按
   `vehicles` 通用过滤）。
4. `cmake --build build_check && ./build_check/test_mission_document` 与
   `flutter test` 通过后，跑 `tools/preflight.sh`。
