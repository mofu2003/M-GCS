# 功能简报：PX4 固件支持对齐（PX4 parity）

状态：方向已立项（阶段 3 方向 3）。简报与上游研究完成；**S1（统一模式表）、S2（引导/起飞行为）、S3（PX4 参数元数据）、S4（任务命令覆盖）、S5（`mg_px4_*` 定性与缺陷修复）、S6（PX4 行为与错误路径测试）已实现并验证**，本方向验收项全部闭环。
日期：2026-10-11
来源：`docs/plans/mgcs-development-plan-2026-10-10.md` 阶段 3「能力扩展，逐项立项」第 3 条；`docs/capabilities.md` 对 `ffi_api_configuration.h` 的「PX4 安全/调参为 FFI 内部能力」说明。

本文件是阶段 3 的第三个方向。上游源码事实来自本地快照（QGC `bf0096d97`、MP `a2fcd74d6`，见 `docs/constraints/parity.md` 参考版本表）。M-GCS 事实来自当前工作树的只读核对，每条带 `file:line`。

> **进度（2026-10-11）**
> - **S1 已完成。** `px4_modes.h` 成为 PX4 模式唯一事实来源（`px4_mode_table`/`decode_px4_mode`/`encode_px4_custom_mode`/`encode_px4_mode_name`），`firmware_plugin_px4.cc` 删除自带 `kPX4Modes` 改为消费它；补齐 `ALTITUDE_CRUISE=11`、POSCTL `ORBIT/SLOW`、`AUTO_GUIDED_COURSE=19`、`VTOL_TAKEOFF=10`；`SIMPLE`/`TERMINATION` 进表且 `can_be_set=false`；原 RTGS（AUTO 子模式 7）标为保留、不可设置、不出现于模式表（解码走 `Mode(N)` 兜底）。
> - **命名决策（D2 相关）：** M-GCS 沿用既有约定，AUTO 子模式统一带 `"Auto "` 前缀（`Auto Loiter`/`Auto RTL`/`Auto Land`/`Auto Mission`/`Auto Follow`/`Auto Precision Land`），不采用 QGC 的 `Hold`/`Return` 简写，以免同时改动状态栏文案与前端依赖。差异仅限展示名，协议值与 `can_be_set` 与 QGC 一致。
> - **两张表分离（P6 根因）：** `px4_fltmode_slot_table()` 是 `COM_FLTMODE1..6` 参数枚举（-1 Unassigned、0 Manual、…、7 Offboard、…、100–107 External），**与 custom_mode 不是同一编码**；`ffi_flight_modes.cc` 的 PX4 槽位名与 `options` 改走该表，通道参数改认实际存在的 `RC_MAP_FLTMODE`。
> - **验证：** `test_firmware_adapter.cc` 新增 3 组（256×256 custom_mode 穷举一致、新模式编解码、双表不混用），bridge `test_sim_ui_paths.cc` 新增 PX4 `mg_flight_get_mode_mapping` 用例；`build_check` 全量 `ctest` 通过，`tools/check_ffi_contract.py` 通过。

> - **S2 已完成（2026-10-11）。** 引导与起飞行为对齐 QGC `PX4FirmwarePlugin`：
>   - **引导语义（G3/P2）：** `PX4Plugin::guided_mode_name()` = `"Auto Loiter"`，`is_guided_mode()` = `Auto Loiter`/`Auto Takeoff`/`Auto Land` 三选一（QGC `isGuidedMode`，`:664-668`），与前端 `connection_controller.dart` 的 `auto loiter` 判定统一，core/前端不再两套说法。
>   - **起飞高度（G4/P3）：** `mg_command_takeoff` 在 PX4 下取 `GLOBAL_POSITION_INT` 的 MSL 高度（mm→m）加到请求的相对高度上，写进 `NAV_TAKEOFF param7`（AMSL，QGC `:322-350`）；缓存无有效位置（lat/lon 全零）时本地拒绝，不发假高度。ArduPilot 分支不变（相对 Home）。
>   - **暂停（G4/P3）：** `mg_command_guided_pause` 在 PX4 下改发 `MAV_CMD_DO_REPOSITION`（`param1=-1`、`param2=MAV_DO_REPOSITION_FLAGS_CHANGE_MODE`，QGC `pauseVehicle` `:276-288`），走 ACK 等待；ArduPilot 仍发零速度 `SET_POSITION_TARGET`。
>   - **capability（G6/P5）：** 新增能力位 `CapRoi`/`CapChangeHeading`；PX4 按机型给——基础 `CapGuidedMode|CapPause|CapCalibration|CapSetHome`，多旋翼加 `CapRoi|CapChangeHeading|CapTakeoff|CapOrbit`，VTOL 加 `CapTakeoff|CapOrbit`，固定翼加 `CapOrbit`（QGC `:174-190`）。ArduPilot 侧同步对齐：基础含 `CapRoi`，Copter 加 `CapChangeHeading|CapOrbit`，Plane 加 `CapOrbit`，Sub 加 `CapChangeHeading`（QGC `APMFirmwarePlugin.cc:65-83`）。
>   - **字段与入口一致（G6/P5）：** `mg_firmware_get_type` 的 `capabilities` 新增 `roi`/`change_heading`；前端 `supportsOrbit`/`supportsRoi` 改读 `orbit`/`roi`（不再用 `guided_mode` 顺带门控），地图菜单「环绕」「兴趣点」与实际入口一一对应（QGC `GuidedActionsController.qml:138-139`）。
>   - **死 API 收口（G11）：** 删除无调用者的 `default_loiter_radius` 虚函数；`minimum_takeoff_altitude` 从 2.5 改为 QGC 基类默认 3.048 m（10 ft）并经 `mg_firmware_get_type` 的 `minimum_takeoff_altitude_m` 暴露，前端 `minimumTakeoffAltitudeMeters` 在 PX4 下采用它（原先 PX4 恒为 0）。
>   - **验证：** `core/tests/test_firmware_adapter.cc` 新增 PX4 capability/引导语义与 ArduPilot 能力位两组（131/131）；`core/tests/test_sim_commands.cc` 新增 PX4 引导命令组（AMSL 起飞、缺位置拒绝、`DO_REPOSITION` 暂停，同时钉住 ArduPilot 仍用相对高度）与 ArduPilot 相对高度断言（110/110）；`flutter_shell/test/mg_map_action_menu_test.dart` 新增「orbit/ROI 按 capability 而非 guided mode」用例（全套 210 通过 / 4 跳过）。`cmake --build build_check` 成功，`ctest` 38/38，`check_ffi_contract.py` 305 声明 / 112 绑定一致。

