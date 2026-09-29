# 现状审计

> 本文是当前滚动审计，不再冻结某一次发布。历史快照保存在
> [`docs/audits/`](audits/)。

## 审计范围

- 代码基线：`v0.0.10`（commit `0c8b193`）之后再叠加 `fix:` 校准提交与本轮审计修订。
- 本文更新时间：2026-09-29（第三轮：关闭 `CAL-001`、`CAL-002`、`CAL-003`、`ARCH-001`、`LOG-001`、`LOCK-001`、`EVENT-001`、`TESTGUARD-001`、`SURVEY-001`、`INTEROP-001`、`TEST-001`、`TEST-002`、`TEST-003`、`TOOL-002`、`CONTRACT-001`、`CLEANUP-001`、`DEPRECATION-001`；`CI-001` 标记 `WONTFIX`；新增 `DEAD-001`、`VIDEO-001`）。
- 当前分支：`main`。
- 本地门禁由 `tools/preflight.sh --no-macos-build` 加单独一次 `flutter build macos --debug` 实跑得出。
- 本审计不访问远端。CI 是否真正运行需要单独检查 GitHub Actions，不能由本地门禁推断。
- 本轮修订同时修复了 `tools/audit_status.sh`（详见 `TOOL-001`）。

历史 v0.0.8 快照：

- [2026-09-29 · v0.0.8](audits/2026-09-29-v0.0.8.md)

## 1. 当前门禁

以下为 2026-09-29 在本地工作树（`v0.0.10` 之后）上实跑的结果：

| 检查 | 结果 |
| --- | --- |
| 补丁格式（`git diff --check`） | 通过 |
| UI token 扫描 | 10 项自检通过，未发现绕过 token 的用法 |
| Flutter 静态分析 | No issues found |
| Flutter 测试 | **12/12 通过**（4 个测试文件） |
| 后端 offline/simulation | **34/34 通过**（offline 21 + simulation 13，18.4s） |
| macOS debug 构建 | **成功**（`flutter build macos --debug`，2026-09-29） |
| `tools/preflight.sh --no-macos-build` | 全部通过 |

说明：`preflight.sh` 用的是 `--no-macos-build`，所以 macOS debug 构建单独执行了一次；
本轮改动含 `flutter_shell/lib/configuration_workspace.dart`，该构建已覆盖这条 UI 改动。

当前门禁覆盖任务文档编译、上传和回读模拟，但仍不覆盖真实飞控写任务。

## 2. 可复现统计

FFI 消费情况由以下命令生成，不再手工维护：

```bash
tools/audit_status.sh
```

脚本只读取本地仓库，不访问网络。它统计：

- `ffi_api*.h` 中实际声明为函数的 `mg_*` 导出。
- Flutter Dart 字符串字面量中实际调用的 `mg_*`。
- C++ 测试中实际调用的 `mg_*`。
- 按领域头拆分的前端使用、测试触达和无人区。

脚本现在只依赖 POSIX 工具（`find` / `awk` / `sort` / `comm`），不再依赖 `ripgrep`。
详见 `TOOL-001`：此前依赖 `rg` 且用 `|| true` 吞掉失败，在缺少 `rg` 的环境会静默产出全 0 的假数据。
测试维度现按裸符号匹配，函数指针形式的 FFI 引用也会计入（见 `TOOL-002`）。

完整本地验收：

```bash
tools/preflight.sh
```

## 3. FFI 契约面

当前自动统计：

| 分类 | 数量 |
| --- | ---: |
| FFI 导出 | 281 |
| 前端实际使用 | 48 |
| 测试触达 | 194 |
| 既无前端调用、也无测试触达 | 86 |

按领域头拆分（三列互斥，逐行求和等于导出数）：

| 领域头 | 导出 | 前端在用 | 仅测试 | 两者皆无 |
| --- | ---: | ---: | ---: | ---: |
| configuration | 43 | 5 | 21 | 17 |
| connection | 21 | 6 | 10 | 5 |
| core | 12 | 3 | 8 | 1 |
| environment | 5 | 0 | 4 | 1 |
| firmware | 7 | 3 | 3 | 1 |
| flight | 32 | 0 | 32 | 0 |
| geofence | 16 | 0 | 4 | 12 |
| logging | 22 | 0 | 7 | 15 |
| mission | 40 | 15 | 10 | 15 |
| params | 20 | 6 | 5 | 9 |
| payload | 17 | 0 | 13 | 4 |
| telemetry | 46 | 10 | 30 | 6 |

