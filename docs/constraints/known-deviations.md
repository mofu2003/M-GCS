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
| KD-40 | U7 | `flutter_shell/test/` | **已决定：接受现状**。U7 的约束是「三端构建通过 + 人工截图比对」，未要求自动化 golden。Flutter golden 对字体与平台渲染敏感，三端同时稳定很难，且会带来持续的基线维护成本；保留人工比对。自动化截图比对作为平台 CI 项列在 `docs/plans` 阶段 4。 | 已决定 |
| KD-43 | 架构（建议：按变更耦合拆分） | `flutter_shell/lib/connection/connection_controller.dart`（4160 行，`ConnectionController` 自 `:491` 起） | **待评估拆分**。这是当前最大的 god file：一个 `ChangeNotifier` 同时承载连接/会话状态机、参数与语义角色解析、遥测/波形/机队通知等多块职责。拆分依据是变更耦合而不是行数——先量「修参数 bug 时是否必须同时改遥测部分」，只有确认是互不一起改的几坨东西才拆；若本就是紧耦合的状态机，拆开只会打散状态、抬高维护成本。评估结论确定后再动手。 | 待决定 |
| KD-44 | 架构（建议：按变更耦合拆分） | `flutter_shell/lib/mission/mission_planning_panel.dart`（3053 行，`_MissionPlanningFloatingPanelState` 为 `:55-831`） | **待评估拆分**。文件已按 widget 拆成二十多个类，真正偏大的是面板状态类本身。是否拆分需先看任务编辑、围栏/Rally、Home、引导动作等区块的变更耦合：若各区块独立演进，按区块抽成独立 widget/文件；若它们共享大量面板状态、只能成对修改，则保持现状。 | 待决定 |

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
| KD-42 | S6 | 通用前端代码里的固件参数字面量清零：后端插件新增语义角色表（`core/src/config/ParameterDescriptors.json` 的 `roles` 段按 `ardupilot`/`copter`/`plane`/`vtol` 等 profile 下发「角色→固件参数名」），经 `mg_config_get_param_roles`、Dart `paramRoles()` 进入 `ConnectionController.roleParam()`，PID、校准、飞行模式、RC 校准、解锁检查等页面改按语义角色取参数名；`tools/check_firmware_literals.py` 的非空基线机制保留，`tools/firmware_literals_baseline.json` 现为 `{}`。 |