> - **S3 已完成（2026-10-11）。** PX4 参数元数据按 ADR-0007 落地：
>   - **来源与许可（G7/D4）：** 引入 `core/src/config/PX4ParameterMetaData.json`（733 KB、1836 参数），取自 QGC 仓库 `src/FirmwarePlugin/PX4/PX4ParameterFactMetaData.json`（QGC `bf0096d97`，Apache-2.0 分支），原始数据由 PX4-Autopilot（BSD-3-Clause）参数定义生成；文件保持上游格式逐字不变（SHA-256 `94bd6df4…eb23a9`），许可与出处记录在 `core/src/config/PX4ParameterMetaData.LICENSE.txt`，决策见 `docs/adr/0007-px4-parameter-metadata.md`。
>   - **加载与作用域（P6 根因收口）：** `ParamDescriptors` 在主描述库与翻译后加载该文件到独立 profile `px4`；参数页查询路径 `ffi_param_profile()` 对 PX4 返回 `px4`。原先把同一函数结果当 ArduPilot 机型名复用的任务门禁拆出 `ffi_ardupilot_vehicle_class()`（只认 ArduPilot，PX4 返回空），参数元数据与任务字段语义不再互相牵制。
>   - **诚实降级（决定 6）：** `standalone_profiles_` 命中时 `get_from_profile()`/`get_group()` 不回落到 ArduPilot default，避免把同名 ArduPilot 参数（如 `BATT_MONITOR`）的显示名与枚举套到 PX4 车辆上；飞控未上报或快照未覆盖的参数仍走 `found=false` 降级。
>   - **值域（决定 5）：** 仅当快照同时给出 `min` 与 `max` 才落库区间（`validate()` 用 `min != max` 判定），只有单边界的 146 个参数不写成 `max=0`。
>   - **打包：** `CMakeLists.txt`、Linux/Windows/macOS/Android 打包清单与 `tools/check_*_bundle.dart`、`.github/workflows/build.yml` APK 资产断言均已同步该数据文件。
>   - **验证：** `core/tests/test_param_descriptors.cc` 新增 `test_px4_metadata()`（67/67），覆盖参数名/描述/分组/单位/枚举/只读/重启标记、单边界不落区间、`px4` profile 不回落 ArduPilot 默认描述；`cmake --build build_check` 成功。

> - **S4 已完成（2026-10-11）。** PX4 任务命令覆盖与 Home 语义对齐 QGC：
>   - **参数可见性（G8）：** `mission_command_catalog` 新增 profile 感知的 `mission_command_parameter_visible()`，按 QGC `PX4-MavCmdInfoCommon.json` 的 `paramRemove` 隐藏 PX4 不适用的字段——`NAV_WAYPOINT`(16) 去 param2/3、`NAV_LAND`(21) 去 param1、`VIDEO_START_CAPTURE`(2500) 去 param2；`mg_mission_command_catalog` 遍历参数时跳过不可见项，并给每条命令附 `vehicles` 数组。**顺带修一个真实缺陷**：目录 JSON 此前从未输出 `vehicles`，而 Dart 模型（`mission_document_models.dart:910`）与 `filterCommandCatalogForVehicle()` 一直在读它，结果前端机型过滤实际是个空操作——任何机型都看到全部 14 条命令。现在 `vehicles` 真正下发，过滤按能力表生效。参数可见性只影响前端显示，不改已存值（与 QGC 一致）。PX4 机型级覆盖文件为空，故不需机型细分。
>   - **Home 语义（G8）：** `bridge/ffi/ffi_mission_bridge.cc:10 mission_uses_home_item()` 只对 ArduPilot 前置 Home item，PX4 本就不把首个位置当 Home，与 QGC `sendHomePositionToVehicle()=false`（`:201-205`）一致，**无需改动**。`capabilities` 保留 `CapSetHome`：`DO_SET_HOME` 仍在 PX4 `supportedMissionCommands` 内，与「任务首项 Home」是两回事。
>   - **`DO_LAND_START`(189) 坐标（G8）：** QGC `PX4-MavCmdInfoCommon.json` 把 189 标成 `specifiesCoordinate:false`，而 ArduPilot `AP_Mission::stored_in_location()` 含 189。新增 `mission_command_uses_location(command, firmware_profile)` 重载（`core/include/core/mission/mission_types.h`）：PX4 下 189 不承载坐标，其余落回原 ArduPilot 语义。已接入任务编译、文档导入、QGC `.plan` 导入/导出与 JSON 序列化（`bridge/ffi/mission_json_serializers.h` 各接口新增 profile 参数，默认空串保持 ArduPilot 语义）。
>   - **D6 版本门控（已决）：** QGC `PX4FirmwarePlugin.h:98 anyVersionSupportsCommand` 把 `MAV_CMD_DO_SET_MISSION_CURRENT` 判为不支持并退回 `MISSION_SET_CURRENT`（`Vehicle.cc:2135`）；M-GCS `mg_mission_set_current`（`ffi_mission_commands.cc:97`）本就发 `MISSION_SET_CURRENT`，行为等价。**不做版本门控**：M-GCS 没有飞控版本解析，引入会带来未经测试的分支，收益为零。
>   - **已知边界：** M-GCS 可编辑命令目录只有 14 条常用命令，不含 189，因此 `validate_document()` 对含 189 的文档报 `entity.command_unsupported`（与固件 profile 无关）。上面 189 的坐标覆盖作用于**导入/导出/序列化**路径；编译期覆盖（`mission_compile.cc:179`）已就位，待目录纳入 189 时生效，测试已固定这一现状。
>   - **验证：** `core/tests/test_sim_ui_paths.cc::test_mission_catalog_param_remove_per_firmware()`（PX4 目录无 accept/pass radius，含 hold_time/yaw）、`core/tests/test_sim_mission.cc::test_mission_plan_json_land_start_per_firmware()`（PX4 plan JSON 保留 `command:189` 但不写其 lat/lon）、`core/tests/test_mission_document.cc::test_land_start_location_override_per_firmware()`（双参重载语义、导入差异、`.plan` params 5/6 null 与数值、往返不产生假位置、编译期目录门禁）。`cmake --build build_check` 成功，`ctest` 38/38 通过，`check_ffi_contract.py` 通过。