说明：

- 导出数包含新增的任务规划 FFI，因此不再等于 v0.0.8 快照中的 266。
- “无人区”不等于可删除代码。专用公开面仍需按 `API_CATALOG.md` 的稳定性规则处理。
- 完整 86 个接口清单可由 `tools/audit_status.sh` 重新生成。
- 早期版本的逐域表格三列不互斥（“仅测试”含前端已用的项），已在修正 `TOOL-001` 时一起修好。

## 4. 当前问题登记

### ARCH-001：core 逆向依赖 bridge

- 状态：`RESOLVED`（2026-09-29）
- 实测（修复前）：`core/include/core/vehicle/vehicle.h` 直接 include
  `"telemetry_cache.h"`，而该文件位于 `bridge/ffi/`；根因是 `mgcore` 把
  `bridge/ffi` 挂到了 PUBLIC include 路径，裸文件名 include 因此能命中。
- 修复：把 `telemetry_cache.h` 连同它依赖的 `json_builder.h`、
  `json_serializers.h`、`param_json_serializers.h`、
  `sensor_health_json_serializers.h`、`telemetry_freshness.h` 一起下沉到
  `core/include/core/telemetry/`，命名空间改为 `mg::core::telemetry`。
  `bridge/ffi/` 保留同名转发头，用 `using` 把符号引入 `mg::bridge`，
  因此 30 多个 FFI 调用点无需改动。
- 回归保护：`test_bridge_architecture_guards.cc` 新增
  `core does not depend on bridge` 组，遍历 `core/include` 与 `core/src`，
  断言没有任何文件 include `bridge/` 路径，并校验 Vehicle 用的是 core 侧头文件。
- 验证：后端 34/34 通过（含新增断言）；`mgcore` 与 `mgcore_static` 均编译通过。

### CAL-001：校准读回失败被判成成功

- 状态：`RESOLVED`（2026-09-29）
- 实测：`core/src/calibration/calibration_service.cc` 的 `handle_success()` 只在
  `quality == Poor` 时判失败。参数读回失败时 `verified == false`，但质量仍是默认档，
  于是校准被报成成功——UI 会显示“校准完成”，实际飞控参数根本没校验过。
- 修复：`!verified` 一律判 `Failed`，并在原因为空时补默认文案。
- 相关修正：`radio_calibration` 的 `MAV_RESULT_ACCEPTED` 由 `Success` 改为 `Calibrating`
  （ACK 只代表命令被接受，遥控器行程仍需打杆验证）；`mg_calibration_get_history`
  的 type 上界由 3 改成 4，与 `API_CATALOG.md` 的 0..4 契约一致。
- 验证：`test_sim_calibration` 75/75 通过，含新增的“无参数读回必须 Failed”回归用例。

### CAL-002：guided point 经纬度按 COMMAND_INT 的量纲塞进 COMMAND_LONG

- 状态：`RESOLVED`（2026-09-29）
- 实测：`bridge/ffi/ffi_flight_commands.cc` 的 `mg_command_guided_point` 走命令队列
  （`send_command_queued` → `COMMAND_LONG`），却把 `lat*1e7` / `lon*1e7` 当成
  param5/param6。COMMAND_LONG 下 `MAV_CMD_DO_REPOSITION` 的 param5/param6 是「度」，
  飞控会自己再做一次 ×1e7，于是坐标溢出成垃圾值；`float` 也存不下 `399042000`。
- 影响：这是导航类安全指令，一旦有 UI 接上就会让飞机飞向错误坐标（当前该入口未被
  UI 使用，属于潜伏缺陷）。
- 修复：param5/param6 改为传「度」；需要 1e7 精度的路径保留在
  `MavlinkAdapter::cmd_guided_point`（走 COMMAND_INT）。
- 回归：`test_sim_commands.cc` 断言 `DO_REPOSITION` 落到 `lat/lon degE7`
  （已验证：改回旧实现该用例立即 FAIL）。

### CONTRACT-001：前端参数写入入口未进入 Catalog

