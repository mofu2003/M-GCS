# 已知偏差

登记当前代码与规则不符的条目。本文件只登记，不在此处修复。修复时把条目从未解决移到已关闭，并在提交说明里引用编号。

- 规则列为约束编号（S、A、F、T、U、R、P）的，属于**约束偏差**，必须修复，或在本文件中登记计划。
- 规则列标注"建议"的，属于**建议偏差**，不阻断合并，但需要在提交说明中写明理由。

状态取值：`待修复`、`待决定`、`已决定`（建议偏差，理由已记录、不阻断合并）、`待验证`、`待核对`、`待补充`。

文件分两部分。**未解决**是当前仍需处理的偏差，也是唯一需要盯住的清单。**已关闭**一行一条，只为让代码注释与提交信息里的 `KD-xx` 编号仍可溯源；正文细节见 `CHANGELOG.md` 的 `Unreleased` 段，不再在这里堆叠过程描述。

## 未解决

| ID | 规则 | 位置 | 现状 | 状态 |
|---|---|---|---|---|
| KD-09 | S3（建议：写入类操作确认） | `flutter_shell/lib/configuration/widgets/parameter_value_control.dart:55-70` | **已决定：接受偏差**。参数写入不弹确认框，与 QGC、MP 一致——`safety.md` 的建议条款本身写明「QGC 与 MP 对这些操作都不确认，因此不做硬性要求」。参数整定是高频操作，逐次弹窗会明显拖慢流程；写入失败已有回滚路径（`parameter_write_status.dart`，`test/configuration_components_test.dart` 的 “numeric editor rolls back a rejected parameter write”）。要让「安全相关参数优先确认」落地，需要插件在参数描述符里额外下发一个安全标记（S6 要求参数名等固件专属值只能由插件提供，前端不得硬编码），属后续能力扩展。 | 已决定 |
| KD-27 | 架构（建议：事件获取方式） | `flutter_shell/lib/connection/connection_controller.dart:1755`（会话 750 ms）、`:1760`（遥测 50 ms）、`:1117`（校准 250 ms）、`:1539`（机队 750 ms）、`:3203`（波形 100 ms） | **已决定：接受偏差，保留轮询**。FFI 的 `mg_set_event_callback` 已存在且由 core/FFI 测试覆盖（`test_ffi_contract.cc`、`test_sim_ui_paths.cc`），Dart 侧仍用 `Timer.periodic` 拉取。选轮询的理由：Dart FFI 上行回调需要 `NativeCallable`/`Pointer.fromFunction` 并处理跨线程与 isolate 生命周期，而当前绑定层完全不含 FFI 回调；轮询延迟有界（50–750 ms），对地面站交互足够。回调路径保留为低延迟备选，等 `docs/plans` 阶段 1 统一视图状态时再评估迁移。 | 已决定 |
| KD-36 | U3 | `flutter_shell/lib/demo/component_gallery_demo.dart:169,953,1039` | **已决定：接受**。该文件是开发者画廊（自带 `main()`，`main.dart` 不引用、不进 release 包），用于并排对照 `macos_ui` 的原始色板与字号，不属于 U3 约束的「业务页面」。它是 `tools/check_ui_tokens.dart` 唯一的豁免文件，文件头已写明原因。 | 已决定 |
| KD-42 | S6 | `flutter_shell/lib/configuration/pid/models/pid_models.dart`（15 处）、`flutter_shell/lib/connection/connection_controller.dart`（6 处）、`flutter_shell/lib/configuration/calibration/rc_calibration_view.dart`（4 处）、`flutter_shell/lib/configuration/calibration/widgets/magnetometer_options_card.dart`（3 处）、`flutter_shell/lib/configuration/calibration/calibration_page.dart`（2 处）、`flutter_shell/lib/configuration/arming/arming_checks_page.dart`（2 处）、`flutter_shell/lib/configuration/flight_modes/flight_modes_page.dart`（1 处）、`flutter_shell/lib/configuration/calibration/widgets/magnetometer_calibration_card.dart`（1 处） | **待修复（历史遗留，非本次新增）**：S6 要求参数名只能由 ArduPilot/PX4 插件提供，前端仍有 8 个文件共 34 处 ArduPilot 参数名字面量（`PILOT_TKO_ALT_M`、`Q_RTL_ALT`、`WP_LOITER_RAD`、`RCMAP_*`、`COMPASS_*`、`ARMING_CHECK`/`ARMING_SKIPCHK`、`FLTMODE_CH`、`TECS_*`/`*2SRV_*` 等），完整清单即 `tools/firmware_literals_baseline.json`。这些字面量在 `origin/main` 上就已存在，此前未登记，属清理主线的欠账。`tools/check_firmware_literals.py` 已作为 S6 门禁挡住**新增**欠账（基线外新增即失败），清零需要插件或参数描述符向前端下发参数名（含安全标记），或改为描述符驱动的配置档位。 | 待修复 |
| KD-40 | U7 | `flutter_shell/test/` | **已决定：接受现状**。U7 的约束是「三端构建通过 + 人工截图比对」，未要求自动化 golden。Flutter golden 对字体与平台渲染敏感，三端同时稳定很难，且会带来持续的基线维护成本；保留人工比对。自动化截图比对作为平台 CI 项列在 `docs/plans` 阶段 4。 | 已决定 |

