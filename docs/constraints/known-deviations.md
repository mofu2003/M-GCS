# 已知偏差

登记当前代码与规则不符的条目。本文件只登记，不在此处修复。修复时把条目从未解决移到已关闭，并在提交说明里引用编号。

- 规则列为约束编号（S、A、F、T、U、R、P）的，属于**约束偏差**，必须修复，或在本文件中登记计划。
- 规则列标注"建议"的，属于**建议偏差**，不阻断合并，但需要在提交说明中写明理由。

状态取值：`待修复`、`待决定`、`已决定`（建议偏差，理由已记录、不阻断合并）、`待验证`、`待核对`、`待补充`。

文件分两部分。**未解决**是当前仍需处理的偏差，也是唯一需要盯住的清单。**已关闭**一行一条，只为让代码注释与提交信息里的 `KD-xx` 编号仍可溯源；正文细节见 `CHANGELOG.md` 的对应版本段，不再在这里堆叠过程描述。

## 未解决

| ID | 规则 | 位置 | 现状 | 状态 |
|---|---|---|---|---|
| KD-09 | S3（建议：写入类操作确认） | `flutter_shell/lib/configuration/widgets/parameter_value_control.dart:55-70` | **已决定：接受偏差**。参数写入不弹确认框，与 QGC、MP 一致——`safety.md` 的建议条款本身写明「QGC 与 MP 对这些操作都不确认，因此不做硬性要求」。参数整定是高频操作，逐次弹窗会明显拖慢流程；写入失败已有回滚路径（`parameter_write_status.dart`，`test/configuration_components_test.dart` 的 “numeric editor rolls back a rejected parameter write”）。要让「安全相关参数优先确认」落地，需要插件在参数描述符里额外下发一个安全标记（S6 要求参数名等固件专属值只能由插件提供，前端不得硬编码），属后续能力扩展。 | 已决定 |
| KD-27 | 架构（建议：事件获取方式） | `flutter_shell/lib/connection/connection_controller.dart:1982`（会话 750 ms）、`:1987`（遥测 50 ms）、`:1247`（校准 250 ms）、`:1771`（机队 750 ms）、`flutter_shell/lib/connection/waveform_controller.dart:277`（波形 100 ms） | **已决定：接受偏差，保留轮询**。FFI 的 `mg_set_event_callback` 已存在且由 core/FFI 测试覆盖（`test_ffi_contract.cc`、`test_sim_ui_paths.cc`），Dart 侧仍用 `Timer.periodic` 拉取。选轮询的理由：Dart FFI 上行回调需要 `NativeCallable`/`Pointer.fromFunction` 并处理跨线程与 isolate 生命周期，而当前绑定层完全不含 FFI 回调；轮询延迟有界（50–750 ms），对地面站交互足够。回调路径保留为低延迟备选，等 `docs/plans` 阶段 1 统一视图状态时再评估迁移。 | 已决定 |
| KD-36 | U3 | `flutter_shell/lib/demo/component_gallery_demo.dart:169,953,1039` | **已决定：接受**。该文件是开发者画廊（自带 `main()`，`main.dart` 不引用、不进 release 包），用于并排对照 `macos_ui` 的原始色板与字号，不属于 U3 约束的「业务页面」。它是 `tools/check_ui_tokens.dart` 唯一的豁免文件，文件头已写明原因。 | 已决定 |
| KD-40 | U7 | `flutter_shell/test/` | **已决定：接受现状**。U7 的约束是「三端构建通过 + 人工截图比对」，未要求自动化 golden。Flutter golden 对字体与平台渲染敏感，三端同时稳定很难，且会带来持续的基线维护成本；保留人工比对。自动化截图比对作为平台 CI 项列在 `docs/plans` 阶段 4。 | 已决定 |
| KD-43 | 架构（建议：按变更耦合拆分） | `flutter_shell/lib/connection/connection_controller.dart`（3833 行，`ConnectionController` 自 `:454` 起） | **已评估：不整体拆分，只抽波形子系统**。实测变更耦合：把当时的 4160 行按职责切成 9 块、用 `git log -L` 追每块在 38 个提交里的共变，结果是没有任何逻辑块独立演进——只碰一个非装配块的提交只有 3 次（其中 2 次是版本号/单方法小改），校准/参数/遥测/波形/飞行命令从未被单独改动；装配/状态块（字段声明 + 子控制器工厂）被 35/38 个提交动过且从不单独出现，最强共变是装配块 ↔ 车辆/会话块（26 次，Jaccard 0.74）。根因是切车要快照/恢复/清空所有子系统状态，所以按功能块拆文件只会把共享状态打散再跨文件拼回来。**结论：不整体拆分**（对齐 A7，不为拆而拆）。**执行：只抽接缝最窄的波形子系统**——新增 `flutter_shell/lib/connection/waveform_controller.dart`（`WaveformController` 与 `WaveformInstanceKind`/`WaveformInstance`/`WaveformSnapshot`，由 `connection_controller.dart` re-export 保持原有 import 路径），状态与逻辑成对迁出，切车/断连改走 `captureSnapshot`/`applySnapshot`/`reset`/`stopPolling`/`restartPolling`/`onLinkEstablished` 六个生命周期钩子（对齐 `70e2692` 抽 `NativeCore` + `SigningController` 的先例）；控制器里波形字段与 12 个波形方法全部迁走，只剩 6 行接线，4160 → 3833 行。验证：`flutter analyze` 无问题、`flutter test` 247 通过 / 4 跳过（含 `fleet_ffi_smoke_test` 的「切车不串车、波形不跨车缓存」断言）、`tools/preflight.sh` 全量通过。共变重测：抽取前 38 个提交里触及波形区域的 10 个提交全部同时改动控制器其它区域（0 个只改波形的提交）；抽取后波形状态与 12 个方法集中在新文件，控制器只剩 12 处接线。**剩余**：装配/状态块仍是最大的那块，要继续动需先有新的变更耦合证据。 | 已决定 |
| KD-44 | 架构（建议：按变更耦合拆分） | `flutter_shell/lib/mission/mission_planning_panel.dart`（2362 行，`_MissionPlanningFloatingPanelState` 为 `:54-430`） | **已评估：只抽任务动作条/传输校验区块**。实测变更耦合：把面板状态类按职责切成 7 块（装配/生命周期、build+任务页、动作条/传输/校验、文案辅助、命令执行、设置页、距离助手），用 `git log -L` 追每块在 19 个提交里的共变，结果是 8/19 提交只碰一个块（装配 2、动作条 2、文案 1、命令 1、设置页 1、距离助手 1），非装配块独占 6/19，而 KD-43 同类只有 1/38；最强共变是动作条 ↔ 命令执行（7 次，Jaccard 0.54），低于 KD-43 的 0.74——说明动作条与其命令处理器是一个单元，但整类不是纠缠团。**执行：抽出 `MissionTransferSection`**（`flutter_shell/lib/mission/mission_transfer_section.dart`，751 行）——动作条 + 传输/校验/诊断展示 + 读/清/导出/上传/采用/开始命令 + 三个只被它使用的私有 widget，构造参数只有 `mission`、`controller`、`onRequestCommandConfirmation`；面板状态类 777 → 377 行，文件 3053 → 2362 行。验证：`flutter analyze` 无问题、`flutter test` 247 通过 / 4 跳过、`tools/preflight.sh` 全量通过。**行为差异（唯一）**：导出成功的内联提示随区块状态存放，而折叠会重建面板子树，因此折叠再展开后不再保留该提示（原来存在面板状态里）。**剩余**：设置页、任务列表、实体编辑弹窗仍在面板文件内，但都已是独立 widget，暂无新的拆分证据。 | 已决定 |