- 状态：`RESOLVED`（2026-09-29）
- 实测（修复前）：`mg_config_read_param`、`mg_config_write_param`、`mg_config_write_params`
  已声明和实现，但未出现在 `API_CATALOG.md`；Catalog 只把 `mg_params_set` 列为参数写入入口，
  而 Flutter 走的是 `mg_config_*`。
- 处理：把 `mg_config_*` 正式登记为“Parameter write (UI) / Parameter metadata”首选入口，
  并写明与 `mg_params_set` 的差别（带描述库校验；不要求参数表已完整下载，类型缺失回退 REAL32）。
- 验证：`API_CATALOG.md` 的 Preferred Entry Points 现包含这三个符号；
  `test_sim_ui_paths` 覆盖“读回描述元数据 + 写入转发 PARAM_SET + 空名拒绝”。

### UI-001：聚合诊断接口未接 Flutter

- 状态：`OPEN`
- 未接入口：
  - `mg_connection_get_diagnostics`
  - `mg_telemetry_get_dashboard_snapshot`
  - `mg_system_get_diagnostics`
- 当前 Flutter 自行拼接 `mg_connection_get_session_state`、`mg_telemetry_get_freshness` 和状态接口。
- 影响：重复聚合逻辑，且容易遗漏 freshness、last-error 和会话一致性。

### UI-002：长任务没有取消入口

- 状态：`OPEN`
- 未接入口：`mg_operations_get_snapshot`、`mg_operations_cancel`。
- 影响：参数同步和任务上传只能等待，不能从统一状态视图取消。

### TEST-001：5 个前端在用接口无测试

- 状态：`RESOLVED`（2026-09-29）
- 接口：`mg_calibration_cancel`、`mg_params_get_by_name`、`mg_serial_get_uart_names`、
  `mg_stream_set_interval`、`mg_vehicle_request_firmware_version`。
- 处理：新增 `core/tests/test_sim_ui_paths.cc`（label `simulation`），在
  `MockFlightController` 上覆盖请求编码、ACK 语义、取消语义和 FTP 回读失败契约。
- 附带修复：给 mock 增加最小 MAVLink FTP 应答器（ResetSessions / OpenFileRO /
  ReadFile / TerminateSession），使 `mg_serial_get_uart_names` 的
  `@SYS/uarts.txt` 下载路径可以在 0.5s 内离线复现，而不是等 3s×4 重试超时。
- 验证：`ctest -L simulation` 中的 `test_sim_ui_paths` 26/26 通过。

### TEST-002：安全敏感飞行指令无测试

- 状态：`RESOLVED`（2026-09-29，模拟链路部分）
- 接口：`mg_command_set_mode`、`mg_command_land`、`mg_command_rtl`、`mg_command_guided_point`，以及起飞高度和 loiter 半径读写。
- 处理：`test_sim_commands.cc` 新增 `SAFETY COMMAND ENCODING` 与
  `FLIGHT MODE STATE SETTERS` 两组，断言飞机指令的 MAVLink 编码、ACK 语义和
  参数范围校验。
- 剩余工作：真机验收仍待人工执行，模拟测试不能替代。

### TEST-003：批量参数变更集无验证

- 状态：`RESOLVED`（2026-09-29）
- 接口：`mg_params_prepare_change_set`、`mg_params_commit_change_set`。
- 风险点：去重、未知参数、未变化参数、逐项失败和 `_ENABLE` 最后写入。
- 处理：`test_sim_params.cc` 新增 4 组用例——未加载参数时拒绝、非数组/缺 value
  的畸形 JSON 拒绝、重名取最后一次且分别统计 unchanged/unknown、`_ENABLE`
  在所有依赖项之后写入；另外断言存在未解析项时 `success:false`。
- 验证：`test_sim_params` 39/39 通过。

### DEPRECATION-001：兼容接口没有退役状态

- 状态：`RESOLVED`（2026-09-29）
- 修复前：`API_CATALOG.md` 虽列了兼容别名，但缺少退役规则；`mg_rc_calibrate_save`
  无条件返回成功，`mg_log_list` 无条件返回 `success:true` + 空数组。
