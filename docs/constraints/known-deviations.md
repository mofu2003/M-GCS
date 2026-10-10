# 已知偏差

登记当前代码与规则不符的条目。本文件只登记，不在此处修复。修复时删除对应条目，并在提交说明中引用编号。

- 规则列为约束编号（S、A、F、T、U、R、P）的，属于**约束偏差**，必须修复，或在本文件中登记计划。
- 规则列标注"建议"的，属于**建议偏差**，不阻断合并，但需要在提交说明中写明理由。

状态取值：`待修复`、`待决定`、`待验证`、`待核对`、`待补充`。

| ID | 规则 | 位置 | 现状 | 状态 |
|---|---|---|---|---|
| KD-01 | S2 | `core/src/mavlink/mavlink_senders.cc:676-690`；`core/src/flight/flight_mode_manager.cc` | 布尔命令的"已发出"被当成成功 | 待修复 |
| KD-02 | S2（建议：目标系统匹配） | `core/src/mavlink/mavlink_adapter_handlers.cc:238-265` | ACK 只按 compid 与命令号匹配，不按目标系统和 ticket；legacy `wait_for_ack` 槽位会被覆盖 | 待修复 |
| KD-03 | S1 | `core/src/mavlink/mavlink_senders.cc` 约 1150 行 | 排队发送失败只标记 TimedOut，不回调 `result_cb` | 待修复 |
| KD-04 | S1 | `core/src/flight/flight_mode_manager.cc:196-212` | arm/disarm 已处于目标状态时静默返回 false | 待修复 |
| KD-05 | S1 | `core/src/transport/tcp_link.cc:160`；`udp_link.cc:165,187` | 吞掉线程启动错误，链路显示已打开但没有读线程 | 待修复 |
| KD-06 | S1 | `core/src/transport/link_session.cc:106`；`core/src/mavlink/mavlink_router.cc:194`；`core/src/vehicle/multi_vehicle_manager.cc:248` | 忽略路由、添加目标、添加飞行器的返回值 | 待修复 |
| KD-08 | S3 调整为建议（写入类操作） | `flutter_shell/lib/mission/mission_planning_panel.dart:595-598, 848, 859` | 任务上传、围栏与集结点上传无确认 | 建议偏差，待决定 |
| KD-09 | S3 调整为建议（写入类操作） | `flutter_shell/lib/configuration/widgets/parameter_value_control.dart:55-62` | 参数写入无确认；安全相关参数建议优先加确认 | 建议偏差，待决定 |
| KD-10 | S3 调整为建议（非飞行状态命令） | `flutter_shell/lib/configuration/calibration/rc_calibration_view.dart:95` | RC 校准启动无确认 | 建议偏差，待决定 |
| KD-11 | S3 调整为建议（写入类操作） | `flutter_shell/lib/gcs_configuration_workspace.dart` 约 240、495 行 | 签名密钥下发（SETUP_SIGNING）与"保存并应用"无确认 | 建议偏差，待决定 |
| KD-13 | S3 | `flutter_shell/lib/flight/flight_mode_panel.dart:22` | `FlightModeFloatingPanel` 为死代码，其解锁对话框不可达；需删除或接入 | 待决定 |
| KD-15 | S5 | `core/src/vehicle/vehicle.cc` | 链路断开后 `Vehicle::state_` 不更新，界面可能仍显示已连接 | 待修复 |
| KD-16 | S5 | `core/src/flight/flight_mode_manager.cc:54` | 断链后 `armed_` 不清零，界面可能显示过期的解锁状态 | 待修复 |
| KD-20 | A1 | `CMakeLists.txt:251-254`；`core/tests/test_bridge_architecture_guards.cc:220` | `bridge/ffi` 位于 core 的公共 include 路径上；守卫测试只匹配 `bridge/` 前缀 | 待修复 |
| KD-21 | A2 | `core/include/core/transport/connection_manager.h:28`；`core/include/core/mavlink/mavlink_adapter.h:8`；`core/include/core/firmware/firmware_flasher.h:17` | `transport → mavlink → firmware → transport` 存在依赖环 | 待修复 |
| KD-22 | A3、A4 | `core/src/mavlink/mavlink_senders.cc:676-681`；`core/include/core/platform/platform_mutex.h:135-142` | 发送回调在 adapter 锁内执行；Windows（递归临界区）与 POSIX（`std::mutex`）语义不同 | 待修复 |
| KD-23 | A3 | `core/src/firmware/firmware_flasher.cc:57-95` | `FirmwareFlasher` 在持锁时报告进度 | 待修复 |
| KD-24 | A3 | `core/src/transport/serial_link.cc:225` | 串口读线程 join 超时后使用 `TerminateThread` | 待修复 |
| KD-25 | S4、A3 | `core/src/transport/tcp_link.cc`；`serial_link.cc:160,294` | TCP 连接与收发无超时（S4）；串口写失败不重试 | 待修复 |
| KD-26 | 架构（建议：UI 线程上的 FFI 调用） | `bridge/ffi/ffi_flight_commands.cc` 约 350-365 行 | `mg_mission_start` 同步等待可达数秒，且未标注耗时 | 建议偏差，待修复 |
| KD-27 | 架构（建议：事件获取方式） | `flutter_shell/lib/connection/connection_controller.dart` 约 1100、1956、2166-2175 行 | Dart 端通过 `Timer.periodic` 轮询事件，未使用原生回调 | 建议偏差，待决定 |
| KD-28 | T1、T2 | `core/tests/test_sim_commands.cc:222-240` | NAK 测试末尾为 OR 断言，任何非空结果都能通过 | 待修复 |
| KD-29 | T2 | `core/tests/test_ffi_contract.cc`；`core/tests/test_ffi_comprehensive.cc:138-149, 252` | FFI 合约测试包含 `CHECK(true)`；部分命令测试被注释掉 | 待修复 |
| KD-30 | T1 | 无 | 缺少验证飞控拒绝能够传播的核心层测试 | 待补充 |
| KD-31 | 测试（建议：等待条件） | `core/tests/test_ffi_contract.cc` | RC 新鲜度测试使用 600 ms 固定 sleep | 建议偏差，待修复 |
| KD-32 | 测试（建议：门禁与 CI 对齐） | `.github/workflows/build.yml` | CI 不调用 `tools/preflight.sh` | 建议偏差，待修复 |
| KD-33 | U3、U4 | `flutter_shell/lib/configuration/calibration/widgets/calibration_aircraft.dart:58`；`configuration/ports/ports_configuration_page.dart:215`；`configuration/system/widgets/system_status_cards.dart:22` | 三处硬编码 `FontWeight`，违反"只有标题加粗"；`check_ui_tokens.dart` 覆盖不足 | 待修复 |
| KD-34 | U3 | `flutter_shell/lib/theme/mg_typography.dart:9` | 注释称 Arimo 仍在打包，实际未打包 | 待修复 |
| KD-35 | U3 | `flutter_shell/lib/theme/mg_typography.dart:28-37` | CJK 回退链包含平台字体名（PingFang SC、Microsoft YaHei UI、Noto Sans SC） | 待决定 |
| KD-36 | U3 | `flutter_shell/lib/demo/component_gallery_demo.dart:169,953,1039` | 演示页使用字面量 `fontSize`，已在检查脚本的允许名单内 | 待决定 |
| KD-37 | U1 | `flutter_shell/lib/map/amap_map_view.dart:64-112` | 地图瓦片源写死在代码中，无法替换 | 待决定 |
| KD-38 | R1 | `tools/release.sh` | 对 README 与 CHANGELOG 的同步已变为可选，文件缺失时静默跳过 | 待修复 |
| KD-39 | R1 | `flutter_shell/linux/CMakeLists.txt:79-80`；`tools/check_linux_bundle.dart` | Linux 库名可能不一致：核心产出 `mgcore.so`，打包期望 `libmgcore.so` | 待验证 |
| KD-40 | U7 | `flutter_shell/test/` | 没有 golden 测试，三端截图比对无自动化支持 | 待决定 |