| KD-45 | 架构（A7） | `tools/check_ffi_contract.py`、`tools/ffi_unbound_baseline.json`、`bridge/ffi/API_CATALOG.md` | **已决定：防护已落地，存量逐轮清理**。公开导出 **280** 个、前端已绑定 **136** 个；未绑定的 **144** 个里，**17 个**在 `INTERNAL_SYMBOL_NOTES` 定性为内部能力、**111 个**有测试引用保持契约，剩余 **16 个**「无绑定、无定性、无测试引用」固化进 `tools/ffi_unbound_baseline.json`（136 + 17 + 111 + 16 = 280，**2026-10-12 更新**：本轮新增的三个参数文件导出 `mg_param_file_export`/`export_csv`/`import_csv` 被测试引用，测试引用数由 108 升到 111，债务相应从 19 收缩到 16，属允许方向）。**2026-10 死代码清理**：确认无调用方、无后续用途的 **34 个**旧导出已从源码物理删除并登记进 `RETIRED_SYMBOLS`（违反即报错）——旧任务管理簇 `ffi_mission_management.cc`（`mg_mission_add_item` 等 11 个 + `mg_mission_start` + `mg_mission_get_templates`）、旧围栏/Rally 直接编辑（`mg_geofence_*`、`mg_fence_clear`、`mg_rally_clear`）、旧连接（`mg_connection_connect_tcp`/`mg_send_heartbeat`/`mg_stream_request`，被 `mg_connection_open`/`mg_stream_set_interval` 取代）、旧串口读写（`mg_serial_get_config`/`set_baud`/`set_protocol`，被参数页取代）、旧 RC 专用校准（`mg_rc_calibrate_start/status/save`，被通用校准取代）、`mg_params_get_changed` 与 `mg_params_save_to_file`/`load_from_file`（被 `mg_param_file_*` 纯内容变换 + 前端 Dart 文件读写取代）。同时删除了旧 `MissionManager` 类，仍使用的静态工厂提取为 `core/mission/mission_item_factory.{h,cc}`。内部能力簇保持不变：`mg_px4_*`、`mg_sitl_*`、`mg_actuator_*`/`mg_rc_set_*`、`mg_config_validate_param`、`mg_error_string`。门禁只允许基线缩小：新增未定性导出会失败，若被重新绑定/被测试引用/被定性却仍留在基线也会失败；`--write-baseline` 走临时文件+rename 原子写。**处置方式**：后续每轮按「绑定 UI / 补测试引用 / 定性为内部能力 / 删除旧路径并登记 RETIRED」择一缩小基线与 ABI。功能视图与逐簇说明见 `docs/features/feature-inventory.md`。 | 已决定 |
| KD-46 | 架构（建议：按变更耦合拆分） | `flutter_shell/lib/map/amap_map_view.dart`（2116 行） | **已评估：暂不拆分，登记为下一个 god file 候选**。`git log --follow` 共 27 个提交，按块统计共变：只碰单个方法块的提交只有 3 次（其中 2 次是 `build()` 小改）；`_AMapMapViewState` ↔ `build` 共变 15/27、`build` ↔ `dispose` 7/27、`initState` ↔ `dispose` 6/27，与 `build` 共变最多的少数方法也只有 4 次里的 3 次（样本太小）。没有「两坨代码独立演进」的证据，按 A7 不为行数而拆。**触发条件**：出现独立变更原因（少数方法在多个提交里只它们一起动、且各自有独立测试）时，再登记拆分方案并跑全量 preflight。 | 已决定 |
| KD-47 | 架构（建议） | `flutter_shell/lib/mission/mission_document_models.dart:1`（`JsonMap` typedef） | **已决定：接受现状，暂不迁移**。`JsonMap = Map<String, Object?>` 定义在 mission 域，`survey_models.dart`、`payload_models.dart` 反向 import 它，属轻微的层次倒挂（Dart 侧没有 A1 那样的方向约束）。各域的 JSON 映射是各自的领域模型，只是形态相似，**不是同一逻辑多份实现**，因此不为消除相似而强抽公共 JSON helper（那才是过度工程）。**触发条件**：出现第三个域需要共享该 typedef、或跨域公共层自然成立时，再把它移到中立的共享位置。 | 已决定 |
| KD-48 | A7 / S5 | `core/src/geofence/geofence_manager.cc`、`bridge/ffi/ffi_geofence.cc`（`mg_geofence_check`）、`bridge/ffi/ffi_vehicle_info.cc`（`mg_vehicle_get_adsb_traffic`/`mg_vehicle_get_distance_sensors`） | **已决定：暂缓，登记为待深度开发的断链，不得按死代码删除**。三处都有完整实现但数据源断开：**(1) 围栏**：`GeofenceManager` 的 18 个公开方法（`add_point`/`remove_point`（单数，按索引删一个）/`update_point`/`clear_points`/`get_points`/`point_count`/`get_config`/`set_config`/`validate`/`is_inside_fence`/`distance_to_fence`/`calculate_area`/`calculate_perimeter`/`to_mission_items`/`from_mission_items`/`export_json`/`import_json`/`export_kml`，声明见 `core/include/core/geofence/geofence_manager.h:60-129`）除两组几何单测外生产零调用——围栏编辑已按 QGC/MP 的做法改走任务文档（mission 协议）路径；结果 `points_` 恒空，`mg_geofence_check` 的 `is_inside_fence` 永远返回 `inside_fence:false`。几何算法本身对齐 MP 的 `PolygonTools`（上游有），MP 的 `PolygonTools.isInside` 只用于可选的 FenceDist 插件（实时算飞机离围栏距离），QGC 无对应常驻接口。**(2) ADS-B**：MAVLink 层已实现 `ADSB_VEHICLE` 处理与 `on_adsb_vehicle` 回调、`ADSBTracker` 类完整，但回调从未被 `set`，`mg_vehicle_get_adsb_traffic` 仍是空数组存根。**(3) 距离传感器**：`mg_vehicle_get_distance_sensors` 空数组存根。**决定**：这些是「有实现、缺接线」的待开发能力，不是「无调用方、无未来用途」的死代码（对齐 A7 的区分）；按用户要求后续深度开发围栏（含围栏距离/侵入提示，参照 MP FenceDist）与 ADS-B 时一并接线，接线完成前相关 FFI 在 UI 上不得声称已支持（S5/S2 精神）。**另**：`test_sim_geofence.cc`/`test_geofence.cc` 对几何算法的测试是有效的回归保护，随功能保留。 | 待决定 |