- 处理：
  - `API_CATALOG.md` 新增 `Deprecation policy` 段落，写明兼容名冻结、不新增行为、
    只在 ABI-major 版本移除，并说明为什么暂不加编译器 `deprecated` 属性。
  - 兼容表补上 `mg_log_list` 与 `mg_rc_calibrate_save` 两行及替代关系。
  - `mg_log_list` 改为读取真实日志缓存；`mg_rc_calibrate_save` 在校准进行中改为
    返回失败，不再谎报保存成功。
  - `mg_mission_request` 之前忽略 `mission_download` 的返回值、无条件返回成功，现已透传失败。
- 验证：`test_sim_ui_paths` 新增 `mg_log_list` / `mg_rc_calibrate_save` /
  `mg_mission_request` 三组用例（含“旧实现会失败”的对照验证）。

### CAL-003：校准历史在持锁路径写盘

- 状态：`RESOLVED`（2026-09-29）
- 实测：`CalibrationService::save_to_history()` 在持有 `mtx_` 时调用
  `persist_history()` 写文件。该路径由 MAVLink 回调驱动，属于仓库禁区里
  “持锁路径做阻塞 I/O”。
- 修复：锁内只置 `persist_dirty_` 标记并保存历史；真正的写盘移到
  `fire_event_safe()`（锁已释放）里，先用短锁取历史快照，再在锁外写文件。
  `EventDispatchScope` 在所有回调返回后都会触发该路径，因此落盘时机基本不变。
- 验证：`test_sim_calibration` 新增 `Calibration history flush` 组，先删除历史文件，
  跑完一次校准后直接读磁盘断言 `MGCAL1` 记录与失败原因确实落盘（79/79 通过）。

### LOG-001：飞行日志声称采集 STATUSTEXT / 连接事件，但从来没接线

- 状态：`RESOLVED`（2026-09-29）
- 实测：`FlightLoggingService` 暴露 `on_statustext` / `on_connection_state`，
  `ffi_logging.cc` 的注释也写明“captures STATUSTEXT, connection events”，
  但 `core/src` 与 `bridge/ffi` 里没有任何地方调用这两个入口。
  结果是日志页永远只有 `system` / `command` 两类事件。
- 修复：`Vehicle::wire_telemetry_callbacks()` 的 statustext 分支补上
  `flight_log_.on_statustext(...)`；`conn_.set_state_callback` 里补上
  `flight_log_.on_connection_state(...)`。
- 验证：`test_sim_ui_paths` 新增 `flight log captures STATUSTEXT` 组。
  已验证删掉接线该用例立即 FAIL。

### LOCK-001：飞行日志写回调在持锁路径调用

- 状态：`RESOLVED`（2026-09-29）
- 实测：`FlightLoggingService::append_event()` 持有 `mtx_` 时调用 `write_cb_`。
  该回调会去拿 `core->core_mtx` 并触发 FFI 事件，等于在 MAVLink 读线程上
  串起两把锁并调用外部回调——仓库禁区，也是锁序隐患。
- 修复：与 `CAL-003` 同一套路：锁内只记账 + 组装 JSON，锁外再调用回调。
- 验证：`test_sim_ui_paths` 的日志用例覆盖事件仍能送达。

### EVENT-001：MG_EVENT_WAYPOINT_REACHED 声明了却从不发出

- 状态：`RESOLVED`（2026-09-29）
- 实测：`MG_EVENT_MISSION`(6) 与 `MG_EVENT_WAYPOINT_REACHED`(26) 都在公开枚举里，
  但 `core/src` 与 `bridge/ffi` 里没有任何 `post_event`/`fire_event` 使用它们；
  `MavlinkAdapter::on_mission_reached` 也没有任何订阅者。订阅航点事件的客户端
  会一直收不到消息，且没有报错。
- 修复：`Vehicle::wire_telemetry_callbacks()` 注册 `mav->on_mission_reached(...)`，
  发出带 `seq` 的 `MG_EVENT_WAYPOINT_REACHED`。
- 附带修正：`MockVehicleTestEnv` 之前没给 fallback vehicle 接事件回调，
  与真实会话（`open_vehicle_session` 会接线）行为不一致，导致事件路径测不到；
  已按生产逻辑补齐。
- 验证：`test_sim_ui_paths` 新增 `MG_EVENT_WAYPOINT_REACHED` 组，
  断言事件发出且 `seq` 正确。已验证删掉接线该用例立即 FAIL。