> - **S5 已完成（2026-10-11）。** 7 个 `mg_px4_*` 定性为 **FFI 内部能力**（不新增大 UI），并修掉封装里的两个真实缺陷（G9/P7 闭环）：
>   - **定位（D5 已决）：** 执行器映射、模式槽、安全断路器、调参四组导出保留给原生工具与测试，前端没有独立 UI 计划；通用参数页（`mg_params_*`/`mg_config_*`）配合 S3 的 `px4` 描述符 profile 已能读写同一批飞控参数并展示分组/范围/枚举。`docs/capabilities.md`/`API_CATALOG.md` 已标注为内部能力，`tools/check_ffi_contract.py` 新增 `INTERNAL_SYMBOL_NOTES` 与守卫：被标为内部的符号一旦出现在 Dart 绑定里就报错。
>   - **修复 1：伪造安全字段。** 旧 `mg_px4_safety_get/set` 写死 `CBRK_AIRSPD_CHK`/`CBRK_GPS_CHK`/`CBRK_IO_SAFETY`。事实：`CBRK_GPS_CHK` 在 QGC 的 `PX4ParameterFactMetaData.json` 与 PX4-Autopilot `e7b1d486e5` 源码里**都不存在**（永远读 0、set 静默失败）；`CBRK_AIRSPD_CHK` 存在于 PX4 源码但不在 QGC 快照。新实现改为**按飞控实际参数缓存报告** `CBRK_*`（字段名就是参数名），set 只接受真实存在的断路器：非 `CBRK_` 键 → `INVALID_PARAM`，飞控没有该参数 → `NOT_FOUND`，发送失败 → `SEND_FAILED`。
>   - **修复 2：执行器只覆盖 8 路。** 旧实现只读写 `PWM_MAIN_FUNC1..8`，而 PX4 主输出有 16 路（`PWM_MAIN_FUNC1..16` 在 QGC 快照与 PX4 源码中都存在）。改为读写 16 路，缺失通道按 PX4 默认 0 = Disabled 报告。
>   - **模式槽只读：** `mg_px4_flight_mode_get_config` 只读 `COM_FLTMODE1..6`。**故意不加 set**——写模式槽必须与 `RC_MAP_FLTMODE` 保持一致，误写会让飞控失去可用模式；写入走通用参数页（头文件已注明理由）。
>   - **验证：** `core/tests/test_ffi_contract.cc`（离线时 7 个导出返回统一 `NOT_SUPPORTED` 错误对象）、`core/tests/test_sim_ui_paths.cc`（PX4 执行器 16 路读写、安全只报真实 `CBRK_*` 且拒绝伪字段、模式槽 6 路只读、ArduPilot 下 7 个导出全部 `NOT_SUPPORTED`）。
> - **S6 已完成（2026-10-11）。** PX4 关键路径有非 `CHECK(true)` 的行为断言（G10 闭环）：
>   - `core/tests/test_sim_ui_paths.cc` 新增四组：`test_px4_actuator_maps_all_sixteen_channels()`（读回 `[1..16]`、写出含 `PWM_MAIN_FUNC9..16`、非数组入参拒绝）、`test_px4_safety_reports_only_real_circuit_breakers()`（只报缓存的 `CBRK_*`、`CBRK_GPS_CHK`/`CBRK_AIRSPD_CHK` 不被伪造、`NOT_FOUND`/`INVALID_PARAM`/写入转发）、`test_px4_flight_mode_slots_are_read_only_list()`（`COM_FLTMODE` 前缀、6 槽、原始参数枚举值）、`test_px4_apis_reject_ardupilot()`（ArduPilot 下 7 个导出全部 `NOT_SUPPORTED`）。
>   - S1/S2/S4 已有的原生断言（模式表穷举一致、引导/起飞 AMSL 与 `DO_REPOSITION`、目录字段可见性与 189 坐标）与本轮合并构成 PX4 关键路径的行为覆盖；`tests/manual/test_real_fc_px4_ops.cc` 保留为真机手工探针（其安全读取改读 `circuit_breakers` 对象）。
>   - **验证：** `cmake --build build_check -j8` 成功，`ctest` 38/38 通过，`check_ffi_contract.py` 通过，`test_sim_ui_paths` 142 项全过。

## 1. 目的

M-GCS 的产品目标里写着「优先 ArduPilot，通过固件插件兼容 PX4」。PX4 现在已经有一条能连、能解模式、能发的通路，但它是在多个历史阶段叠加出来的，存在**两张互相漂移的模式表**、**按 ArduPilot 参数名推导 PX4 行为**、以及**批未接入的配置导出**。本方向把 PX4 从「能凑合用」推进到「行为可解释、可验收、和 QGC 对齐」，并顺手消掉历史堆积，而不是继续加第三张表。

目标不是逐项追平 QGC 的全部 PX4 setup 页，而是先让 PX4 的**模式、引导/起飞、任务命令、参数元数据**四条主线语义正确，并明确哪些配置能力只做内部、哪些不承诺。

## 2. 范围