## 已关闭

| ID | 规则 | 一句话 |
|---|---|---|
| KD-01 | S2 | 无 ACK 的命令不再当成功：`mg_command_set_global_origin`/`mg_command_landing_gear` 等改为等 `COMMAND_ACK`，引导类命令在头文件明确标注「只表示已发出」。 |
| KD-02 | S2 | `COMMAND_ACK` 匹配带来源系统（sysid=0 通配）；删除没有生产调用者的单槽 `ack_waiter`/`wait_for_ack`。 |
| KD-04 | S1 | 删除 core 里只发送、不等 ACK 且无调用者的 `arm`/`disarm`/`set_mode` 等；Dart 侧处于目标状态时不再重复下发，并给出提示。 |
| KD-05 | S1 | 串口读线程创建失败时关闭端口并返回失败，不再出现「链路已打开但没有读线程」。 |
| KD-06 | S1 | 目标注册失败记录带 route/sysid/compid 的错误日志，不再静默丢弃。 |
| KD-08 | S3（建议） | 任务上传与围栏/Rally 下发改用全应用统一的确认组件，取消则不下发（上传/清除在 `mission_transfer_section.dart` 的 `_uploadToVehicle`/`_clearVehicleMission`，围栏/Rally 在 `mission_planning_panel.dart` 的 `_FenceRallyCard`）。 |
| KD-10 | S3（建议） | RC 校准启动前先确认（`rc_calibration_view.dart:97,124`）。 |
| KD-11 | S3（建议） | 「保存并应用密钥」与「下发 SETUP_SIGNING」前先确认（`gcs_configuration_workspace.dart:240,280`）。 |
| KD-15 | S5 | 车辆状态改为与传输层状态同源，断链/心跳丢失后不再残留 `Connected`。 |
| KD-16 | S5 | 状态接口统一返回 `heartbeat_fresh`/`heartbeat_age_ms`，前端危险操作经新鲜度门控。 |
| KD-20 | A1 | core 与 bridge/ffi 拆成两个 CMake 目标，core 反向引用 bridge 头变为编译期报错。 |
| KD-21 | A2 | 去掉 `firmware_flasher.h` → `connection_manager.h` 的回边；新增模块环守卫测试。（所引用的 `firmware_flasher.{h,cc}` 已在本轮清洗中删除、尚未提交，CHANGELOG 暂无条目，见 `docs/workflow/HANDOFF-2026-10-12.md:56`） |
| KD-22 | A3、A4 | 剩余持锁发送改为锁内打包、出锁统一 flush（ADR-0005）；重入回调测试覆盖。 |
| KD-23 | A3 | `firmware_flasher.cc` 的进度回调改为锁内复制、锁外调用。（`firmware_flasher.{h,cc}` 已在本轮清洗中删除、尚未提交，CHANGELOG 暂无条目，见 `docs/workflow/HANDOFF-2026-10-12.md:56`） |
| KD-24 | A3 | 串口读线程只访问自持状态；join 超时不再 `TerminateThread`（避免 use-after-free）。 |
| KD-25 | S4、A3 | TCP 建立连接改为非阻塞 + 3 s 有界等待；串口写补齐短写/`EAGAIN` 与有界等待。 |
| KD-26 | 架构（建议） | 同步命令在头文件标注最长阻塞时间并指向 submit/poll 非阻塞形式。 |
| KD-31 | 测试（建议） | 等待条件改为轮询 + 超时，不再用固定 `sleep` 同步。 |
| KD-32 | 测试（建议） | CI 后端/前端改调 `tools/preflight.sh`，不再自维护一份检查表。 |
| KD-33 | U3、U4 | token 检查扩到裸字重等规则；业务代码里的字面量字重/字号清理。 |
| KD-35 | U3 | 澄清 U3：`MgFonts` 内部的平台回退链是允许的原生例外（见 `ui.md`）。 |
| KD-37 | U1 | 地图新增「自定义瓦片源」（第 6 个选项），校验模板并本地持久化。 |
| KD-41 | S8 | 校准取消改为异步确认：收到该取消命令的 `COMMAND_ACK` 才落到 `Cancelled`。 |
| KD-42 | S6 | 通用前端代码里的固件参数字面量清零：后端插件新增语义角色表（`core/src/config/ParameterDescriptors.json` 的 `roles` 段按 `ardupilot`/`copter`/`plane`/`vtol` 等 profile 下发「角色→固件参数名」），经 `mg_config_get_param_roles`、Dart `paramRoles()` 进入 `ConnectionController.roleParam()`，PID、校准、飞行模式、RC 校准、解锁检查等页面改按语义角色取参数名；`tools/check_firmware_literals.py` 的非空基线机制保留，`tools/firmware_literals_baseline.json` 现为 `{}`。 |