### TESTGUARD-001：架构守卫读不到文件时会静默空转

- 状态：`RESOLVED`（2026-09-29）
- 实测：`test_bridge_architecture_guards.cc` 的 `read_file()` 打不开文件时返回空串，
  而绝大多数断言是“不应该包含 X”。文件一改名/搬走，守卫全部空转通过——
  本轮搬迁 `telemetry_cache.h` 时就真实发生过。
- 修复：`read_file()` 打不开文件时直接记一次失败并打印路径；两处已搬迁文件的
  守卫再加 `CHECK(!content.empty(), "... was actually read")`。
- 验证：临时把路径改成不存在的文件，确认立即 FAIL 并打印 `cannot open ...`。

### MISSION-001：Flutter 任务规划层尚未连接后端

- 状态：`OPEN`（只剩前端）
- 更正（2026-09-29）：本条早先写的“Dart 侧不依赖 `ConnectionController`”已过期。
  `ConnectionController implements MissionDocumentBackend`，`missionPlanning`
  控制器走的就是真实 FFI（`mg_mission_doc_*`），并不是纯本地模型层。
- 实际缺口：`MissionDocumentController` 目前没有任何页面或组件使用，
  也就是“后端已通、UI 未接”。mission 领域 40 个导出里仍有 15 个落在无人区。
- 下一步：设计任务规划页面（属新增前端能力），按用现有 `missionPlanning` 控制器接线。
- 契约提醒：上传是“启动传输”语义，完成后需要用 `mg_mission_get_progress`
  轮询再 `mg_mission_doc_verify`，`API_CATALOG.md` 已写明。
- 后端补充（2026-09-29）：`survey_grid` / `corridor` 之前在校验里被直接拒绝
  （“Parameterized survey entities are not implemented yet”）。现已实现为参数化
  实体：文档存设计意图，编译期由 `SurveyCalculator` 展开，`source_entity_ids`
  逐项指回源实体，`estimate` 也按展开结果统计。详见 `SURVEY-001`。

### CLEANUP-001：组件陈列页无人引用

- 状态：`RESOLVED`（保留并写明用途，2026-09-29）
- 实测：`flutter_shell/lib/component_gallery_demo.dart`（1,047 行）没有被任何页面或测试 import。
- 影响：它仍持有 `MacosColors.*` 引用，会让 token 扫描的豁免范围长期存在。
- 处理：确认仍有展示价值（对照 macos_ui 原始组件与色板，是前端选型时的参考件），
  因此保留而不是删除：文件头已注明“开发工具、不进 release 包、main.dart 不引用”，
  并已在 `tools/check_ui_tokens.dart` 的豁免清单中。
- 验证：`main.dart` 只 `runApp(const MgcsApp())`，不引用该文件；
  `flutter analyze lib test` 通过，说明该独立 target 仍可编译。

### SURVEY-001：参数化测绘实体未接入文档

- 状态：`RESOLVED`（2026-09-29）
- 实测（修复前）：`mission_document.cc` 的校验对 `survey_grid` / `corridor`
  直接报 `entity.type_unsupported` + “Parameterized survey entities are not
  implemented yet”，所以文档里根本放不下测绘区域，只能用旧 survey API 单独生成。
- 修复：新增 `MissionSurveySpec`，实体 JSON 增加 `survey` 子对象；校验改为真正的
  参数校验（经纬度、尺寸、高度、速度、重叠率、相机参数、走廊起终点不同）；
  编译期按类型调用 `SurveyCalculator::generate_grid / generate_cross_grid /
  generate_corridor_scan` 展开，逐项写入 `source_entity_ids`；
  `estimate` 用展开结果统计距离、时间、航点与照片数。
- 验证：`test_mission_document` 新增 `Parameterized survey entities` 与
  `Survey entity through FFI` 两组共 20 条断言（101/101 通过），覆盖
  验证、编译展开、序列化往返、走廊终点缺失拒绝、宽度为 0 拒绝、FFI 全链路。

### INTEROP-001：任务文件互操作未实现

- 状态：`RESOLVED`（2026-09-29，`.waypoints` 部分）
- 实测（修复前）：`core` 只有自定义 `.mgcad.json`，Mission Planner / QGC 通用的
  `.waypoints`（`QGC WPL 110`）无法导入导出，`MissionManager` 的 csv/json 导出
  是另一套字段，不能直接喂给 MP。