- 范围内：PX4 模式表的统一（消除双表漂移、补齐 PX4 1.14+ 新模式、按机型过滤）；引导/起飞/暂停/环绕/ROI 的行为对齐（AMSL 语义、模式前置、`isGuidedMode` 语义）；capability 表按机型细化；任务命令覆盖与 Home 语义；PX4 参数元数据的来源与许可决策（是否引入）；7 个 `mg_px4_*` 配置导出的去向（接 UI 或明确内部）；PX4 错误路径测试。
- 范围外：PX4 固件刷写/引导加载（需独立 ADR，与阶段 3 方向 6 合并考虑）；PX4 全量参数元数据的人工维护（只做「引入官方 JSON」或「不引入」二选一）；PX4 的 Offboard 外部控制接口；多机 PX4 机队。

## 3. M-GCS 现状

| 层 | 位置 | 状态 |
|---|---|---|
| PX4 插件 | `core/src/firmware/firmware_plugin_px4.cc`（S1 后改为消费 `px4_modes.h`，自带 `kPX4Modes` 已删除）、`core/include/core/firmware/px4_plugin.h` | S1+S2：`decode_flight_mode`/`encode_flight_mode`/`available_modes` 走唯一模式表；`guided_mode_name()`=`Auto Loiter`，`is_guided_mode()`=Auto Loiter/Takeoff/Land 三模式；`capabilities()` 按机型细化；`minimum_takeoff_altitude()=3.048 m`；`default_loiter_radius` 已删除 |
| PX4 模式表（唯一事实来源） | `core/include/core/firmware/modes/px4_modes.h` | S1 后为唯一表：`px4_mode_table()`/`decode_px4_mode()`/`encode_px4_custom_mode()`/`encode_px4_mode_name()`；另含含义不同的 `px4_fltmode_slot_table()`（参数枚举）。插件与 adapter 都从这里取 |
| 固件检测/路由 | `core/src/firmware/firmware_adapter.cc:33-44`（`MAV_AUTOPILOT_PX4=12`）、`:180`（走 `decode_px4_mode`）、`:238-262`（`resolve_mode_role` 用 PX4 名） | 已实现；PX4 无 ArduPilot 式的子机型分类 |
| 工厂 | `core/src/firmware/firmware_plugin_ardupilot.cc:317-327`（`create_firmware_plugin()` 位于 ArduPilot 文件尾部） | 结构略怪：PX4 与通用插件工厂藏在 ArduPilot 实现文件末尾 |
| 模式映射 FFI | `bridge/ffi/ffi_flight_modes.cc` | S1 后：按实际存在的参数选 `FLTMODE1`/`COM_FLTMODE1` 前缀；PX4 通道参数改认 `RC_MAP_FLTMODE`；PX4 槽位名与 `options` 走 `px4_fltmode_slot_table()`（参数枚举），不再用模式表反查 |
| 飞行动作 FFI | `bridge/ffi/ffi_flight_commands.cc`（起飞、暂停、环绕、`0x04040000` 自动模式打包） | S2：PX4 起飞换算 AMSL 且缺位置拒绝；PX4 暂停发 `DO_REPOSITION`+`CHANGE_MODE`；ArduPilot 分支保持相对高度与零速度暂停 |
| PX4 配置导出 | `bridge/ffi/ffi_px4.cc`（7 个 `mg_px4_*`）、`bridge/ffi/ffi_api_configuration.h:51-57` | **S5：定性为 FFI 内部能力**（无 Dart 绑定、无独立 UI 计划；通用参数页 + `px4` profile 覆盖同一批参数）。并修真实缺陷：安全只报飞控实际 `CBRK_*`（不再伪造 `CBRK_GPS_CHK`）、执行器 8→16 路、模式槽只读 |
| 前端能力消费 | `flutter_shell/lib/connection/connection_controller.dart`（`guided_mode`/`pause`/`orbit`/`roi`/`takeoff`/`change_heading` capability、`minimumTakeoffAltitudeMeters`、PX4 Go To=`auto loiter`、PX4 起飞先 takeoff 再解锁）、`flutter_shell/lib/widgets/mg_map_action_menu.dart` | S2：起飞/环绕/ROI/改航向入口改读 capability；PX4 最小起飞高度读 `minimum_takeoff_altitude_m`；未定位时起飞前置拒绝（`smart_rtl`/`set_home`/`motor_test` 仍未消费） |
| 校准 | `bridge/ffi/ffi_calibration.cc`、`docs/constraints/parity.md`「校准取消」 | PX4 上不存在的取消命令返回「不支持」而非假装成功（S8 已完成） |
| 传感器 ID | `core/src/sensor/sensor_id_decoder.cc:177-305`、`bridge/ffi/ffi_sensor_decode.cc:35-60` | 已支持 PX4 设备 ID 编码/类型码 |
| 参数元数据 | `core/src/config/ParameterDescriptors.json`（`sources` 只有 `ardupilot_4_8`）+ `core/src/config/PX4ParameterMetaData.json`（PX4 快照，1836 参数，ADR-0007） | S3：加载到独立 profile `px4`，参数页有分组/描述；快照未覆盖项诚实降级 |
| 任务命令 / Home | `core/src/mission/cad/mission_command_catalog.cc`（`px4_hides_parameter`/`mission_command_parameter_visible`）、`bridge/ffi/ffi_mission_catalog.cc`（跳过不可见参数、附 `vehicles`）、`bridge/ffi/ffi_mission_bridge.cc:10`（`mission_uses_home_item`）、`core/include/core/mission/mission_types.h`（`mission_command_uses_location` 重载）、`bridge/ffi/mission_json_serializers.h`（带 profile） | S4：PX4 按 `paramRemove` 隐藏字段；Home 只对 ArduPilot 前置（与 QGC 一致）；PX4 下 189 不承载坐标（导入/导出/序列化已接入，编译期待目录纳入） |
| 测试 | `core/tests/test_firmware_adapter.cc`（S1 模式表三组 + S2 capability/引导语义两组）、`core/tests/test_sim_commands.cc`（S2 PX4 引导命令组）、`core/tests/test_sim_ui_paths.cc`（S1 PX4 模式映射 + **S6 四组 PX4 行为/错误路径**：16 路执行器、真实 `CBRK_*`、6 路模式槽只读、ArduPilot 全部拒绝）、`core/tests/test_ffi_contract.cc`（离线 7 个 `mg_px4_*` 返回统一 `NOT_SUPPORTED` 错误对象）、`flutter_shell/test/mg_map_action_menu_test.dart`（S2 orbit/ROI capability 用例）、`tests/manual/test_real_fc_px4_ops.cc`（手工真机探针） | S1/S2/S4/S6 行为已有断言；PX4 错误路径与拒绝分支已覆盖（无 `CHECK(true)`） |