## 已关闭

| ID | 规则 | 一句话 |
|---|---|---|
| KD-01 | S2 | 无 ACK 的命令不再当成功：`mg_command_set_global_origin`/`mg_command_landing_gear` 等改为等 `COMMAND_ACK`，引导类命令在头文件明确标注「只表示已发出」。 |
| KD-02 | S2 | `COMMAND_ACK` 匹配带来源系统（sysid=0 通配）；删除没有生产调用者的单槽 `ack_waiter`/`wait_for_ack`。 |
| KD-04 | S1 | 删除 core 里只发送、不等 ACK 且无调用者的 `arm`/`disarm`/`set_mode` 等；Dart 侧处于目标状态时不再重复下发，并给出提示。 |
| KD-05 | S1 | 串口读线程创建失败时关闭端口并返回失败，不再出现「链路已打开但没有读线程」。 |
| KD-06 | S1 | 目标注册失败记录带 route/sysid/compid 的错误日志，不再静默丢弃。 |
| KD-08 | S3（建议） | 任务上传与围栏/Rally 下发改用全应用统一的确认组件，取消则不下发（`mission_planning_panel.dart:597`、`:886`）。 |
| KD-10 | S3（建议） | RC 校准启动前先确认（`rc_calibration_view.dart:97,124`）。 |
| KD-11 | S3（建议） | 「保存并应用密钥」与「下发 SETUP_SIGNING」前先确认（`gcs_configuration_workspace.dart:240,280`）。 |
| KD-15 | S5 | 车辆状态改为与传输层状态同源，断链/心跳丢失后不再残留 `Connected`。 |
| KD-16 | S5 | 状态接口统一返回 `heartbeat_fresh`/`heartbeat_age_ms`，前端危险操作经新鲜度门控。 |
| KD-20 | A1 | core 与 bridge/ffi 拆成两个 CMake 目标，core 反向引用 bridge 头变为编译期报错。 |
| KD-21 | A2 | 去掉 `firmware_flasher.h` → `connection_manager.h` 的回边；新增模块环守卫测试。 |
| KD-22 | A3、A4 | 剩余持锁发送改为锁内打包、出锁统一 flush（ADR-0005）；重入回调测试覆盖。 |
| KD-23 | A3 | `firmware_flasher.cc` 的进度回调改为锁内复制、锁外调用。 |
| KD-24 | A3 | 串口读线程只访问自持状态；join 超时不再 `TerminateThread`（避免 use-after-free）。 |
| KD-25 | S4、A3 | TCP 建立连接改为非阻塞 + 3 s 有界等待；串口写补齐短写/`EAGAIN` 与有界等待。 |
| KD-26 | 架构（建议） | 同步命令在头文件标注最长阻塞时间并指向 submit/poll 非阻塞形式。 |
| KD-31 | 测试（建议） | 等待条件改为轮询 + 超时，不再用固定 `sleep` 同步。 |
| KD-32 | 测试（建议） | CI 后端/前端改调 `tools/preflight.sh`，不再自维护一份检查表。 |
| KD-33 | U3、U4 | token 检查扩到裸字重等规则；业务代码里的字面量字重/字号清理。 |
| KD-35 | U3 | 澄清 U3：`MgFonts` 内部的平台回退链是允许的原生例外（见 `ui.md`）。 |
| KD-37 | U1 | 地图新增「自定义瓦片源」（第 6 个选项），校验模板并本地持久化。 |
| KD-41 | S8 | 校准取消改为异步确认：收到该取消命令的 `COMMAND_ACK` 才落到 `Cancelled`。 |