- 修复：新增 `core/.../mission_interchange.{h,cc}`，实现 `QGC WPL 110` 读写，
  格式对齐 Mission Planner 自身的写出实现（第 0 行 home、任务项 seq 从 1 起、
  lat/lon 十进制度）；FFI 暴露 `mg_mission_doc_export_format` /
  `mg_mission_doc_import_format`，格式名参数化，后续可加 `.plan`。
- 验证：`test_mission_document` 新增 `.waypoints interchange` 与
  `.waypoints interchange through FFI` 两组，覆盖表头、home 行、往返、
  导入后校验、缺表头/空行拒绝、格式名不合法拒绝。
- 剩余：QGC `.plan`（JSON，含 geoFence / rallyPoints）与 Rally 文档尚未实现。

### DEAD-001：固件刷写是死代码，状态接口返回硬编码值

- 状态：`OPEN`（低危，未修）
- 实测：`FirmwareFlasher`（`download_firmware` / `flash_firmware`）只在
  `CMakeLists.txt` 里被编译，`core/src`、`bridge/ffi` 里没有任何调用者；
  两个方法都只 `report_progress(..., "not implemented")`。
  `mg_firmware_flash` 诚实返回 `MG_ERROR_NOT_SUPPORTED`，但
  `mg_firmware_flash_status` 直接返回硬编码的 `{"flashing":false,"progress":0}`，
  不看 `FirmwareFlasher::is_flashing()`。
- 危害：目前只是死代码 + 一个永远报“空闲”的状态接口，不会造成误动作。
- 为什么没修：刷写要实现 HTTP 下载 + bootloader 握手，属于功能开发；
  只改状态接口反而会让人以为刷写可用。
- 建议：要么删除 `FirmwareFlasher` 与状态接口，要么等真正实现刷写时一起接通。

### VIDEO-001：FFmpegDecoder 的帧回调存在数据竞争

- 状态：`OPEN`（未验证，未修）
- 实测：`FFmpegDecoder::set_frame_callback()` 直接赋值 `frame_cb_`，
  解码线程在循环里直接读同一个成员，类里没有任何互斥保护。
- 危害：启用 FFmpeg 构建时属于数据竞争（UB），换回调可能与解码循环读到半初始化状态。
- 为什么没修：本机 `build_check` 未开 `MGCORE_HAS_FFMPEG`，改了无法编译验证，
  属于“改了也不敢说对”。需要能编 FFmpeg 的环境再动。
- 建议：加一把回调锁，set 与读各自持锁，读侧取副本后在锁外调用。

### TOOL-001：审计脚本缺失依赖时静默产出假数据

- 状态：`RESOLVED`（2026-09-29）
- 原实现：`tools/audit_status.sh` 用 `rg` 提取符号，且以 `|| true` 吞掉失败。
  在缺少 `ripgrep` 的机器上，前端和测试两维全部统计为 0，脚本仍正常退出，
  于是报告出“281 个导出全是无人区”的错误结论。
- 修复：改用 `find` + `awk`，脚本现只依赖 POSIX 工具。
- 附带修复：逐域表格的“仅测试”列原先包含前端已用的项，三列不互斥、行和超过导出数；
  现改为 `导出 - 前端在用 - 两者皆无`，已校验 12 行全部自洽。
  - 验证：`tools/audit_status.sh` 在无 `rg` 环境下输出 281 / 48 / 170 / 105，与独立统计一致。

### TOOL-002：测试维度漏统计函数指针形式的 FFI 引用

- 状态：`RESOLVED`（2026-09-29）
- 原实现：测试维度的 `awk` 只匹配 `mg_xxx(` 调用点。测试把 FFI 入口当函数指针
  传递时（例如 `env.call_with_pump(mg_serial_get_uart_names)`）没有尾随 `(`，
  于是该符号被误报为“无人区”。
- 修复：`extract_tests.awk` 改为匹配裸 `mg_[A-Za-z0-9_]+` 记号；伪符号
  （`mg_core_t`、`mg_socket_t` 等类型名）在与导出表求交集时自然被丢弃。