### 已发现的真实问题（本轮只读核对）

- **P1（S1 已闭环）两张 PX4 模式表漂移。** `PX4Plugin`（`firmware_plugin_px4.cc:59-79`）与 `px4_modes.h:16-78` 是同一协议事实的两份实现，命名与集合都不一致：
  - 子模式 8：插件叫 `"Follow Target"`，`px4_modes.h` 叫 `"Auto Follow"`；
  - 子模式 9：插件叫 `"Precision Land"`，`px4_modes.h` 叫 `"Auto Precision Land"`；
  - 子模式 7（PX4 已于 2020-03-05 删除的 RTGS）：插件仍列 `"Auto RTGS"`（`can_be_set=false`），`px4_modes.h:42` 叫 `PX4_AUTO_SUB_MODE_RESERVED` 且 `decode_px4_mode` 没有分支，落到默认 `"Auto"`；
  - `px4_modes.h` 多了 `SIMPLE=9`、`TERMINATION=10`、`VTOL_TAKEOFF=10`，插件的可设置表里没有这些；
  - 两张表都**缺** PX4 1.14+ 的新值：`ALTITUDE_CRUISE=11`、POSCTL 子模式 `ORBIT=1`/`SLOW=2`、`AUTO_GUIDED_COURSE=19`（QGC `px4_custom_mode.h:9-60` 已有）。
  因为 `firmware_adapter.cc:180` 走 `px4_modes.h`，而 `resolve_mode_role`（`:238-262`）走插件表，同一个 custom_mode 在状态栏与角色解析里可能得到不同名字。（**S1 已消除**：插件与 adapter 现共用 `px4_modes.h` 的 `px4_mode_table()`，并有穷举一致性测试。）
- **P2 引导语义错位。** `px4_plugin.h:29` 把 `guided_mode_name()` 定为 `"Offboard"`、`:34-36` 把 `is_guided_mode` 判为 `mode == "Offboard"`；但 QGC 的 PX4 引导/Go To 模式是 `AUTO_LOITER`，`isGuidedMode` 判定的是 `pause`/`takeOff`/`land` 三个模式（`PX4FirmwarePlugin.cc:664-668`）。前端已经按 QGC 纠正（`connection_controller.dart:853` 把 PX4 的 Go To 模式判成 `auto loiter`），于是 core 与前端对「什么是引导模式」有两套说法。 （**S2 已闭环**：`guided_mode_name()` 改为 `Auto Loiter`，`is_guided_mode` 判 `Auto Loiter`/`Auto Takeoff`/`Auto Land`，与前端一致。）
- **P3 起飞高度语义与前置顺序未对齐 QGC。** `mg_command_takeoff`（`ffi_flight_commands.cc:147-151`）把用户输入的**相对高度**直接写进 `NAV_TAKEOFF` `param7`；QGC PX4 先取当前 AMSL，把 `param7` 换算成 **AMSL**，且 `param3=0`（`PX4FirmwarePlugin.cc:322-345`）。前端 PX4 分支（`connection_controller.dart:2428-2435`）确实做了「先发 takeoff 再解锁」的顺序，但没有 AMSL 换算。 （**S2 已闭环**：PX4 分支换算 AMSL 并在缺位置时拒绝；暂停改发 `DO_REPOSITION` + `CHANGE_MODE`；ArduPilot 语义不变。前端「先发 takeoff 再解锁」的 PX4 顺序保留。）
- **P4 起飞最小高度参数对 PX4 失效。** 前端 `minimumTakeoffAltitudeMeters`（`:1025-1042`）只读 ArduPilot 参数（`PILOT_TKO_ALT_M`/`Q_PILOT_TKO_ALT_M` 等），PX4 下恒为 0；core `PX4Plugin::minimum_takeoff_altitude` 返回 2.5（`firmware_plugin_px4.cc:121-124`）却**没有任何调用者**，也没经 FFI 暴露。 （**S2 已闭环**：`PX4Plugin::minimum_takeoff_altitude` 改为 QGC 基类默认 3.048 m 并经 `mg_firmware_get_type` 的 `minimum_takeoff_altitude_m` 暴露，前端 PX4 分支采用它；死 API `default_loiter_radius` 删除。）
- **P5 capability 表不分机型且字段与实际不符。** `PX4Plugin::capabilities`（`firmware_plugin_px4.cc:116-119`）对所有机型返回同一组 `CapGuidedMode|CapPause|CapTakeoff|CapSetHome|CapCalibration`；QGC 按机型给：多旋翼加 `ROI|ChangeHeading`，多旋翼/VTOL 加 `TakeoffVehicle|GuidedTakeoff|OrbitMode`，VTOL 再加 `VTOLMulticopterTakeoff`，固定翼加 `TakeoffVehicle|OrbitMode`（`PX4FirmwarePlugin.cc:174-190`）。M-GCS 的 `capabilities()` 不含 `CapOrbit`，但前端环绕入口用的是 `guided_mode`（`amap_map_view.dart:594,617`）而非 `orbit`，所以行为上能用、JSON 里却报 `orbit=false`——字段与行为不一致。 （**S2 已闭环**：capability 按机型细化并新增 `CapRoi`/`CapChangeHeading`；`capabilities` JSON 新增 `roi`/`change_heading`，`orbit`/`roi` 字段与地图菜单入口一一对应。）
- **P6（S1 已闭环）模式映射 FFI 的通道逻辑对 PX4 无意义。** `mg_flight_get_mode_mapping`（`ffi_flight_modes.cc:96-107`）只认 `FLTMODE_CH`/`MODE_CH`，PX4 没有模式选择通道参数，会静默不输出 `channel_param`；模式槽位名称经 `decode_mode_name`（走 P1 的漂移表）得到，PX4 槽位显示名同样受 P1 影响。
- **P7 7 个 `mg_px4_*` 是无人调用的公开导出。** `ffi_px4.cc` 全部实现且进入 `API_CATALOG.md`，但 Dart 侧 0 引用，`docs/capabilities.md` 只说「内部能力」。这些接口既没有 UI 入口，也没有 FFI 契约测试覆盖其成功路径（`test_ffi_contract.cc` 旧版只断言返回非空）。 （**S5 已闭环**：定性为 FFI 内部能力并登记在 `capabilities.md`/`API_CATALOG.md` 与 `check_ffi_contract.py`；顺带修掉两个真实缺陷——安全不再伪造 `CBRK_GPS_CHK`，执行器改为覆盖 16 路。 **S6 已闭环**：`test_ffi_contract.cc` 改为断言离线统一 `NOT_SUPPORTED` 错误对象，`test_sim_ui_paths.cc` 覆盖成功/拒绝/错误三条路径。）

