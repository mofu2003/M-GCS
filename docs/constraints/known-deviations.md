# 已知偏差

登记当前代码与规则不符的条目。本文件只登记，不在此处修复。修复时删除对应条目，并在提交说明中引用编号。

- 规则列为约束编号（S、A、F、T、U、R、P）的，属于**约束偏差**，必须修复，或在本文件中登记计划。
- 规则列标注"建议"的，属于**建议偏差**，不阻断合并，但需要在提交说明中写明理由。

状态取值：`待修复`、`待决定`、`待验证`、`待核对`、`待补充`。

| ID | 规则 | 位置 | 现状 | 状态 |
|---|---|---|---|---|
| KD-01 | S2 | `core/src/mavlink/mavlink_senders.cc:682-700`（`send_command_long_status`）；`core/include/core/mavlink/mavlink_adapter.h:508`（`cmd_return_to_launch`） | 核心层的布尔命令仍返回**「已发出」**：`set_mode`/`arm_disarm`/`cmd_*` 最终都是 `send_command_long` 的发送结果，飞控是否接受要等 ACK。FFI 侧不受影响（走 `cmd_result_queued`，按 ACK 判结果）。另：`FlightModeManager::return_to_launch/land/loiter` 全仓无调用方，属未接线的死代码 | 待修复 |
| KD-02 | S2（建议：目标系统匹配） | `core/src/mavlink/mavlink_adapter_handlers.cc:255-260`；`core/src/mavlink/command_queue.h:137,146` | **部分修复**：ACK 匹配已带来源系统（`find_by_command(sysid, compid, command)`、`handle_ack(m.sysid, ...)`，`handlers.cc:246` 传入 `m.sysid`），所以「别的飞机的 ACK 不会结束本机命令」已成立。仍缺：命令按 (sysid, compid, command) 匹配而非 ticket（同命令并发时无法分辨，靠 FFI 层拒绝重复命令规避）；legacy `ack_waiter` 仍是**单槽**，每条 ACK 都覆盖 `ack_waiter.pending`（`handlers.cc:257`），`wait_for_ack` 的旧调用方可能读到别人的 ACK | 待修复 |
| KD-03 | S1 | `core/src/mavlink/mavlink_senders.cc:1152-1162` | 排队发送失败时只把 `queued_entry->state` 置为 `TimedOut`（`ack_result = 4`）并 `cv.notify_all()`，**不调用该条目的 `result_cb`**；依赖回调的调用方只能靠轮询或自己超时 | 待修复 |
| KD-04 | S1 | `core/src/flight/flight_mode_manager.cc:196-212` | `arm()` 在已解锁时、`disarm()` 在未解锁时都**静默 `return false`**（无日志、无说明）；目标状态已达成却报失败，调用方无法区分「已经是这个状态」和「命令被拒」 | 待修复 |
| KD-05 | S1 | `core/src/transport/tcp_link.cc:160`；`udp_link.cc:165,187` | 吞掉线程启动错误，链路显示已打开但没有读线程 | 待修复 |
| KD-06 | S1 | `core/src/transport/link_session.cc:106`；`core/src/mavlink/mavlink_router.cc:194`；`core/src/vehicle/multi_vehicle_manager.cc:248` | 忽略路由、添加目标、添加飞行器的返回值 | 待修复 |
| KD-08 | S3 调整为建议（写入类操作） | `flutter_shell/lib/mission/mission_planning_panel.dart:595-599`（任务上传，调用点 `:350`、`:456`）；`:850-870`（围栏/集结点下发卡片）；`:908-912`（下发按钮） | 任务上传、围栏与集结点上传点击即下发，路径上无确认；同文件清除与读取都用了 `confirmMissionAction`，说明组件可用而未用在写入口 | 建议偏差，待决定 |
| KD-09 | S3 调整为建议（写入类操作） | `flutter_shell/lib/configuration/widgets/parameter_value_control.dart:55-62` | 参数写入无确认；安全相关参数建议优先加确认 | 建议偏差，待决定 |
| KD-10 | S3 调整为建议（非飞行状态命令） | `flutter_shell/lib/configuration/calibration/rc_calibration_view.dart:95` | RC 校准启动无确认 | 建议偏差，待决定 |
| KD-11 | S3 调整为建议（写入类操作） | `flutter_shell/lib/gcs_configuration_workspace.dart:231`（保存并应用 → `upsertSigningKey`）、`:263`（下发到飞控 → `sendSetupSigning`）；按钮 `:488`、`:495` | 签名密钥下发（SETUP_SIGNING）与「保存并应用」无确认，先执行后提示。注意 SETUP_SIGNING 会改变链路信任状态，密钥不匹配可使 GCS 与飞控失联，而它不是「已记录的决定」第 1 条枚举的写入类操作 | 建议偏差，待决定 |
| KD-13 | S3 | `flutter_shell/lib/flight/flight_mode_panel.dart:22` | `FlightModeFloatingPanel` 为死代码，其解锁对话框不可达；需删除或接入 | 待决定 |
| KD-15 | S5 | `core/src/vehicle/vehicle.cc:254-281` | 链路断开时 `install_connection_state_callback` 只写飞行日志并发 `MG_EVENT_CONNECTION` 事件，**没有更新 `Vehicle::state_`**；`state_` 只在显式连接/移除路径（`:122`、`:190`、`:323`）被置为 `Disconnected`。因此心跳丢失后读 `state_` 的一方仍看到 `Connected`（界面若走事件或遥测新鲜度则不受影响） | 待修复 |
| KD-16 | S5 | `core/src/flight/flight_mode_manager.cc:54` | `armed_` 只在心跳更新路径被赋值，类内没有任何链路断开入口（头文件中无 link/disconnect 相关接口），断链后 `armed_` 保持旧值。Dart 侧的 `telemetryStale` 只覆盖界面显示（顶栏显示「—」），核心状态仍会给出过期的解锁结论 | 待修复 |
| KD-20 | A1 | `CMakeLists.txt:251-254`；`core/tests/test_bridge_architecture_guards.cc:220` | `bridge/ffi` 位于 core 的公共 include 路径上；守卫测试只匹配 `bridge/` 前缀 | 待修复 |
| KD-21 | A2 | `core/include/core/transport/connection_manager.h:28`；`core/include/core/mavlink/mavlink_adapter.h:8`；`core/include/core/firmware/firmware_flasher.h:17` | `transport → mavlink → firmware → transport` 存在依赖环 | 待修复 |
| KD-22 | A3、A4 | `core/src/mavlink/mavlink_senders.cc:676-681`；`core/include/core/platform/platform_mutex.h:135-142` | 发送回调在 adapter 锁内执行；Windows（递归临界区）与 POSIX（`std::mutex`）语义不同 | 待修复 |
| KD-23 | A3 | `core/src/firmware/firmware_flasher.cc:52-96` | `FirmwareFlasher::flash_firmware` 持 `mtx_` 时经 `report_progress` 调用外部 `progress_cb_`（A3：不在锁内调用外部回调）；同文件的 `download_firmware:54` 是反向问题——**不持锁**就读同一个 `progress_cb_`，属数据竞争。两处都位于占位实现中（flash/download 均直接返回失败） | 待修复 |
| KD-24 | A3 | `core/src/transport/serial_link.cc:225` | 串口读线程 join 超时后使用 `TerminateThread` | 待修复 |
| KD-25 | S4、A3 | `core/src/transport/tcp_link.cc:212`；`serial_link.cc:294` | TCP 建立连接仍是**无超时的阻塞 `connect`**，且它在看门狗启动之前（`connection_manager.cc:347` → `:370`）；POSIX 串口写失败/短写/`EAGAIN` 不重试（Windows 分支有 partial-write 循环）。收发已有兜底：无有效 MAVLink 流量 5 s（首包 10 s）转 `HeartbeatLost` 并 reset，阻塞写也会被 shutdown 解除，故原描述「收发无超时」不准确 | 待修复 |
| KD-26 | 架构（建议：UI 线程上的 FFI 调用） | `bridge/ffi/ffi_flight_commands.cc:355-358`；`bridge/ffi/ffi_api_mission.h:12` | `mg_mission_start` 仍是同步等待最长 5 s（`max_wait_ms = 5000`）且头文件未标注耗时。但**「UI 线程上的调用」这一归因已失效**：生产路径统一走非阻塞 `mg_command_submit_for_vehicle` + `mg_command_poll`（`flight_command_ffi_backend.dart:206-207`），同步等待发生在 `std::async` 工作线程，Dart 侧已无该符号绑定 | 建议偏差，待修复 |
| KD-27 | 架构（建议：事件获取方式） | `flutter_shell/lib/connection/connection_controller.dart:1744-1751`（会话 750 ms、遥测）、`:1096`（校准 250 ms）、`:1528`（机队 750 ms）、`:3181`（波形 100 ms） | Dart 端仍用 `Timer.periodic` 轮询，未使用原生回调（`mg_set_event_callback` 在 Dart 侧 0 处引用，也无 `Pointer.fromFunction`/`NativeCallable`）。另 `refreshFlightMessages`（`:1948`）是拉取式轮询而非常规 Timer | 建议偏差，待决定 |
| KD-28 | T1、T2 | `core/tests/test_sim_commands.cc:233-235` | NAK 测试三条断言中，第三条 `json_has(result, "\"result\"") \|\| is_failure(result)` 恒真——飞控 NAK 走 `command_ack_json`，该 JSON 恒带 `"result"` 字段（`json_serializers.h:146`），所以这一条永远通过；测试也没有钉住拒绝码（未断言 `result==1` 或 `result_str`）。注意另两条（非空、非 success）是真断言，故不能概括为「整条测试无效」 | 待修复 |
| KD-29 | T2 | `core/tests/test_ffi_contract.cc:317-322`；`test_ffi_comprehensive.cc:138-149, 252` | `CHECK(true, ...)` 仍在；被注释掉的命令测试仍在，**且注释给出的理由「not linked」已过期**——FFI 源就在 `mgcore_static` 里（`CMakeLists.txt:181-200`、`:344`），测试也链接它（`:435`） | 待修复 |
| KD-31 | 测试（建议：等待条件） | `core/tests/test_ffi_contract.cc` | RC 新鲜度测试使用 600 ms 固定 sleep | 建议偏差，待修复 |
| KD-32 | 测试（建议：门禁与 CI 对齐） | `.github/workflows/build.yml` | CI 不调用 `tools/preflight.sh` | 建议偏差，待修复 |
| KD-33 | U3（原标 U3、U4） | `configuration/calibration/widgets/calibration_aircraft.dart:58`、`configuration/ports/ports_configuration_page.dart:215`、`configuration/system/widgets/system_status_cards.dart:22`，**另有未登记的第 4 处** `configuration/motors/widgets/motor_output_cells.dart:30` | 业务代码里出现字面量 `FontWeight`（U3 的「只有 MgTypography 定义字重」），三处属实。但原描述把它归给 U4「只有标题加粗」并不成立——U4 允许页面/区块/卡片标题与表头加粗，而这三处正是卡片标题、区块标题与表头。真正的缺口是**检查脚本零覆盖**：`tools/check_ui_tokens.dart` 没有任何字重规则，自检甚至把 `fontWeight: FontWeight.w600` 断言为「不违规」（`:150-154`），与 U4 写明的验证方式不符 | 待修复 |
| KD-34 | U3 | `flutter_shell/lib/theme/mg_typography.dart:9` | 注释称 Arimo 仍在打包，实际未打包 | 待修复 |
| KD-35 | U3（需澄清规则归属） | `flutter_shell/lib/theme/mg_typography.dart:27-38` | 平台字体名（`PingFang SC`/`Microsoft YaHei UI`/`Noto Sans SC`）确实出现在代码里，但**只出现在 `MgFonts` 内部**且无业务页面引用；U3 原文要求「字体族只在 MgFonts 中定义、业务页面不得出现字体族名」，按原文不构成违反。因此这不是代码偏差，而是规则本身需要澄清：平台字体回退要么在 U2 下作为原生集成层例外明确，要么在 U3 里写清是否允许 | 待决定 |
| KD-36 | U3 | `flutter_shell/lib/demo/component_gallery_demo.dart:169,953,1039` | 演示页使用字面量 `fontSize`，已在检查脚本的允许名单内 | 待决定 |
| KD-37 | U1 | `flutter_shell/lib/map/amap_map_view.dart:63-111` | 瓦片源 URL 是代码内字面量（5 个预置 `_MapSource`），设置项只能 5 选 1 并持久化枚举名（`map_preferences.dart:28-50`），**不能自定义瓦片源 URL、不能接入自建瓦片服务** | 待决定 |
| KD-38 | R1 | `tools/release.sh` | 对 README 与 CHANGELOG 的同步已变为可选，文件缺失时静默跳过 | 待修复 |
| KD-39 | R1 | `CMakeLists.txt:242-246`；`flutter_shell/linux/CMakeLists.txt:77-80`；`tools/check_linux_bundle.dart:6-8`；`flutter_shell/lib/connection/native_core.dart:300` | 核心库的 `set_target_properties(mgcore PROPERTIES PREFIX "")` **没有 WIN32 守卫**（注释只提 MinGW），在类 Unix 上同样生效：macOS 实测产物为 `build/mgcore.dylib`。而 Linux 打包、`check_linux_bundle.dart` 与 Android 的 `DynamicLibrary.open('libmgcore.so')` 一律期望 `libmgcore.so`。本机实测 `cmake -E copy_if_different` 源文件缺失时返回 1，故 Linux 的 POST_BUILD 拷贝很可能直接让构建失败。结构不变，但**仅在 macOS 上做过静态对照，未在 Linux/Android 上实跑验证** | 待验证 |
| KD-40 | U7 | `flutter_shell/test/` | 没有 golden 测试，三端截图比对无自动化支持 | 待决定 |
| KD-41 | F1 | `bridge/ffi/API_CATALOG.md`（缺失） | F1 要求修改 `bridge/ffi/ffi_api*.h` 时同一提交必须包含 API 目录 `bridge/ffi/API_CATALOG.md`，但该文件在整个仓库中不存在（`find` 无结果），因此每次改动 `ffi_api*.h` 都无法满足 F1 | 待修复 |