- 影响（仅这一项修复带来的变化）：测试触达 170 → **179**，无人区 105 → **101**；
  新计入的 5 个符号是
  `mg_serial_get_uart_names`、`mg_log_request_list`、`mg_terrain_query`、
  `mg_flight_get_takeoff_altitude`、`mg_flight_get_loiter_radius`。
- 验证：修复后脚本输出 281 / 48 / 179 / 101。叠加本轮 `TEST-001` / `TEST-002` /
  `TEST-003`、事件/日志用例后，当前值为 **281 / 48 / 194 / 86**（以第 3 节与脚本实跑为准）。

### CI-001：远端 CI 运行状态未在本地审计确认

- 状态：`WONTFIX`（2026-09-29，用户明确指示不再跟踪）
- 原因：本审计不访问远端。
- 已知历史：v0.0.8 时 CI 因私有仓库计费被拦截，作业从未真正运行。
- 说明：此项不再列为待办，也不影响本地门禁结论；若将来 CI 恢复且需要，再另开条目。

## 5. 已解决或已变化的事项

以下是 v0.0.8 快照之后确认发生变化的项目：

| 项目 | 当前状态 |
| --- | --- |
| 版本号 | v0.0.8 → **v0.0.10**（commit `0c8b193`） |
| 后端门禁 | 31 → **34 个测试**（offline 21 + simulation 13） |
| Flutter 测试 | 3 → **12 个**（4 个测试文件） |
| `core/src` 规模 | 54 文件 / 14,854 行 → **57 文件 / 16,664 行** |
| `bridge/ffi` 规模 | 38 `.cc` → **40 `.cc`**（头仍为 29） |
| `core/tests` | 30 → **33**（新增 `test_sim_ui_paths.cc`） |
| 前端 Dart | 16 + 1 文件 / 9,116 行 → **21 + 4 文件 / 11,821 行** |
| UI token 迁移 | 已完成业务代码收敛，并加入自动检查 |
| 内置字体 | MiSans 三字重 + LICENSE（19M）已入库，ADR 0007 记录该决定 |
| 校准 | 新增 `radio_calibration` 策略（RC 校准）；修正“读回参数失败仍判成功”的校验漏洞（`CAL-001`） |
| 导航指令编码 | `mg_command_guided_point` 的 `DO_REPOSITION` 经纬度单位修正为「度」（`CAL-002`） |
| 模拟测试 | 新增 `test_sim_commands` 的安全指令编码与飞行模式状态用例；`flight` 领域已无“无人区”接口 |
| 参数变更集 | 新增 `test_sim_params` 的批量变更集用例：去重、未知/未变化统计、`_ENABLE` 最后写入（`TEST-003`） |
| 组件画廊 | `component_gallery_demo.dart` 明确为开发工具（不进 release、已列入 token 豁免），保留而非删除（`CLEANUP-001`） |
| 兼容入口 | `mg_log_list` 返回真实日志、`mg_rc_calibrate_save` 不再谎报成功、`mg_mission_request` 透传发送失败；`API_CATALOG.md` 补退役策略（`DEPRECATION-001`） |
| 校准落盘 | 校准历史写盘移出持锁路径，改在 `fire_event_safe` 的锁外快照写文件（`CAL-003`） |
| 飞行日志 | STATUSTEXT 与连接事件真正接入日志服务（`LOG-001`）；写回调移出持锁路径（`LOCK-001`） |
| 任务规划 | `survey_grid` / `corridor` 成为参数化实体，编译期由 `SurveyCalculator` 展开（`SURVEY-001`）；新增 `.waypoints` 互操作（`INTEROP-001`） |
| 事件通道 | `MG_EVENT_WAYPOINT_REACHED` 以前从不发出，现已接线并带 `seq`（`EVENT-001`） |
| 架构守卫 | `read_file` 打不开文件时直接判失败，避免守卫静默空转（`TESTGUARD-001`） |
| 校准测试 | 新增读回失败回归用例；mock 补 `INS_GYROFFS_*` / `INS_ACCOFFS_*` 参数 |
| 飞行模式分档 | 多旋翼/固定翼 PWM 分档拆分，无效 RC 值不再误判档位 |
| Mock FTP | `MockFlightController` 增加最小 MAVLink FTP 应答器，支撑 `mg_serial_get_uart_names` 离线复现 |
| 协作脚本 | `preflight.sh`、`install-hooks.sh`、`release.sh` 已实现 |
| 审计脚本 | 新增 `audit_status.sh`，并修复 `TOOL-001`、`TOOL-002` |
| FFI 契约目录 | `API_CATALOG.md` 把 `mg_config_read_param` / `mg_config_write_param(s)` 登记为参数读写首选入口（`CONTRACT-001`） |
| pre-commit | 校验暂存快照，拒绝未暂存和相关未跟踪文件 |
| CMake/CTest | 最低版本 3.20，空测试会失败 |
| 任务规划 | 新增 MissionDocument、命令目录、校验、编译、估算、上传和回读 |
| Flutter 桥接 | 新增任务模型、后端接口和 `missionPlanning` 控制器（尚未连 FFI，见 `MISSION-001`） |
| 电机控件 | `mg_range_bar.dart` 删除，替换为 `mg_motor_output_bar.dart` |
| 新增文档 | `COLLABORATION.md`、`MISSION_PLANNING.md`、`FONT_ALTERNATIVES_RESEARCH.md`、`MULTIVEHICLE_CONNECTION_RESEARCH.md`、ADR 0007 |
| 依赖方向 | `telemetry_cache.h` 及 4 个序列化器下沉到 `core/include/core/telemetry/`，bridge 只留转发头；新增 core→bridge 方向断言（`ARCH-001`） |
| Windows 串口 | 修复 ANSI/UTF-8 越界写、补齐 DCB/清理/部分写处理、加强读线程退出 |
| Windows 飞控发现 | USB 遥测电台 `SiK Radio` 现在也会标记为飞控候选 |
| Windows 打包 | Flutter Windows 构建显式继承生成器/架构，安装阶段校验 DLL 和数据文件 |
| Windows CI | 新增 `windows-shell` job，构建并上传 Windows debug 产物 |