## 4. 上游行为研究

### QGroundControl（Apache-2.0 / GPL 双许可；参考提交 `bf0096d97`）

- **模式表**：`src/FirmwarePlugin/PX4/PX4FirmwarePlugin.cc:63-84`（枚举→名称）与 `:96-117`（`availableFlightModes`，22 项，每项带 `canBeSet` 与 `adv`）；`:120-140` 按 `vehicle->fixedWing()` 过滤可设置模式。协议值来源 `src/FirmwarePlugin/PX4/px4_custom_mode.h:9-60`，文件头写明「Mirror of PX4's px4_custom_mode.h（source of truth）…never renumber or insert, only append」。
- **Capability**：`PX4FirmwarePlugin.cc:174-190`（`isCapable`）基础 `SetFlightMode|PauseVehicle|GuidedMode`，按机型叠加 ROI/ChangeHeading/TakeoffVehicle/GuidedTakeoff/OrbitMode/VTOLMulticopterTakeoff。
- **Home 语义**：`:201-205` `sendHomePositionToVehicle()` 返回 **false**——PX4 不接受「第一个位置即 Home」，后续序号要调整。
- **任务命令**：`:199-249` `supportedMissionCommands(vehicleClass)` 列举 NAV/DO 命令，按机型加 VTOL/固定翼命令；`:250-...` `missionCommandOverrides()` 按机型返回 `PX4-MavCmdInfo{Common,FixedWing,MultiRotor,Rover,Sub,VTOL}.json`。`PX4-MavCmdInfoCommon.json` 只有 4 条覆盖（WAYPOINT 去掉 param2/3、LAND 去掉 param1、DO_LAND_START 无坐标、VIDEO_START_CAPTURE 去掉 param2）。
- **引导行为**：`pauseVehicle`（`:276-288`）发 `DO_REPOSITION`，`param2 = MAV_DO_REPOSITION_FLAGS_CHANGE_MODE`；`guidedModeRTL/Land`（`:290-299`）走 `_setFlightModeAndValidate`；`guidedModeTakeoff`（`:322-350`）取当前 AMSL，`param7 = takeoffAltRel + currentAMSL`，`param3=0`，命令被接受后再 arm；`guidedModeChangeAltitude`（`:486-...`）先暂停再发 `DO_REPOSITION`；`guidedModeChangeHeading`（`:551-...`）用 `DO_REPOSITION` `param4=radians`；`guidedModeROI` 把相对高度转 AMSL（PX4 忽略 frame、按 AMSL 处理，PX4-Autopilot#28257）。
- **起步流程**：`startTakeoff`（`:586-...`）/`startMission`（`:598-...`）都是**先 `_setFlightModeAndValidate` 切模式，再 `_armVehicleAndValidate`**，各自有明确错误提示。
- **特殊模式名**：`pauseFlightMode=AUTO_LOITER`（`:619`）、`missionFlightMode=AUTO_MISSION`（`:624`）、`rtlFlightMode=AUTO_RTL`（`:629`）、`landFlightMode=AUTO_LAND`（`:634`）、`takeControlFlightMode=MANUAL`（`:639`）、`gotoFlightMode=AUTO_LOITER`（`:644`）、`followFlightMode=AUTO_FOLLOW_TARGET`（`:649`）、`takeOffFlightMode=AUTO_TAKEOFF`（`:654`）、`stabilizedFlightMode=STABILIZED`（`:659`）；`isGuidedMode`（`:664-668`）判定为「当前模式 == pause/takeoff/land 三者之一」。
- **版本处理**：`_handleAutopilotVersion`（`:686-...`）解析 PX4 `flight_sw_version` 高/中/低字节，低于支持版本时提示用户；`PX4FirmwarePluginInstanceData::anyVersionSupportsCommand` 用于「不支持 `MAV_CMD_DO_SET_MISSION_CURRENT`」等按版本门控。
- **其他**：`supportsNegativeThrust` Rover/Submarine 为 true（`:747-...`）；`hasGripper`（`:774-...`）旧版看 `PD_GRIPPER_EN != 0`、v1.17+ 看 `PD_GRIPPER_TYPE >= 0`；`getHobbsMeter` 读 `LND_FLIGHT_T_HI/LO`（64 位微秒）。
- **参数元数据**：`src/FirmwarePlugin/PX4/PX4ParameterMetaData.cc`（79 行）解析 QGC 的 JSON v1（`parameters` 数组），`_postProcessMetaData` 把空类别归 `Standard`、volatile 值设只读、短/长描述换行改空格。数据文件 `PX4ParameterFactMetaData.json` 共 **1836 个参数**，顶层 `{parameters, translation, version:1}`。

### Mission Planner（GPLv3，**只参考行为，禁止复制源码**；参考提交 `a2fcd74d6`）

- `ExtLibs/ArduPilot/Firmwares.cs:15` 里 PX4 只是一个固件枚举；`APFirmware.cs` 未匹配到 PX4 关键词。**MP 对 PX4 的支持很薄，没有插件抽象**，可作为「不做插件抽象、PX4 只能弱支持」的对照证据，不作为 PX4 行为参考。