## 6. 建议顺序

1. 把 `MISSION-001` 的后端任务规划真正接到 Flutter（先定边界，再绑 FFI）。
2. 接入 `UI-001` / `UI-002` 聚合诊断和长任务取消。

剩余的 `MISSION-001` / `UI-001` / `UI-002` 都是**新增前端能力**，不是缺陷：
后端接口、契约和测试都已就绪，缺的是页面本身。

`DEAD-001`（固件刷写死代码）与 `VIDEO-001`（FFmpeg 回调数据竞争）是已知但
本机无法安全验证的项：前者需要实现完整刷写功能才有意义，后者需要能编
FFmpeg 的构建环境。两者都不影响当前默认构建。

## 7. 维护规则

1. 当前事实写在本文；历史结论写入 `docs/audits/`。
2. 统计数字必须能由 `tools/audit_status.sh` 复现。
3. 问题必须有稳定 ID，并使用 `OPEN` / `RESOLVED` / `STALE` / `ACCEPTED-RISK` 状态。
4. 已解决问题保留一行状态和验证提交，不删除历史审计。
5. 本地门禁与远端 CI 分开记录，不以本地绿色代替远端运行。

---

## 落款

| 项 | 内容 |
| --- | --- |
| 编写 | WorkBuddy（AI 助手） |
| 日期 | 2026-09-29（第二轮更新） |
| 依据版本 | tag `v0.0.10` 之后的本地工作树（含 `CAL-001` 修复与新增仿真测试） |
| 数据来源 | 本机实跑 `tools/preflight.sh --no-macos-build` 与 `tools/audit_status.sh`，非估算 |
| 本轮改动 | 关闭 `CAL-001`、`CAL-002`、`CAL-003`、`ARCH-001`、`LOG-001`、`LOCK-001`、`EVENT-001`、`TESTGUARD-001`、`SURVEY-001`、`INTEROP-001`、`TEST-001`、`TEST-002`、`TEST-003`、`TOOL-002`、`CONTRACT-001`、`CLEANUP-001`、`DEPRECATION-001`；`CI-001` 转 `WONTFIX`；新增 `DEAD-001`、`VIDEO-001`；刷新门禁与统计 |
| 复核状态 | **未经人工复核**；本地门禁（含 macOS debug 构建）已实跑通过 |

> 按 [协作流程](COLLABORATION.md) 中“不要由实现者自证”的约定，本文所列修改建议
> 应由另一人复跑同一验收命令后再采纳。