## 5. 差距与决策点

| 编号 | 差距 | 严重度 | 后续 |
|---|---|---|---|
| G1 | PX4 双模式表漂移 + 缺新模式（P1） | P2 | **S1 已闭环** |
| G2 | 模式映射槽位名受漂移表影响、通道逻辑对 PX4 无意义（P6） | P3 | **S1 已闭环** |
| G3 | 引导模式名与 `isGuidedMode` 语义（P2） | P2 | **S2 已闭环** |
| G4 | 起飞 AMSL 换算与前置顺序（P3） | P1 | **S2 已闭环** |
| G5 | PX4 最小起飞高度失效（P4） | P3 | **S2 已闭环** |
| G6 | capability 不分机型、`orbit` 字段与行为不一致（P5） | P2 | **S2 已闭环** |
| G7 | PX4 参数元数据缺失（`ParameterDescriptors.json` 无 PX4） | P2 | **S3 已闭环**（ADR-0007） |
| G8 | PX4 任务命令覆盖 / Home 语义未按 QGC | P2 | **S4 已闭环** |
| G9 | 7 个 `mg_px4_*` 无人调用（P7） | P3 | **S5 已闭环**（定性为 FFI 内部能力；`API_CATALOG.md`/`capabilities.md`/`parity.md` 一致） |
| G10 | PX4 错误路径与行为无自动测试 | P2 | **S6 已闭环**（`test_sim_ui_paths.cc` 四组行为断言 + `test_ffi_contract.cc` 离线错误对象） |
| G11 | `minimum_takeoff_altitude`/`default_loiter_radius` 无调用者（死 API 面） | P3 | **S2 已闭环** |

决策点：

- **D1 模式表唯一来源（S1 已决）。** 已按前者落地：`px4_modes.h` 作为唯一数据 + 枚举，`PX4Plugin` 改为消费它（保持 `firmware_adapter` 的「表在 header」结构，与 ArduPilot 五张表一致）。
- **D2 RTGS / SIMPLE / TERMINATION 的展示（S1 已决）。** PX4 已删除 RTGS，`SIMPLE`/`TERMINATION` 不可设置。S1 定为：`SIMPLE`/`TERMINATION` 保留在模式表、`can_be_set=false`（状态栏仍能正确显示「飞控当前处于 Termination」）；RTGS（AUTO 子模式 7）不进表，解码走 `Mode(N)` 兜底。
- **D3 capability 分机型（S2 已决）。** 已照 QGC 分组落地：PX4 基础 `CapGuidedMode|CapPause|CapCalibration|CapSetHome`，多旋翼加 `CapRoi|CapChangeHeading|CapTakeoff|CapOrbit`，VTOL 加 `CapTakeoff|CapOrbit`，固定翼加 `CapOrbit`（`PX4FirmwarePlugin.cc:174-190`）；ArduPilot 侧也按 `APMFirmwarePlugin.cc:65-83` 细化。前端 Go To/环绕/起飞入口改读 capability（`orbit`/`roi`/`takeoff`），不再各自猜测。
- **D4 参数元数据来源（S3 已决）。** 已引入 QGC 的 `PX4ParameterFactMetaData.json` 快照（1836 参数、733 KB）为独立数据文件，许可与降级策略见 `docs/adr/0007-px4-parameter-metadata.md`（原始数据 BSD-3、QGC 副本 Apache-2.0，均允许保留声明再分发；M-GCS 不引入 GPL 代码路径）。
- **D5 `mg_px4_*` 去向（S5 已决）。** 定性为 FFI 内部能力：7 个导出保留给原生工具与测试，不接入独立配置 UI（通用参数页配合 `px4` 描述符 profile 已覆盖同一批参数），并在 `API_CATALOG.md`/`capabilities.md`/`check_ffi_contract.py` 标注与守卫。
- **D6 `MAV_CMD_DO_SET_MISSION_CURRENT` 版本门控（S4 已决，不实现）。** QGC 按固件版本禁用该命令并退回 `MISSION_SET_CURRENT`；M-GCS `mg_mission_set_current` 本就发 `MISSION_SET_CURRENT`，行为等价，且 M-GCS 无飞控版本解析，引入门控只会增加未测试分支。

## 6. 交付切片

- **S1 统一模式表（已完成，2026-10-11）。** 合并 `px4_modes.h` 与 `kPX4Modes` 为单一事实；补齐 `ALTITUDE_CRUISE=11`、POSCTL `ORBIT/SLOW`、`AUTO_GUIDED_COURSE=19`、`VTOL_TAKEOFF=10`；把 RTGS 标为保留、不可设置；统一命名（`Follow Target`/`Precision Land` 等取 QGC 或一致命名，二选一并记录）；`mg_flight_get_mode_mapping` 的 PX4 槽位名改走统一表；补一份「两表一致」的断言测试。验收：任意 custom_mode 在插件与 adapter 解码结果一致，新模式能编码/解码。
- **S2 引导与起飞行为（已完成，2026-10-11）。** core `guided_mode_name`/`is_guided_mode` 改为 QGC 的 pause/takeoff/land 三模式语义；`mg_command_takeoff` 增加 PX4 AMSL 换算（缺位置本地拒绝）；暂停按 QGC 用 `DO_REPOSITION flags=CHANGE_MODE`；capability 按机型细化且 `orbit`/`roi` 字段与 UI 入口一致；死 API 收口（`minimum_takeoff_altitude` 经 `minimum_takeoff_altitude_m` 暴露给前端并取 QGC 默认 3.048 m，`default_loiter_radius` 删除）。验收达成：PX4 引导动作的 MAVLink 帧与 QGC 语义一致（`test_sim_commands.cc` PX4 组断言 AMSL/`DO_REPOSITION`/缺位置拒绝），capability JSON 与 UI 入口一一对应（`mg_map_action_menu_test.dart`）。
- **S3 PX4 参数元数据（已完成，2026-10-11）。** 已写 `docs/adr/0007-px4-parameter-metadata.md` 定来源/许可/降级，引入 `PX4ParameterMetaData.json` 并由 `ParamDescriptors` 解析到 profile `px4`（复用 QGC 的 v1 结构；仅同时有 `min`+`max` 才落区间；独立 profile 不回落 ArduPilot 默认描述）。验收达成：`core/tests/test_param_descriptors.cc::test_px4_metadata()` 断言参数页有正确分组/描述/枚举/只读标记，未覆盖项诚实降级。
- **S4 PX4 任务命令覆盖（已完成，2026-10-11）。** 参数可见性按 QGC `paramRemove` 落地（WAYPOINT 去 param2/3、LAND 去 param1、VIDEO_START_CAPTURE 去 param2），目录命令附 `vehicles`；Home 语义核对后确认与 QGC 一致（只对 ArduPilot 前置）；新增 profile 感知的 `mission_command_uses_location` 覆盖 PX4 下 189 无坐标；D6 版本门控决定不实现。验收达成：PX4 目录参数过滤与 189 坐标语义有断言（`test_sim_ui_paths.cc`/`test_sim_mission.cc`/`test_mission_document.cc`），Home 不误入任务。
- **S5 `mg_px4_*` 去向（已完成，2026-10-11）。** 定性为 FFI 内部能力（D5 已决）：7 个导出保留给原生工具/测试，前端无独立 UI 计划，通用参数页覆盖同一批参数；顺带修两个真实缺陷（安全字段不再伪造 `CBRK_GPS_CHK`、执行器 8→16 路）。`tools/check_ffi_contract.py` 新增内部符号定性表与「内部符号被绑定到 Dart 即报错」的守卫，`API_CATALOG.md`/`capabilities.md`/`parity.md` 已一致。验收达成：没有「公开导出但身份不明」的 PX4 符号。
- **S6 PX4 capability 与错误路径测试（已完成，2026-10-11）。** `core/tests/test_sim_ui_paths.cc` 新增四组原生断言（16 路执行器读写、安全只报真实 `CBRK_*` 并拒绝伪字段、模式槽 6 路只读、ArduPilot 下 7 个导出全部 `NOT_SUPPORTED`）；与 S1/S2/S4 原有断言合并覆盖 PX4 关键路径。`tests/manual/test_real_fc_px4_ops.cc` 保留为真机手工探针（安全读取改读 `circuit_breakers` 对象）。验收达成：PX4 关键路径有非 `CHECK(true)` 的行为断言。

## 7. 验收标准

1. PX4 模式只有一份事实来源；任意 custom_mode 的解码在插件与 adapter 一致（有测试钉住）。
2. 新模式（`ALTITUDE_CRUISE`、POSCTL `ORBIT/SLOW`、`GUIDED_COURSE`、`VTOL_TAKEOFF`）可正确编码/解码。
3. PX4 起飞、引导、暂停的 MAVLink 帧语义与 QGC 一致（AMSL 高度、`DO_REPOSITION flags`、模式前置）。
4. `capabilities` 按机型正确，且 JSON 字段与前端实际入口一一对应（无字段说 false 而入口可用）。
5. 参数元数据已引入（S3）并有许可 ADR（`docs/adr/0007-px4-parameter-metadata.md`）：PX4 连接后参数页有快照提供的分组/描述，未覆盖项诚实降级，且不回落 ArduPilot 默认描述。
6. PX4 任务命令覆盖与 QGC 一致：目录按 `paramRemove` 隐藏 PX4 不适用的字段，Home 不误入任务，`DO_LAND_START`(189) 的坐标语义按固件区分（均有断言钉住）。
7. 7 个 `mg_px4_*` 有明确定位（已接入 UI 或标注为内部），`API_CATALOG.md`/`capabilities.md`/`parity.md` 一致。
8. `check_ffi_contract.py` 无未登记差异；`preflight.sh` 通过。

## 8. 平台

- 本轮为 native core + bridge + Flutter 行为对齐，三平台通用；不引入平台专属依赖。
- 真机验收依赖 PX4 飞控或 PX4 SITL（M-GCS 无法代跑，需用户与硬件/仿真环境）。

## 9. 许可与外部依赖

- **QGC 为 Apache-2.0 / GPL 双许可**（仓库根有 `LICENSE-APACHE` 与 `LICENSE-GPL`）。M-GCS 只参考**行为与协议事实**（模式值、命令参数、状态机、错误语义），不复制 QGC 源码；引用 Apache 分支代码前必须先写 ADR 记录许可与再分发义务（`docs/constraints/upstream.md` P4）。
- **MP 为 GPLv3**，禁止复制源码，只作行为对照。
- **PX4 参数元数据（S3 已落地）**：`core/src/config/PX4ParameterMetaData.json`（1836 参数）取自 QGC `PX4ParameterFactMetaData.json`（Apache-2.0 分支，QGC `bf0096d97`），原始数据由 PX4-Autopilot（BSD-3-Clause）生成。BSD-3 与 Apache-2.0 均允许保留版权/许可声明再分发；M-GCS 保持 MIT，不引入 GPL 代码路径。许可原文、出处与 SHA-256 见 `core/src/config/PX4ParameterMetaData.LICENSE.txt`，再分发声明清单与决策见 `docs/adr/0002-upstream-reference-and-license.md`、`docs/adr/0007-px4-parameter-metadata.md`。

## 10. 未决问题

- ~~D1/D2/D3 的实现形态~~（S1/S2 已决，见 §5 决策点）。
- ~~D4 参数元数据来源与许可~~（S3 已决，见 ADR-0007）。
- ~~D5 `mg_px4_*` 是接 UI 还是标注为内部~~（S5 已决：标注为 FFI 内部能力，接通用参数页，见 §5 决策点）。
- ~~D6 是否做 `DO_SET_MISSION_CURRENT` 版本门控~~（S4 已决：不实现，行为等价且无版本解析，见 §5 决策点）。
- 与阶段 3 方向 6（固件刷写）的边界：PX4 刷写需独立 ADR，不在本方向。
