# M-GCS 开发计划

> **读法提醒**：本文件是按日期追加的历史进度记录。正文中出现的各种计数——后端测试套数（37/38）、FFI 声明与 Dart 绑定数（298/302/303/305/314…）、Flutter 用例数（149/160/184/199/206/209…）——**都是写下那一行时的快照**，不代表当前值。当前值以 `docs/capabilities.md`、`bridge/ffi/API_CATALOG.md`、`docs/development.md` 与 `ctest -N` 为准；历史记录不要回改。

计划基准：2026-10-10  
比较基准：`docs/comparisons/ground-station-comparison-2026-10-10.md`。  
项目规则：`AGENTS.md`、`docs/constraints/` 与 `docs/adr/`。  
计划类型：分阶段产品与工程计划；不承诺日历日期，阶段按退出条件推进。

## 1. 产品目标

先把 ArduPilot 的连接、状态、任务、参数、校准、飞行控制和故障恢复做成可信的端到端工作流，再逐步扩大 PX4 与专业工具覆盖。每个对外展示为“已支持”的功能必须有实际 UI/调用路径、明确状态与错误语义、固件/机型能力说明、可自动验证的失败路径，以及至少一个对应 SITL 或模拟场景。

M-GCS 的差异化目标是：可替换 Flutter 前端、稳定的 C FFI、核心状态可独立测试、对飞行风险采用明确的拒绝/过期/未知语义。QGC 与 Mission Planner 是行为和能力参考，不是要求复制其实现。

## 2. 计划原则

- **安全先于覆盖率。** stale 状态、错误 ACK、被拒命令、取消失败不能转成看似成功的 UI。
- **按用户工作流交付。** API、类、按钮和占位响应本身都不算产品完成。
- **先稳定共享事务模型。** Vehicle/session、ACK、参数写、校准和任务传输先定义共同的目标、状态、超时与取消语义，再扩展更多调用者。
- **用接口测试同一条 seam。** Native/Fleet/operation 的状态契约由适配器和 fake/模拟链路测试；少写只复刻实现的测试。
- **平台能力要有证据。** 在一个平台编译成功，不代表其余平台包体、动态库名、权限、地图和设备输入都正确。
- **未实现能力诚实降级。** 占位实现必须返回 unsupported 并隐藏入口，或登记为实验功能；不使用成功空包掩盖缺失。
- **FFI 面只保留一套当前接口。** 重复或错误命名的旧导出已删除，不承诺兼容旧版应用或仓库外二进制；新 UI 只用现有稳定接口。确需新增时按 FFI 规则一次完成头文件、实现、测试、目录和文档。

## 3. 分阶段路线

### 阶段 0：建立可度量基线

**目的**：在继续开发前让团队知道每项能力究竟交付到哪一层。

**工作包**

- 为所有公开 FFI 和主要页面建能力表，分类为 `已交付`、`内部能力`、`实验性`、`未实现/unsupported`；记录 owner、固件/机型、用户入口、错误路径、测试和平台。
- 对 `known-deviations.md` 中每个条目重新核实当前源码；删除已关闭或错误描述，保留真实未解项并标注证据与后续工作。
- 为危险动作、任务事务、参数写、校准和连接状态确定共享状态词汇与错误码映射；禁止前端各自解释裸 `success`。
- 将 FFI catalog 和 `check_ffi_contract.py` 维护成稳定门禁；确认头文件声明、Flutter symbol binding、动态库导出、库版本相互一致。
- 把本次工作树中已完成的清理/测试修复审阅、验证并拆成可独立审查的提交；比较报告和开发计划单独保留。

**退出条件**

- 全部公开导出都有类别、责任模块和 ABI 状态（阶段 0 时 314 个；0.2.1 删除 34 个历史遗留后为 280 个）；未接入 UI 的公开符号被明确解释为内部或待实现，不再保留重复别名。
- 偏差台账没有已修复但仍显示待修复的条目；每个 P0/P1 都有 owner、验收测试和依赖。
- 版本、catalog、实际编译库三个一致性检查在干净构建中通过。

### 阶段 1：P0 安全与事务基础

这是首个功能实现阶段，完成前不启动视频、固件刷写或大规模 UI 扩展。

#### 1A. Vehicle 状态新鲜度与多机隔离

- 定义 per-vehicle state snapshot：连接状态、armed、flight mode、位置、Home、landed/VTOL 状态分别有有效性/更新时间；不把缺字段默认成“安全态”。
- 从 core 的 telemetry/heartbeat 状态经 FFI 输出 freshness；Fleet summary 和 active Vehicle 使用同一语义。
- Flutter UI 用单一 view state 判断新鲜度；地图、任务、顶栏、批量命令和控制器 API 都消费该结果。
- 每个异步 poll 捕获 vehicle/session generation；切换焦点、断开或重连后，旧请求结果不得覆写新状态。
- 核对 sysid、compid、link/route 和 vehicle session 的关联；明确同 sysid 多链路的归属规则。

**关键验收**：断链、心跳变旧、切换 A→B→A、旧 poll 延迟返回、同 sysid 多链路时，所有危险命令均被拒绝或路由到原目标；界面不把 stale armed/mode 展示为当前值。

#### 1B. 命令事务

- 定义 `submitted → pending/in_progress → accepted/rejected/timeout/send_failed/cancelled` 状态，错误包含命令、目标 Vehicle、飞控结果和是否有状态回读确认。
- 本地 ticket 绑定 Vehicle/session 和命令请求；MAVLink ACK 按可得的来源系统/组件/命令过滤。
- MAVLink ACK 不提供可依赖的任意请求唯一 ID；因此不得声称本地 ticket 能从线上 ACK 精确识别所有同命令并发。相同目标相同命令未终结前拒绝并发；超时后定义晚到 ACK 的隔离/清理策略。
- 统一 queued command 与 COMMAND_INT 的发送失败、ACK、timeout 回调；回调均在锁外执行，错误结果同步可由 poll/waiter 取得。
- 统一命令入口的语义：仍返回“已发出”的同步 FFI 入口要么改为 ACK 语义，要么在目录中明确标注 send-only；新 UI 只使用 acknowledged path。
- Fleet batch 操作逐机返回结果；应用 freshness、确认快照和目标 pinned 规则，不绕过安全门控。

**关键验收**：ACCEPTED、各类拒绝、IN_PROGRESS、无 ACK、发送失败、晚到 ACK、其他系统 ACK、重复命令和断链分别可测；用户绝不收到“已接受”但实际上只有本地发包成功的结果。

#### 1C. 校准事务

- 校准状态增加明确的 `cancelling`；在 native 返回前保持进度轮询和原状态。
- start/cancel/ACK/progress/report 均绑定 target sysid/compid、Vehicle/session generation、校准 attempt。
- 罗盘 start 串行化旧 attempt cancel 与新 attempt start；绑定请求参与的 compass set；所有目标实例结束后才汇总成功。
- 每个 firmware/vehicle capability 明确声明是否支持启动、取消、六面确认和多实例；不支持时明确拒绝。
- 拆分锁内状态转换与锁外 I/O/外部回调；完成回调和结果验证不得阻塞 MAVLink 锁路径。

**关键验收**：取消发送失败/NACK/ACK、无取消能力、断链中止、旧进度延迟、多罗盘混合成功失败、重复启动和析构均不会显示虚假完成；core 与 Flutter 有对应模拟测试。

#### 1D. 参数写入事务

- 将参数写入改成 `pending → confirmed/rejected/timeout/cancelled`，目标绑定 session/sysid/compid、参数名、MAV_PARAM_TYPE、请求 generation 和预期值。
- 只有匹配的 `PARAM_VALUE` 且类型/值符合预期，才向 UI 返回确认；`PARAM_ERROR` 明确失败。
- force refresh/cancel/reconnect 时隔离旧下载帧和旧写回执；同目标同参数并发写需要排队或拒绝。
- 元数据缺失与值未收到要分别显示，不把默认值当飞控值。

**关键验收**：错误目标、错类型、不同值、旧 generation 回包、重复同名写、部分参数下载、刷新取消和 reconnect 均有测试。

#### 1E. 共享 core 架构 P0

- 处理锁内回调和持锁 I/O；先从 `known-deviations.md` KD-22/KD-23/KD-24/KD-25 逐项复测，避免直接做大重构。
- 建立 `transport`、`mavlink`、`firmware` 的依赖方向目标图，按真实 include/调用拆解 KD-20/KD-21。
- TCP connect 限时；串口写 partial/EAGAIN/error 有有界处理；线程创建失败必须传播到连接结果。
- Vehicle 添加/route 注册等不可忽略的失败统一成为 operation 错误和诊断日志。

**退出条件**

- KD-16/22/23/24/25 等影响安全或线程正确性的条目被修复或有明确阻断理由和测试。
- 核心状态、命令、参数和校准在 disconnect/session switch 时可证明不会串到别的 Vehicle。
- 全部离线/仿真测试及新增跨模块回归通过；ASan/TSan 至少在可支持的主机配置中跑过相应目标。

### 阶段 2：ArduPilot 主用户路径

在阶段 1 的事务模型稳定后，集中做好一个完整且真实可用的主路径。

**目标工作流**：Copter 连接 → 固件识别 → 参数加载 → 校准/飞前检查 → MissionDocument 规划和文件交换 → 上传与回读核验 → 用户确认后启动/引导控制 → 断链恢复 → 日志导出。

**工作包**

- 把 MissionDocument 的文件版本、单位、高度 frame、complex item 和未知字段 round-trip 写成格式契约；导入/编辑/导出不丢用户数据。
- 为常见 ArduPilot Copter/Plane/QuadPlane/Rover 任务项建立 capability table；不支持的 item 在上传前拒绝并解释，而非降级为另一条命令。
- 任务、围栏、rally 共用 operation 状态、进度、超时、取消、ACK source、回滚和 readback verification 语义。
- 完成全参数列表、搜索、元数据、类型控件、批量读写、参数文件导入/导出；参数写入使用阶段 1 事务。
- 让 pre-arm/health 条目从飞控消息到可操作 UI 有来源、严重度、清除条件和 freshness。
- 在地图上完成当前源管理、基本缓存策略、归因和无网/服务失败显示；不立即建设完整 GIS 系统。
- 飞行日志支持本地事件/tlog 保存、目标路径选择、磁盘失败提示和 session 绑定；若 DataFlash 尚未完成，明确标记 unsupported。

**退出条件**

- 选定的一组 SITL 场景从新连接开始跑完整任务，记录命令结果、任务 plan revision、实际回读、断链和恢复。
- 任务文件 round-trip、参数文件 round-trip、飞控拒绝、超时、取消和断链错误均自动化。
- 至少一次受控真实飞控现场验收；必须遵循硬件规则，且有操作者在场。

### 阶段 3：能力扩展，逐项立项

每个方向必须先完成产品需求、上游行为研究、license/外部依赖检查和验收标准；不做“把所有占位 API 都接按钮”。

1. **日志/DataFlash 与 replay**：优先于分析器 UI，先做可靠下载、取消、文件完整性、离线打开和基本消息检索。
2. **测绘与 terrain**：调查 camera model、survey/spline/corridor、terrain following、影像导出和常用格式，选一条 ArduPilot 关键流程完成后再拓展。
3. **PX4 parity**：按连接、mode、arm/takeoff、参数、任务、校准、health 一项项做真机/SITL capability 与错误测试，不仅做 firmware type 分支。
4. **视频/camera/gimbal**：设计平台 codec capability、流发现、解码器生命周期、recording 和设备 ACK；先证明 macOS/Linux/Windows 里至少两端和 Android 至少一个方案可运行。
5. **Joystick/manual control**：先定义安全模型（输入丢失、死手、车辆焦点、模式切换和 deadman），再实现 OS backend 和用户配置。
6. **Firmware flashing**：需独立 ADR/恢复设计/板型验证/签名来源/断电保护；在拥有可测恢复环境前保持 UI 隐藏。
7. **Map/offline provider**：支持自定义 URL/WMS/WMTS、缓存配额、认证 secret 安全和 attribution；不是把无限 URL 字符串直接拼进请求。

**退出条件**：每个方向有自己的一页 feature brief、parity 证据、平台能力矩阵、自动测试和已知限制；没有完成的仍标 `experimental/unsupported`。

### 阶段 4：平台、发布和长期工程治理

- CI 对齐统一 contracts：C++ lint/format、Flutter analyze/test、C++ offline/simulation、FFI catalog/native symbols、各平台 native package smoke。
- 每次 release 校验 CMake/pubspec/FFI/CHANGELOG/README 版本，生成 SBOM/third-party notices，检查字体、地图和依赖再分发条款。
- 对 QGC、MP、MAVLink、ArduPilot/PX4 的版本引用设置更新流程；协议行为以明确的 MAVLink/firmware 版本 fixture 验证。
- 按平台给出实际支持等级：`build-tested`、`package-tested`、`runtime-smoke-tested`、`hardware-tested`；没有运行的等级不打勾。
- 建立性能基线：启动/连接时间、参数加载速率、遥测负载、地图内存、长时间连接泄漏和多机并发上限。
- 建立故障报告包：版本、平台、firmware、Vehicle/endpoint 状态、去敏后的 operation/error logs；绝不默认导出 signing key 或敏感位置历史。

## 4. 首批任务拆分与依赖

当前进度：

- **阶段 0 退出条件已满足**。`docs/capabilities.md` 给出全部公开导出（阶段 0 为 314 个；0.2.1 清理历史遗留后为 280 个）的模块级交付状态（已交付/部分接入/待接入/内部能力）与 UI 覆盖计数，表格由 `tools/check_ffi_contract.py` 生成并纳入 preflight，计数漂移即失败；`API_CATALOG.md`、Dart 绑定、编译库导出与库版本四项一致。清空后的 `known-deviations.md` 只剩 6 条「已决定」的建议偏差（KD-09、KD-27、KD-36、KD-40，以及后续登记的 KD-43、KD-44），没有已修复却仍列待修复的条目。
- **阶段 1A 完成**。`heartbeat_fresh`/`heartbeat_age_ms` 已从 core 链路状态进入 Fleet JSON、session telemetry、`mg_flight_get_modes`、诊断与 Dart 模型；危险操作（批量指令、引导、顶栏、地图）在 `heartbeat_fresh` 为 false 时按「未知」拒绝或显示占位，不再用最后一帧放行。`ConnectionController` 的会话/校准/机队/波形轮询都捕获 `sessionId`/`_activeLinkId`/`_sessionGeneration`，焦点切换或断链后旧结果不覆写新状态。
- **阶段 1B 基本完成**。命令结果统一到 queued command 的 operation 状态，`COMMAND_ACK` 按来源 `sysid`/`compid` 与命令匹配；排队发送失败与 `COMMAND_INT` 失败都在锁外即时投递结果回调，不再只靠轮询（KD-03）；`set_global_origin`/`landing gear` 改为等 `COMMAND_ACK` 再返回（KD-01）；同一目标的不同命令可并发，只有未终结的同一条命令被拒绝。`test_sim_commands.cc` 与 `test_state_machines.cc` 覆盖发送失败回调、拒绝结果和 ACK 来源过滤。
- **阶段 1C 基本完成**。校准增加 `cancelling` 阶段：取消只有在收到该 `COMMAND_ACK` 后才落 `Cancelled`（KD-41），静默或断链由看门狗收尾并给出原因；PX4 不支持取消/六面确认时明确拒绝（KD-17）。`test_sim_calibration.cc` 覆盖取消发送失败、静默看门狗与断链中止。
- **阶段 1D 完成**。参数写入 `pending → confirmed/failed` 事务：写入绑定目标 `sysid`/`compid`、`MAV_PARAM_TYPE`、预期值与请求 generation，只有四项都匹配的 `PARAM_VALUE` 才确认，`PARAM_ERROR` 立即失败，同名新写取代未终结的旧写，新下载/重连把旧写判失败，旧 generation 帧不能确认新请求；`mg_params_get_write_status` 把事务历史暴露给前端，`ConnectionController` 用写入 id 作 generation 基线。`test_sim_params.cc` 覆盖错目标、错类型、错值、旧 generation、重复同名写与重连隔离。
- **阶段 1E 完成，阶段 1 退出条件满足**。持锁派发路径的发送改为锁内打包、出锁统一 flush（ADR-0005，KD-22/KD-23/KD-24/KD-25 已关闭）；锁外回调与 bounded I/O 由 `test_bridge_architecture_guards.cc` 的重入回调用例钉住。`tools/sanitize.sh` 在 ASan 与 TSan 下各跑 37 套 `offline|simulation`，全部通过——这是本阶段退出条件要求的「可支持主机配置下跑过相应目标」。
- 收尾期间修掉一处真实的构建回归：`CMakeLists.txt` 把 `mgcore` 共享目标输出固定到仓库根 `build/`，而 `sanitize.sh` 曾以 `MGCORE_BUILD_SHARED=ON` 配置 sanitizer 树，于是最后一次 sanitizer 链接会用带 TSan 运行时的库覆盖 Release 产物，Flutter 内嵌与 preflight 的 FFI 符号核对随之加载到错误库。现在 sanitize 树固定 `MGCORE_BUILD_SHARED=OFF`，preflight 在符号核对前检测并重链接被污染的产物（见 `docs/development.md`）。
- **阶段 2 工作包 1（MissionDocument 文件契约）完成**。`docs/mission-format-contract.md` 写明原生 文档的单位、高度 frame、`schema_version` 策略，以及 `.waypoints` 与 QGC `.plan` 的导出/保留规则。QGC 导入不再丢 M-GCS 未建模的数据：任务项未知字段（`altitudeMode`/`altitude`/`AMSLAltAboveTerrain` 等）存进新的 `MissionEntity::extensions_json`，`mission` 级与顶层未知段存进 `document.extensions` 的 `qgc_plan.mission_fields`/`qgc_plan.root_fields`，导出时逐字回填且 `hoverSpeed` 不重复。前端 `MissionDocument`/`MissionEntity` 模型补齐 `complex_items`、`extensions` 透传，修复了 `replace`/`update` 会删掉 QGC 导入的 Survey/Corridor 复杂项与保留字段的问题。新增 `test_qgc_plan_preserves_unknown_fields` 与 Dart 模型 round-trip 用例；37/37 离线/仿真测试与 Flutter analyze/test 通过。
- **阶段 2 工作包 2（任务命令机型能力表）完成**。`docs/mission-capability-table.md` 记录 13 条已进 UI 的命令在 Copter/Plane/VTOL/Rover 上的执行情况，每条 `MissionCommandDescriptor` 新增 `vehicles` bit mask，依据 ArduPilot `mode_auto.cpp`/`commands_logic.cpp`/`AP_Mission_Commands.cpp` 的 `start_command` 分支逐条标注。门禁在三处生效：导入/编译按文档声明的机型产出 `entity.command_vehicle_unsupported`，上传前用已连接飞机的参数档案再跑一次 `validate_plan_for_vehicle()`（`mission.command_vehicle_unsupported`），前端下拉按机型过滤并对正在编辑的实体命令强制保留。PX4 或未声明 firmware 的文档不判机型，Sub/Tracker 等未建模机型走「没有数据就不拦」分支，不用猜测拒绝任务。与 QGC `MissionCommandTree` + `MavCmdInfo*.json` 的固件×机型两级树做了对照，差异和已知限制（`.plan` 导出暂不支持 `rover`）写入文档。`test_mission_vehicle_capability` 与 Dart 过滤用例覆盖；37/37 离线/仿真测试、`flutter analyze`、`flutter test` 全量通过。
- **阶段 2 工作包 3（mission / fence / rally 共用 operation 语义）已就位并复核**。三者共用 `MissionTransferState`（`core/src/mavlink/mission_transfer.h`）：状态/进度由 `transfer_operation_json()` 统一序列化，超时与重试是 `MISSION_ACK_TIMEOUT_MS`/`MISSION_MAX_RETRIES`，取消走统一的 `mg_operations_cancel`（`bridge/ffi/ffi_operations.cc` 覆盖 mission/fence/rally/params/calibration/log），ACK 来源用 `transfer_matches_target()` 按 target sysid/compid 过滤（`mavlink_adapter_mission.cc` `handle_mission_ack`），上传用 `begin_upload_backup()`/`rollback_upload()` 在飞控拒绝、发送失败或超时时回滚，readback 用 `mg_mission_doc_verify*`。前端 `mission_document_ffi_backend.dart` 已消费 `mg_operations_get_snapshot`/`mg_operations_cancel`；`test_sim_fence_rally.cc`、`test_sim_mission.cc`、`test_sim_full_flow.cc` 覆盖并行传输互斥、拒绝、回滚与全流程。
- **阶段 2 工作包 4（SITL 全流程证据）完成**。`tools/sitl-mission-scenario.sh` 能拉起本地 ArduPilot SITL（`--serial0 udpclient:127.0.0.1:<port>`，不是 `sim_vehicle.py` 的 `--out`）并运行 `tests/manual/test_sitl_mission_scenario.cc`：连接 → 固件/机型识别 → 建文档 → 校验 → 上传（等 `MISSION_ACCEPTED`）→ 回读核验 → 断链 → 重连 → 从飞控重建文档 → 再次核验，20 项断言全部通过。已在 `arducopter --model quad` 与 `arduplane --model plane` 两个构建各跑一轮，产物是 `docs/sitl-scenario-evidence.md` 与 `docs/sitl-scenario-evidence-plane.md`（只保留断言与 `[EVIDENCE]` 行，可直接复算）。
- 由 SITL 首次跑通暴露并修复一个真实缺陷：ArduPilot 固定翼构建把「航点接受半径/通过半径」打包进 `MISSION_ITEM_INT.p1`，不存 `param2`/`param3`；Copter/Rover 存 `param1`，不存 `param2`/`param3`；`param4`（yaw）任何 ArduPilot 构建都不存。`mission_command_parameter_applies()` 给每条命令的每个参数标注机型适用性（`kArduPilotNone` 表示「ArduPilot 都不存」），`mg_mission_doc_verify` 对不适用的字段改记 `normalization` 而不是 `difference`。证据：copter 首轮 `normalization_count=3`（三条 param2 归一），plane 首轮 `normalization_count=0` 且回读 `param2=2`。`test_mission_vehicle_capability` 与 `test_sim_mission_document` 覆盖。
- 阶段 2 退出条件只剩一项：一次受控真实飞控现场验收（需操作者和硬件在场，按 `docs/constraints/release-and-hardware.md` 执行）。
- 由 SITL 重连步骤带出一个**待决定**的设计差异：编辑文档按连接（`vehicle_id`）持有，`mg_connection_disconnect`/`mg_connection_close` 会释放该条目的注册项，重连后拿到的是空文档，正式恢复流程是从飞控回读并导入（`mg_mission_doc_import_from_vehicle`）。QGC 的 `PlanManager` 在断链时保留内存计划。是否把「断链后保留上一连接的草稿」单独立项，需要先定义离线草稿与连线文档的关系和未保存改动提示，留待用户决定。
- **阶段 3 方向 1（日志/DataFlash 与回放）功能简报完成**。`docs/features/flight-logging-and-replay.md` 记录 19 个 `ffi_api_logging.h` 导出的四组职责、QGC/MP 上游做法（带 file:line）、差距表、三个待定决策和 S1/S2/S3 切片与验收标准。简报过程中核对出 7 个真实问题：`mg_fs_*` 在 FFI 线程同步阻塞传输（P1）；日志下载在落盘前就报告完成、写失败被吞（P2）；`mg_log_*`/`mg_log_record_*`/`mg_log_file_*` 三组命名重叠（P3）；`log_download_status` 带补包副作用（P4）；计数以 `int` 截断（P5）；无擦除/索引/校验（P6）；`mg_onboard_log_get_entries` 缺 `CHECK_CONNECTED`（P7）。`docs/constraints/parity.md` 的日志条目已按当前 QGC/MP 源码更正（此前「QGC 未发现机载 dataflash 下载」已过时）。S1 实现前先确认 P1/P2/P3 的处理方式。
- **阶段 3 方向 1 S1 native 修复完成（2026-10-11）**。P2（完成态改为落盘成功后置位，写失败经 `download_error`/`saving`/`saved_bytes` 暴露）、P3（命名统一为 `mg_onboard_log_*`/`mg_tlog_*`/`mg_session_log_*`/`mg_event_*`，旧名删除、不留别名）、P4（`log_download_status` 去副作用，补包移入 `process_timeouts()`）、P5（计数改 64 位）、P7（补 `CHECK_CONNECTED`）已改并配套 4 个测试。仍开放：P1（`mg_fs_*` FTP 异步化）与 P6（擦除/索引/校验）。
- **阶段 3 方向 1 S1 Flutter 接入完成（2026-10-11）**。新增 `flutter_shell/lib/logging/log_backend.dart`、`flutter_shell/lib/logging/log_ffi_backend.dart`、`flutter_shell/lib/logging/log_models.dart`、`flutter_shell/lib/logging/log_controller.dart`、`flutter_shell/lib/logging/onboard_log_page.dart`，入口为设置菜单「机载日志」→ 配置工作区 `logs` 页：刷新列表（`expected`/`complete` 收齐、按编号倒序）、逐条下载与进度、取消、失败/断链提示、完成显示落盘路径；下载结果绑定发起会话，切换飞机丢弃旧状态。native `mg_onboard_log_get_entries` 新增 `expected`/`complete` 并按 id 去重。Dart 绑定新增 5 个 `mg_onboard_log_*` 符号（87→92），`docs/capabilities.md` 的 `ffi_api_logging.h` 由「待接入」改为「部分接入」。测试：`core/tests/test_sim_ui_paths.cc` 日志用例、新 `flutter_shell/test/log_controller_test.dart`（8 个用例）。S1 收尾仅剩 P1/P6。
- **阶段 3 方向 1 S1 收尾（P6 擦除与下载完整性）完成（2026-10-11）**。新增 `mg_onboard_log_erase`（`bridge/ffi/ffi_api_logging.h`、`ffi_log_commands.cc`）：`CHECK_CONNECTED`，下载进行中拒绝并返回 `MG_ERROR_NOT_SUPPORTED`，否则发 `LOG_ERASE`；Flutter「机载日志」页新增「擦除全部」确认（完成清空列表并提示）。下载完整性：`LogDownloadStatus` 增 `size_declared`，`LOG_ENTRY` 声明大小后提前收到空 `LOG_DATA` 不再挂死/谎报完成，改为 `state="failed"` + `log truncated: received X of Y bytes` 且不落盘；未声明大小则正常收尾并标注「无法校验完整性」。FFI 目录/Dart 绑定/编译导出一致（298 声明 / 93 绑定，+1 `mg_onboard_log_erase`），`tools/preflight.sh` 全绿，Flutter 149 通过 / 4 跳过，离线+仿真 37/37 通过。**S1 仅剩 P1（`mg_fs_*` FTP 异步化）与本地索引/缓存（S3）。**
- **阶段 3 方向 1 S2（tlog 录制接线 + 离线回放）完成（2026-10-11）**。统一 `.tlog` 磁盘格式契约（`core/include/core/log/tlog_format.h`：无文件头，每条 `[8 字节大端 Unix 微秒][完整 MAVLink 帧]`），写入端与读取端共用。修一个真实缺陷：`MavlinkLogWriter` 此前除单元测试外**没有调用点**，录制从未生效——`MavlinkAdapter` 新增 `set_log_sink()`/`record_frame()` 并挂到帧投递路径，`Vehicle::wire_telemetry_callbacks_for` 幂等挂载（ADR-0006）。导出 `mg_tlog_replay_open/step/seek/close`（`ffi_api_logging.h`、`ffi_log_commands.cc`），`open` 全扫描并回报 `truncated`，`step` 默认 200/上限 2000 条回报 `messages/progress/eof`，`seek` 越界 `NOT_FOUND`；消息名经 `mavlink_message_names.{h,cc}` 解析。Flutter 新增「飞行记录」页（`flutter_shell/lib/logging/tlog_models.dart`、`flutter_shell/lib/logging/tlog_backend.dart`、`flutter_shell/lib/logging/tlog_ffi_backend.dart`、`flutter_shell/lib/logging/tlog_controller.dart`、`flutter_shell/lib/logging/flight_log_page.dart`），入口在设置菜单「飞行记录」→ 配置工作区 `flightLog` 页：录制开关（未连接拒发、1 s 轮询字节）、本地 `.tlog` 列表、手动路径、分步回放、进度条与按比例跳转（防抖）、消息过滤、最近 5000 条上限、截断提示。FFI 目录/Dart 绑定/编译导出一致（302 声明 / 100 绑定，+7 `mg_tlog_*`），`docs/capabilities.md` `ffi_api_logging.h` 说明更新。过程中修一个真实缺陷：`mg_tlog_replay_seek` 旧实现从目标字节裸扫描找帧，会把 8 字节时间戳前缀喂进解析器、落点随录制时间戳漂移（30 次跑错 2 次）；改为按记录边界前进、落点记录挂起，seek 后首次 step 返回落点本身，percent==1.0 返回 `NOT_FOUND`。测试：`core/tests/test_new_modules.cc::test_tlog_recording_wiring`、`test_ffi_contract.cc` 回放正常/截断两组、`flutter_shell/test/tlog_controller_test.dart`（10 用例），Flutter 160 通过 / 4 跳过，离线+仿真 37/37 通过。**阶段 3 方向 1 仅剩 P1（`mg_fs_*` FTP 异步化）与本地索引/缓存（S3）。**
- **阶段 3 方向 1 P1（`mg_fs_*` FTP 异步化）完成（2026-10-11）**。`MavlinkFtpClient` 增加异步 façade（`start_list_directory`/`start_download_file`/`start_upload_file`/`transfer_status`/`transfer_active`/`wait_for_completion`/`cancel`/`shutdown`）：阻塞传输移到后台线程，状态由独立 `status_mtx_` 保护、进度用原子量更新，状态查询不再可能被 `wait_response` 的超时拖住；取消同时置 `cancelled_` 与 `cancel_requested_`，覆盖「刚 start 未入循环」的窗口，取消以 `cancelled` 汇报、与超时/被拒区分。`MavlinkAdapter` 改为 `file_*_start`/`file_transfer_status`/`file_transfer_cancel`/`file_transfer_wait`，`Impl::~Impl()` 先 `shutdown()` 再析构，避免后台线程碰到已析构的发送成员。FFI 侧 `mg_fs_list_directory`/`mg_fs_download_file`/`mg_fs_upload_file` 立即返回（冲突返回 `MG_ERROR_BUSY`）、`mg_fs_transfer_status` 轮询（含 list 的 `items`）、`mg_fs_cancel` 取消，删除旧的 `mg_fs_download_cancel`；`transfer_status` 不做 `CHECK_CONNECTED`，断链后仍能读到最终结果。测试：`test_new_modules.cc` 新增异步下载/列表/上传 + 单会话拒绝 + 取消中断 + 取消后复用（124 通过），`test_sim_ui_paths.cc` 新增 `mg_fs_*` 端到端（completed/落盘、`FILE_NOT_FOUND`→failed/error_code 10、无响应取消→cancelled），MockFlightController 补 `ListDirectory`。FFI 目录/Dart 绑定/编译导出一致（303 声明 / 100 绑定）。**阶段 3 方向 1 仅剩 S3（本地索引/缓存 + FTP 文件浏览页）。**
- **阶段 3 方向 1 S3（FTP 文件浏览页 + 本地索引/缓存）完成（2026-10-11）**，本方向收口。新增 Flutter「机载文件」页（`flutter_shell/lib/logging/file_backend.dart`、`flutter_shell/lib/logging/file_ffi_backend.dart`、`flutter_shell/lib/logging/file_models.dart`、`flutter_shell/lib/logging/file_index.dart`、`flutter_shell/lib/logging/file_controller.dart`、`flutter_shell/lib/logging/file_browser_page.dart`，入口在设置菜单「机载文件」→ 配置工作区 `files` 页）：路径行与常用目录（`@SYS`/`/APM/LOGS`/`/`）、刷新/上一级、目录进入、单文件下载（进度 + 取消）、本地上传到当前目录；结果只按 `mg_fs_transfer_status` 呈现。`FileController` 复用 native 的单会话模型（并发发起拒绝、会话切换丢弃进行中的传输、迟到状态不写回），列表轮询带 idle 去抖与 8 s 超时。`FileIndexStore` 把目录缓存与「远程→本地副本」映射写到应用支持目录 `files/file_index.json`，断链/列表失败时回填缓存并标注可能过期，副本大小与机载声明不一致时显式提示，索引写失败只提示不阻断传输。顺带把机载日志页与飞行记录页各自的私有 `_Notice`/`_DismissButton` 提取为共享组件 `flutter_shell/lib/widgets/mg_inline_notice.dart`（`ui.md` U5）。测试：`file_controller_test.dart`（20 用例）、`file_browser_page_test.dart`（3 用例）以及路由/设置菜单/FFI 边界三处更新；Flutter 全量 184 通过 / 4 跳过，`check_ffi_contract.py` 303 声明 / 105 绑定一致。未做（另立议题）：DataFlash `.bin` 解析/图表、下载后压缩、断点续传。**阶段 3 方向 1 全部完成。**
- **阶段 3 方向 2（测绘与 terrain）功能简报完成（2026-10-11）**。`docs/features/survey-and-terrain.md` 记录 M-GCS 现状（原生有测绘骨架 `survey_calculator.{h,cc}` 与地形骨架 `terrain_query.{h,cc}`，FFI 已导出 `mg_survey_generate*`/`mg_terrain_query*`，但前端 0 绑定、地形 provider `fetch_elevation()` 是桩恒返回 `-1001.0f`、无 `TERRAIN_REQUEST` 回发）、QGC/MP 上游做法（QGC `SurveyComplexItem.cc`/`TransectStyleComplexItem.cc`/`TerrainTileManager.cc`/`TerrainProtocolHandler.cc`，MP `GridUIv2.cs`/`TerrainFollow.cs`，均带 file:line）、7 项差距与决策点（D1 provider 选择、D2 真批量、D3 磁盘缓存、D4 地形协议回发、D5 复杂项、D6 导出、D7 前端入口）、S1/S2/S3 交付切片与验收标准，以及许可边界（QGC Apache-2.0/GPL 双许可、MP GPLv3 只参考行为、高程第三方服务条款）。`docs/constraints/parity.md` 新增「测绘与地形」对照条目。实现前先确认 D1/D5。
- **阶段 3 方向 2 S1（地形 provider + 跨调用共享缓存）完成（2026-10-11）**。用 `core/.../terrain/terrain_store.{h,cc}` 取代旧的 `terrain_query.{h,cc}`（`TerrainCache` 内存 0.01° 网格 + 批量淘汰、`TerrainStore` 提供 `store`/`store_batch`/`lookup`/`sample_line`/`clear`/`hit_rate`）；缓存挂到 `mg_core_t`，由 `mg_terrain_query`/`mg_terrain_query_line` 读写，新增 `mg_terrain_cache_update`/`mg_terrain_cache_clear`（`ffi_api_environment.h:10-17`）。查询类返回 `valid`/`elevation:null`（缺值为数据契约，不是 `success:false`），剖面返回 `complete`/`missing_count`/`missing`/`points`。设计取舍：core 不内置 HTTPS 客户端（多平台 TLS 差异会新增平台依赖，违反 architecture A6），高程由前端 `flutter_shell/lib/terrain/elevation_provider.dart`（`OpenElevationProvider`，端点可配置、100 点分批、20 s 超时）拉取后经 `cache_update` 回写，`TerrainController` 负责「缓存优先 → provider 回填 → 回写 → 重新采样」。验证：`core/tests/test_terrain.cc` 38 通过（缓存命中/未命中/同网格、批量跳过无效、剖面缺值/回填、采样下限、FFI 数据契约、清缓存）、`test_bridge_architecture_guards` 56、`test_ffi_contract` 135、`test_sim_survey` 36 全绿；`flutter_shell/test/terrain_controller_test.dart` 6 用例 + FFI 边界用例 8 通过；`flutter analyze` 无问题；`tools/check_ffi_contract.py` 305 声明 / 109 绑定一致。`docs/features/survey-and-terrain.md` 与 `docs/constraints/parity.md` 已按 S1 实际结果更新。顺带修掉一个真实的 `.gitignore` 隐患：SITL 运行产物块用未锚定的 `terrain/`，会连带忽略 `core/{include/core,src}/terrain/` 与 `flutter_shell/lib/terrain/` 源码（旧 `terrain_query.{h,cc}` 是靠强制添加才进库），改为 `/terrain/` + `**/terrain/*.DAT`，只忽略根目录与任意层级下的 `.DAT` 瓦片，源码目录恢复正常跟踪。未做（S2/S3 或另立议题）：地形进页面（D7）、磁盘瓦片持久化（D3）、`TERRAIN_REQUEST`/`TERRAIN_DATA` 回发（D4）、测绘复杂项与 KML/CSV 导出（D5/D6）。此时阶段 3 方向 2 仅剩 S2（测绘进 UI）与 S3（terrain following 与导出）。
- **阶段 3 方向 2 S2（测绘生成进 UI）完成（2026-10-11）**。新增 `flutter_shell/lib/survey/survey_models.dart`、`flutter_shell/lib/survey/survey_backend.dart`、`flutter_shell/lib/survey/survey_ffi_backend.dart`、`flutter_shell/lib/survey/survey_controller.dart`、`flutter_shell/lib/survey/survey_planning_page.dart`：`SurveyFfiBackend` 绑定 `mg_survey_generate`/`mg_survey_generate_corridor`（返回值经 `mg_free_string` 释放），`SurveyController` 提供 `generateGrid`/`generateCorridor`/`entitiesFor`/`applyToMission`，页面「测绘规划」（配置工作区 `survey`，设置菜单入口）以三模式（矩形网格 / 交叉网格 / 走廊扫描）配置区域与航线、相机与重叠参数，生成后预览航点/航线/航程/预计用时/覆盖面积/拍照数，并把结果并入任务文档。入口接线：`widgets/mg_settings_menu.dart` 新增 `MgSettingsDestination.survey`，`configuration/configuration_workspace.dart` 新增 `survey` 页，`connection_controller.dart` 暴露 `surveyController`（native 为 null 时用 `SurveyUnavailableBackend`）。设计取舍：生成「一次性航点」并入任务文档，不做 QGC 式可编辑复杂项（D5）；页面不持有 `dart:ffi`，只经 `ConnectionController`（架构 A5）；`applyToMission` 在 `replaceDocument` 后显式复查 `mission.error` 与实体数增量，避免编辑失败被 `MissionDocumentController` 收进 `error` 后仍被当作成功。验证：`flutter analyze` 无问题；`flutter test` 199 通过 / 4 跳过（新增 `survey_controller_test.dart` 7 用例、`survey_planning_page_test.dart` 1 用例，`mission_document_controller_test.dart` 的 `_FakeMissionBackend` 提取为 `test/support/mission_backend_fake.dart`）；`tools/check_ui_tokens.dart` 通过；`tools/check_ffi_contract.py` 305 声明 / 111 绑定一致（新增 2 个 survey 绑定）。`docs/features/survey-and-terrain.md`、`docs/constraints/parity.md`、`docs/constraints/ui-pages.md`、`docs/capabilities.md` 已按 S2 实际结果更新。未做（S3 或另立议题）：地图上叠加显示航线、自动上载、`.plan`/KML/CSV 导出（D6）、地形进页面（D7）、磁盘瓦片持久化（D3）、`TERRAIN_REQUEST`/`TERRAIN_DATA` 回发（D4）。**阶段 3 方向 2 仅剩 S3。**
- **阶段 3 方向 2 S3a（任务文件导出进 UI）完成（2026-10-11）**。任务规划面板在「清除飞控任务」之后新增「导出文件」按钮（`mission/mission_planning_panel.dart`），弹层 `mission/mission_export.dart` 可选 `waypoints`（`.waypoints`，编译后任务项）或 QGC `.plan`（`.plan`，保留复杂项），复用已有 native `mg_mission_doc_export_format`（`ffi_api_mission.h:92`），把文本写入用户可编辑路径（默认 `getApplicationSupportDirectory()/missions/`），完成后在面板内以 `MgInlineNotice` 显示「已导出 N 条到 <path>（M 条提示）」。链路：`MissionInterchangeExport`（`mission_document_models.dart`）→ `MissionDocumentBackend.exportMissionInterchange`（`mission_document_backend.dart`）→ `MissionDocumentFfiBackend.exportMissionInterchange`（`mission_document_ffi_backend.dart`，+1 绑定）→ `MissionDocumentController.exportInterchange`（`mission_document_controller.dart`）。设计取舍：**导出不改任务文档**（不触碰 dirty/revision）；**不引入文件选择器插件**（pubspec 无），与 `logging/log_controller.dart` 一致，用文本路径 + 默认目录。验证：`flutter analyze` 无问题；`flutter test` 206 通过 / 4 跳过（新增 `test/mission_export_test.dart` 5 用例、`mission_document_controller_test.dart` 导出转发用例）；`tools/check_ui_tokens.dart` 通过；`tools/check_ffi_contract.py` 305 声明 / 112 绑定一致（新增 1 个 `mg_mission_doc_export_format` 绑定）。`docs/features/survey-and-terrain.md`、`docs/constraints/ui-pages.md` 已按 S3a 实际结果更新。未做：KML/CSV 导出、`mg_mission_doc_import_format` 前端绑定（导入侧）、地图航线叠加、自动上载、地形进页面（D7）、磁盘瓦片持久化（D3）、`TERRAIN_REQUEST`/`TERRAIN_DATA` 回发（D4）。**阶段 3 方向 2 仅剩 S3b（terrain following / AGL 高度写入任务）。**
- **阶段 3 方向 2 S3b（terrain following / AGL 高度写入任务）完成（2026-10-11）**。任务编辑与测绘页新增「高度基准」选择（相对高度 Home / 地形跟随 AGL，写入 `MAV_FRAME_GLOBAL_RELATIVE_ALT`(3) / `MAV_FRAME_GLOBAL_TERRAIN_ALT`(10)）。落在三处：`mission_document_models.dart` 增加 `kMissionFrameRelativeAlt`/`kMissionFrameTerrainAlt`/`missionFrameLabel`/`kMissionFrameChoices`；`mission_planning_panel.dart` 的实体编辑弹层新增「高度基准」并在保存时写 `frame`、任务设置页新增「默认高度基准」，选 AGL 时航高标签变「高度 (m, AGL)」并显示地形数据提示；`survey_models.dart`/`survey_controller.dart` 的 `toMissionEntity`/`entitiesFor`/`applyToMission` 增加可选 `frame`，测绘页新增「高度基准」行，选 AGL 后「并入任务」把整条航线统一写成帧 10。设计取舍：**只写帧、不做地形高度换算**——`param7` 是用户填的离地数值，由飞控按地形数据解释执行（与 QGC `SimpleMissionItem.cc:74,737` 一致）；**M-GCS 不替飞控回发地形网格**（D4 仍不做），框内/页内文案明确提示需飞控载有该区域地形数据。帧 10 在 core 侧早已合法（`mission_validation.cc` 允许 0/2/3/10，`mission_compile.cc`/`mission_waypoints.cc`/`mission_qgc_plan.cc` 原样透传），S3b 只补前端选择与应用。验证：`flutter analyze` 无问题；`flutter test` 209 通过 / 4 跳过（新增 `survey_controller_test.dart` 帧覆盖 + 常量用例、`survey_planning_page_test.dart` AGL 联动用例）；`tools/check_ui_tokens.dart` 通过；`cmake --build build_check` 与 `ctest`（38/38）通过；`tools/check_ffi_contract.py` 305 声明 / 112 绑定一致（无新增符号）；`tools/preflight.sh` 通过。`docs/features/survey-and-terrain.md`、`docs/constraints/parity.md`、`docs/constraints/ui-pages.md` 已按 S3b 实际结果更新。未做：`TERRAIN_REQUEST`/`TERRAIN_DATA` 回发（D4）、磁盘瓦片持久化（D3）、地形页面入口（D7）、KML/CSV 导出（D6 剩余）、任务编辑弹层帧写入的可注入组件测试。**阶段 3 方向 2（测绘与地形）四个切片 S1/S2/S3a/S3b 全部完成。**
- **阶段 3 方向 3（PX4 parity）功能简报完成（2026-10-11）**。按阶段 3「每个方向先完成产品需求、上游行为研究、license/外部依赖检查、验收标准」的要求，新增 `docs/features/px4-parity.md`（133 行）：目的、范围、M-GCS 现状（带 `file:line`）、上游研究（QGC `PX4FirmwarePlugin.cc` 模式表/capability/Home/任务命令/引导行为，MP 只作「无插件抽象、PX4 弱支持」对照）、差距 G1–G11、决策点 D1–D6、切片 S1–S6、验收标准与许可核对。**只读核对，未改任何代码**。核对出 7 个真实问题：**P1 两张 PX4 模式表漂移**（`firmware_plugin_px4.cc:59-79` 与 `px4_modes.h:16-78` 命名/集合不一致，且都缺 PX4 1.14+ 的 `ALTITUDE_CRUISE=11`、POSCTL `ORBIT/SLOW`、`AUTO_GUIDED_COURSE=19`、`VTOL_TAKEOFF=10`；`firmware_adapter.cc:180` 走前者、`resolve_mode_role`（`:238-262`）走后者）；**P2 引导语义错位**（`px4_plugin.h:29,34-36` 把引导/`is_guided_mode` 定为 `Offboard`，QGC 引导是 `AUTO_LOITER`、`isGuidedMode` 判 pause/takeoff/land，前端 `connection_controller.dart` 的 `isGuidedGotoFlightMode` 已按 QGC 纠正，core 与前端两套说法）；**P3 起飞高度语义与前置顺序**（`ffi_flight_commands.cc:147-151` 把相对高度直接写进 `NAV_TAKEOFF param7`，QGC 换算成 AMSL `PX4FirmwarePlugin.cc:322-350`）；**P4 PX4 最小起飞高度失效**（前端 `minimumTakeoffAltitudeMeters` 只读 ArduPilot 参数恒为 0，core `PX4Plugin::minimum_takeoff_altitude`=2.5 无调用者）；**P5 capability 不分机型**（`PX4FirmwarePlugin.cc:174-190`）且 `orbit` 字段报 false 而前端环绕入口用 `guided_mode` 仍可用（字段与行为不一致）；**P6 `mg_flight_get_mode_mapping` 的 PX4 通道逻辑无意义**（只认 `FLTMODE_CH`/`MODE_CH`）；**P7 7 个 `mg_px4_*` 无人调用**（`API_CATALOG.md:38-44` Dart 绑定为空）。`docs/constraints/parity.md` 新增「PX4 固件支持」小节（QGC/MP/M-GCS 三方对照 + 结论），引用 `docs/features/px4-parity.md`。**阶段 3 方向 3 只做了简报，尚未实现 S1–S6。**
- **阶段 3 方向 3 S1（统一 PX4 模式表）完成（2026-10-11）**。把 PX4 模式表收敛为唯一事实来源：`core/include/core/firmware/modes/px4_modes.h` 提供 `px4_mode_table()`/`decode_px4_mode()`/`encode_px4_custom_mode()`/`encode_px4_mode_name()`，`firmware_plugin_px4.cc` 删除自带 `kPX4Modes`（16 项）改为消费它，`firmware_adapter` 与插件不再各持一份。补齐 PX4 1.14+ 新值：`ALTITUDE_CRUISE=11`、POSCTL `ORBIT=1`/`SLOW=2`、`AUTO_GUIDED_COURSE=19`、`VTOL_TAKEOFF=10`；`SIMPLE`/`TERMINATION` 进表且 `can_be_set=false`（状态栏仍能显示）；原 RTGS（AUTO 子模式 7）标为保留、不可设置、不进表（解码走 `Mode(N)` 兜底）。命名决策：AUTO 子模式沿用 M-GCS 既有 `"Auto "` 前缀（`Auto Loiter`/`Auto RTL`/`Auto Mission`/`Auto Follow`/`Auto Precision Land`），不采用 QGC 的 `Hold`/`Return` 简写，避免同时改动状态栏与前端依赖；差异只限展示名，协议值与 `can_be_set` 与 QGC 一致。另把**参数枚举表** `px4_fltmode_slot_table()`（`COM_FLTMODE1..6`：-1 Unassigned…7 Offboard…100–107 External）与 `custom_mode` 表明确分开，`ffi_flight_modes.cc` 的 PX4 槽位名与 `options` 改走该表、通道参数改认实际存在的 `RC_MAP_FLTMODE`（P6 根因消除）。测试：`core/tests/test_firmware_adapter.cc` 新增 3 组（256×256 custom_mode 穷举「插件==adapter」、新模式编解码+`can_be_set`、双表不混用），bridge `core/tests/test_sim_ui_paths.cc` 新增 PX4 `mg_flight_get_mode_mapping` 用例。验证：`cmake --build build_check` 成功，`test_firmware_adapter` 100/100、`test_sim_ui_paths` 110/110，`build_check` 全量 `ctest` 38/38，`tools/check_ffi_contract.py` 305 声明 / 112 绑定一致。`docs/features/px4-parity.md`、`docs/constraints/parity.md` 已按 S1 实际结果更新（G1/G2 标记闭环，D1/D2 标记已决）。**方向 3 仍剩 S2（引导/起飞行为）、S3（PX4 参数元数据，需先写 ADR）、S4（任务命令覆盖）、S5（`mg_px4_*` 去向）、S6（capability 与错误路径测试）。**
- **阶段 3 方向 3 S2（引导/起飞行为对齐 QGC）完成（2026-10-11）**。按 `docs/features/px4-parity.md` 第 6 节 S2 收口：（1）**引导语义（G3/P2）** core `PX4Plugin::guided_mode_name()` 由 `Offboard` 改为 `Auto Loiter`，`is_guided_mode()` 改为 QGC 的「pause/takeoff/land」三模式判定（`PX4FirmwarePlugin.cc:664-668`），消除 core 与前端「什么是引导模式」的两套说法。（2）**起飞高度（G4/P3）** `mg_command_takeoff` 在 PX4 下取 `GLOBAL_POSITION_INT` 的 MSL 高度（mm→m）加到请求的相对高度写进 `NAV_TAKEOFF param7`（AMSL，QGC `:322-350`）；缓存无有效位置（lat/lon 全零）时返回 `MG_ERROR_INVALID_STATE` 并拒绝发送，ArduPilot 分支保持「相对 Home」不变。（3）**暂停（G4/P3）** `mg_command_guided_pause` 在 PX4 下改发 `MAV_CMD_DO_REPOSITION`（`param1=-1`、`param2=MAV_DO_REPOSITION_FLAGS_CHANGE_MODE`、其余 NAN，QGC `pauseVehicle` `:276-288`），走 ACK 等待；ArduPilot 仍发零速度 `SET_POSITION_TARGET`。（4）**capability（G6/P5）** 新增能力位 `CapRoi`/`CapChangeHeading`，PX4 按机型给（基础 `CapGuidedMode|CapPause|CapCalibration|CapSetHome`，多旋翼加 `CapRoi|CapChangeHeading|CapTakeoff|CapOrbit`，VTOL 加 `CapTakeoff|CapOrbit`，固定翼加 `CapOrbit`，QGC `:174-190`）；ArduPilot 侧同步对齐（基础含 `CapRoi`，Copter 加 `CapChangeHeading|CapOrbit`，Plane 加 `CapOrbit`，Sub 加 `CapChangeHeading`，QGC `APMFirmwarePlugin.cc:65-83`）。`mg_firmware_get_type` 的 `capabilities` 新增 `roi`/`change_heading`，前端 `supportsOrbit`/`supportsRoi` 改读 `orbit`/`roi`，地图菜单环绕/ROI 入口与字段一一对应（不再用 `guided_mode` 顺带门控）。（5）**死 API 收口（G11）** 删除无调用者的 `default_loiter_radius` 虚函数；`minimum_takeoff_altitude` 由 2.5 改为 QGC 基类默认 3.048 m（10 ft），经 `mg_firmware_get_type` 的 `minimum_takeoff_altitude_m` 暴露，前端 PX4 分支采用它（原先恒为 0）；前端 PX4 起飞前置新增「位置未知则拒绝」。测试：`core/tests/test_firmware_adapter.cc` 新增 PX4 capability/引导语义与 ArduPilot 能力位两组（131/131）；`core/tests/test_sim_commands.cc` 新增 PX4 引导命令组（AMSL 起飞 60 m、缺位置拒绝且不发帧、`DO_REPOSITION`+`CHANGE_MODE` 暂停）并钉住 ArduPilot 仍用相对高度（110/110）；`flutter_shell/test/mg_map_action_menu_test.dart` 新增「orbit/ROI 按 capability 而非 guided mode」用例。验证：`cmake --build build_check -j8` 成功，`ctest` 38/38，`tools/check_ffi_contract.py` 305 声明 / 112 绑定一致，`flutter analyze` 无问题，`flutter test` 210 通过 / 4 跳过。`docs/features/px4-parity.md`、`docs/constraints/parity.md` 已按 S2 实际结果更新（G3–G6/G11 标记闭环，D3 标记已决）。**方向 3 仍剩 S3（PX4 参数元数据，需先写 ADR）、S4（任务命令覆盖）、S5（`mg_px4_*` 去向）、S6（capability 与错误路径测试）。**
- **阶段 3 方向 3 S3（PX4 参数元数据）完成（2026-10-11）**。新增 `docs/adr/0007-px4-parameter-metadata.md`（决定 1–7：独立数据文件、来源/许可、加载作用域、不改飞控行为、仅完整区间落库、独立 profile 不回落 ArduPilot、诚实降级）。引入 `core/src/config/PX4ParameterMetaData.json`（733 KB、1836 参数，与 QGC `src/FirmwarePlugin/PX4/PX4ParameterFactMetaData.json`（QGC `bf0096d97`）逐字一致，SHA-256 `94bd6df4…eb23a9`，原始数据由 PX4-Autopilot BSD-3 生成）与 `core/src/config/PX4ParameterMetaData.LICENSE.txt`（BSD-3/Apache-2.0 原文、出处、SHA-256）。`ParamDescriptors` 加载该文件到独立 profile `px4`；`ffi_param_profile()` 拆为 `ffi_ardupilot_vehicle_class()`（只认 ArduPilot，供任务门禁）与新的 `ffi_param_profile()`（PX4 返回 `px4`），`ffi_mission_document.cc` 两处改走前者；`get_from_profile()`/`get_group()` 对 `standalone_profiles_` 不再回落 ArduPilot default。值域仅当快照同时给 `min`+`max` 才落库（单边界 146 参数不写 `max=0`）。打包清单同步：`CMakeLists.txt`、Linux/Windows/macOS/Android、`tools/check_*_bundle.dart`、`.github/workflows/build.yml` APK 资产断言。测试：`core/tests/test_param_descriptors.cc` 新增 `test_px4_metadata()`（67/67，覆盖显示名/描述/分组/单位/枚举/只读/重启标记、单边界不落区间、px4 profile 不回落 ArduPilot 默认描述）；`cmake --build build_check` 成功。ADR-0002 补充「纯数据例外 + 再分发声明清单」。`docs/features/px4-parity.md`、`docs/constraints/parity.md` 已按 S3 更新（G7/D4 标记闭环）。**方向 3 后续切片 S4–S6 见下方条目。**
- **阶段 3 方向 3 S4（PX4 任务命令覆盖与 Home 语义）完成（2026-10-11）**。按 `docs/features/px4-parity.md` 第 6 节 S4 收口：（1）**参数可见性（G8）** `mission_command_catalog` 新增 profile 感知的 `mission_command_parameter_visible()`，按 QGC `PX4-MavCmdInfoCommon.json` 的 `paramRemove` 隐藏 PX4 不适用的字段（`NAV_WAYPOINT`(16) 去 param2/3、`NAV_LAND`(21) 去 param1、`VIDEO_START_CAPTURE`(2500) 去 param2；PX4 机型级覆盖文件为空，无需机型细分），`mg_mission_command_catalog` 遍历参数时跳过不可见项并给每条命令附 `vehicles` 数组；只影响前端显示、不改已存值。（2）**Home 语义** 核对 `bridge/ffi/ffi_mission_bridge.cc:10 mission_uses_home_item()`：只对 ArduPilot 前置 Home item，PX4 本就不把首条位置当 Home，与 QGC `sendHomePositionToVehicle()=false`（`:201-205`）一致，无需改动；`CapSetHome` 保留（`DO_SET_HOME` 仍在 PX4 `supportedMissionCommands` 内）。（3）**`DO_LAND_START`(189) 坐标** 新增 `mission_command_uses_location(command, firmware_profile)` 重载（`core/include/core/mission/mission_types.h`）：QGC 把 PX4 的 189 标成 `specifiesCoordinate:false`，而 ArduPilot `AP_Mission::stored_in_location()` 含 189；PX4 下 189 不承载坐标，其余落回原语义。已接入 `mission_compile.cc:179`、`mission_document.cc:1103`、`mission_qgc_plan.cc:945/1179/1190` 与 `bridge/ffi/mission_json_serializers.h`（各接口新增 profile 参数，默认空串保持 ArduPilot 语义）。（4）**D6 版本门控** 决定不实现：QGC 把 `DO_SET_MISSION_CURRENT` 判为不支持并退回 `MISSION_SET_CURRENT`，M-GCS `mg_mission_set_current` 本就发 `MISSION_SET_CURRENT`，行为等价且 M-GCS 无飞控版本解析。测试：`core/tests/test_sim_ui_paths.cc::test_mission_catalog_param_remove_per_firmware()`、`core/tests/test_sim_mission.cc::test_mission_plan_json_land_start_per_firmware()`、`core/tests/test_mission_document.cc::test_land_start_location_override_per_firmware()`（含编译期目录门禁现状）。验证：`cmake --build build_check -j8` 成功，`ctest` 38/38。`docs/features/px4-parity.md`、`docs/constraints/parity.md` 已按 S4 更新（G8 闭环、D6 已决）。**方向 3 仍剩 S5（`mg_px4_*` 去向）、S6（capability 与错误路径测试）。**
- **阶段 3 方向 3 S5+S6（`mg_px4_*` 定性与 PX4 行为/错误路径测试）完成（2026-10-11）**。按 `docs/features/px4-parity.md` 第 6 节收口：（1）**S5 定性（G9/P7/D5）** 7 个 `mg_px4_*`（执行器/模式槽/安全/调参）定为 **FFI 内部能力**——保留给原生工具与测试，无 Dart 绑定、无独立 UI 计划（通用参数页 `mg_params_*`/`mg_config_*` 配合 S3 的 `px4` 描述符 profile 已覆盖同一批参数）；`docs/capabilities.md` 与 `bridge/ffi/API_CATALOG.md` 标注其定位，`tools/check_ffi_contract.py` 新增 `INTERNAL_SYMBOL_NOTES` 与守卫（内部符号一旦出现在 Dart 绑定即报错，引用不存在的导出也报错）。（2）**修两个真实缺陷**：旧 `mg_px4_safety_get/set` 写死 `CBRK_AIRSPD_CHK`/`CBRK_GPS_CHK`/`CBRK_IO_SAFETY`，其中 `CBRK_GPS_CHK` 在 QGC `PX4ParameterFactMetaData.json` 与 PX4-Autopilot `e7b1d486e5` 源码里**都不存在**（永远读 0、set 静默失败）——改为按飞控实际参数缓存报告 `CBRK_*`（字段名即参数名），set 对非 `CBRK_` 键返回 `INVALID_PARAM`、飞控无该参数返回 `NOT_FOUND`、发送失败返回 `SEND_FAILED`；旧执行器只读写 `PWM_MAIN_FUNC1..8`，而 PX4 主输出有 16 路——扩到 16 路（缺失通道按默认 0 = Disabled）。`mg_px4_flight_mode_get_config` 明确**只读** `COM_FLTMODE1..6`（写入走通用参数页，避免与 `RC_MAP_FLTMODE` 不一致而失去可用模式）。（3）**S6 测试** `core/tests/test_sim_ui_paths.cc` 新增四组：`test_px4_actuator_maps_all_sixteen_channels()`、`test_px4_safety_reports_only_real_circuit_breakers()`、`test_px4_flight_mode_slots_are_read_only_list()`、`test_px4_apis_reject_ardupilot()`；`core/tests/test_ffi_contract.cc` 的 PX4 组由「只断言返回非空」改为断言离线统一 `NOT_SUPPORTED` 错误对象（F4）。验证：`cmake --build build_check -j8` 成功，`ctest` 38/38，`test_sim_ui_paths` 142/142（新增 22 项断言），`tools/check_ffi_contract.py` 通过。`docs/features/px4-parity.md`、`docs/constraints/parity.md` 已按 S5/S6 更新（G9/G10 闭环、D5 已决）。**方向 3（PX4 parity）S1–S6 全部完成，验收项闭环。**
- **阶段 3 方向 4（视频/相机/云台）功能简报完成（2026-10-11）**。按阶段 3「每个方向先完成产品需求、上游行为研究、license/外部依赖检查、验收标准」的要求，新增 `docs/features/video-camera-gimbal.md`：目的、范围、M-GCS 现状（带 `file:line`）、上游研究（QGC `QGCCameraManager`/`GimbalController`/`VideoManager` 与 MP `CameraProtocol`/`GimbalManagerProtocol`，只参考行为）、差距 G1–G9、决策点 D1–D5、切片 S1–S4、验收标准、平台矩阵与许可核对（QGC 双许可、MP GPLv3 禁止复制源码）。**只读核对，未改代码**。核对出 5 个真实缺陷：**G1 `mg_video_start` 误发 `camera_start_video`**（把「启动视频源」当成「开始录像」）；**G2 读接口硬编码假值**（`mg_camera_get_status` 恒 `available:false`、`mg_video_status` 恒 `status:"stopped"`）；**G3 相机/云台状态只落日志、无缓存**，`CameraManager::process_message` 是死代码；**G4 dispatcher 缺 `CAMERA_SETTINGS`(260)/`STORAGE_INFORMATION`(261)/`VIDEO_STREAM_STATUS`(270)/`GIMBAL_DEVICE_INFORMATION`(283)/`GIMBAL_DEVICE_ATTITUDE_STATUS`(285)**，且 `case 261/254` 注释错位（261 实为 `STORAGE_INFORMATION`、254 实为 `DEBUG`）；**G5 `camera_id` 被 `(void)` 丢弃**，命令永远打向活动 target。**阶段 3 方向 4 只做了简报，S1 见下。**
- **阶段 3 方向 4 S1（native 状态层 + FFI 读契约）完成（2026-10-11）**。按 `docs/features/video-camera-gimbal.md` 第 6 节收口：**（1）修 G1/G2/G4**——dispatcher 的相机/云台 case 全部改用 `MAVLINK_MSG_ID_*` 宏（覆盖 259/260/261/262/263/269/270/280/281/283/285），消除 `261=STORAGE_INFORMATION`/`254=DEBUG` 被误当相机信息的错位；适配器按 `(sysid, compid[, stream_id])` 建相机信息/设置/拍摄/存储/云台管理/云台设备/姿态/视频流状态缓存，替换原「只打日志」实现；修正 `GimbalDeviceInformationInfo` 的 `uid`（`uint8_t`→`uint64_t`）与 `cap_flags2`（`uint16_t`→`uint32_t`）截断。**（2）FFI 读契约**——`mg_camera_get_status` 返回信息/设置/拍摄/存储，新增 `mg_gimbal_get_status`/`mg_gimbal_get_attitude`（四元数→欧拉角），`mg_video_status` 返回真实流列表并与 `VIDEO_STREAM_STATUS` 配对、`playing:false`；无设备时读接口 `success:true`+`available:false`（D5），命令类按 `camera_id`/`gimbal_id` 解析 `(sys, comp)` 寻址（G5/D4）；`mg_video_start(uri)` 不再误发录像命令——空 URI `INVALID_PARAM`、非空 `NOT_SUPPORTED`（本地解码待 S3）。**（3）死代码清理（G3/D1）**——本仓库无调用点的 `CameraManager`/`VideoReceiver` 删除（含 `CMakeLists.txt`、`Vehicle` 持有与转发），设备状态唯一事实来源统一到适配器缓存。验证：`cmake --build build_check -j8` 成功，`ctest` 38/38，`test_mavlink_protocol` 193/193（新增相机/云台/视频流缓存与 259/261/254 回归）、`test_new_modules` 145/145（新增 `test_video_ffi`，扩展相机/云台无设备断言），`tools/check_ffi_contract.py` 307 声明 / 112 绑定一致（+2 云台读导出），`tools/preflight.sh` 全部通过。`docs/features/video-camera-gimbal.md`、`docs/capabilities.md`、`bridge/ffi/API_CATALOG.md` 已按 S1 实际结果更新。**方向 4 仍剩 S2（前端相机/云台页）、S3（本地解码，先写 ADR）、S4（录像/截图落盘）。**
- **阶段 3 方向 4 S2（前端相机/云台页 + 多设备寻址）完成（2026-10-11）**。新增 `flutter_shell/lib/payload/`（`payload_models` 模型、`payload_backend` 接口 + 不可用替身、`payload_ffi_backend`（`dart:ffi`，A5 白名单）、`payload_controller` 状态机、`camera_gimbal_page` 页面）；入口在设置菜单「相机与云台」→ 配置工作区 `payload` 页，页面只经控制器读写、不直接接触 backend（U1/A5）。三张卡片：相机（清单 + 型号/分辨率/模式/拍摄/存储）、云台（清单 + 能力范围 + 姿态）、视频源（流列表 + 地址行）。**多设备寻址（G5/D4）**：按 MAVLink compId 选择（「自动（第一台）」→ `id<=0`），选择后立即用该 compId 重读；D4 定为 compId 语义（与 QGC 一致）。**诚实措辞**：命令成功只显示「已发送…」，失败/未连接复用 native 规范错误对象，无设备显示「已查询、当前无设备」。**视频**：只展示 `mg_video_status`/`mg_video_get_streams` 元数据并明确「本地解码未实现（S3）」；`startVideo` 空 URI 前端拒绝、非空照实转发 native 的 `NOT_SUPPORTED`。测试：新增 `payload_controller_test.dart`（9 用例）、`camera_gimbal_page_test.dart`（3 用例）、`test/support/payload_backend_fake.dart`，`architecture_ffi_boundary_test.dart` 白名单登记新 backend。`flutter analyze` 无问题、`flutter test` 全绿、`dart tools/check_ui_tokens.dart` 通过、`ctest` 38/38、`tools/check_ffi_contract.py` 307 声明 / 129 绑定一致。文档：`docs/capabilities.md` 的 `ffi_api_payload.h` 由「待接入」改「部分接入」（19 导出 / 17 绑定），`docs/constraints/ui-pages.md` 登记新页，`docs/features/video-camera-gimbal.md` 标 S2 完成、D4/G5 收口。**方向 4 仍剩 S3（本地解码，先写 D3 ADR 与许可/平台矩阵）、S4（录像/截图落盘 + 容量管理）。**
- **阶段 3 方向 4 S3（本地视频解码/渲染管线 + D3 决策）完成（2026-10-11；S4 随后落地，见下）**。按 `docs/features/video-camera-gimbal.md` 第 6 节收口：**（1）D3 → `docs/adr/0008-video-decode-backend.md`（已接受）**——本地解码后端选 **FFmpeg 且只允许 LGPL 构建**（`--disable-gpl`），帧交付走 **FFI 拷贝 + 前端 `ui.decodeImageFromPixels`**（不动平台纹理、`dart:ffi` 仍只在 backend 层），无后端诚实降级；备选 GStreamer/平台原生保留为未来 ADR。**（2）native**——删除遗留死代码 `core/{include,src}/video/ffmpeg_decoder.{h,cc}`（无调用点、`MGCORE_HAS_FFMPEG=OFF` 时空壳），新增 `core/include/core/video/video_decoder.h` + `core/src/video/video_decoder.cc`（`VideoFrameBuffer` 单槽最新帧 + 序列号 + 自带互斥量、`VideoDecoder`），新增 `core/tests/test_video_pipeline.cc`（合成帧走帧/轮询契约，后端无关，35 断言）。**（3）FFI**——`bridge/ffi/ffi_api_payload.h` 新增 `mg_video_renderer_status`（`{success,available,backend("none"|"ffmpeg"),running,uri,has_frame,width,height,pts_us,sequence}`）与 `mg_video_poll_frame(core,dst,cap,out_w,out_h,out_pts,out_seq)`（`>0` 字节数 / `0` 无帧 / `-1` 容量或参数错误并回报所需尺寸）；`mg_video_start` 语义固定为「本地视频源启停」（无后端 `NOT_SUPPORTED`、空 URI `INVALID_PARAM`、打不开 `SEND_FAILED`），与 `mg_camera_start_video` 彻底区分；`mg_video_status.playing` 仅解码器运行且有新帧时为 `true`。**（4）Flutter**——`payload_models` 加 `VideoRendererStatus`；`payload_backend` 加 `rendererStatus()`/`pollFrame()`/`dispose()` + `VideoFrameData`，不可用替身返回合法能力态（`available:false/backend:"none"` 非错误）；`payload_ffi_backend` 绑定两符号并复用原生帧缓冲；`payload_controller` 加 renderer 读取/轮询/会话清理；新增 `flutter_shell/lib/payload/video_preview.dart`（运行中 33 ms 轮询、序列号去重、`RawImage` 渲染），`camera_gimbal_page.dart` 视频面板重写（无后端显示「本平台未启用本地视频解码（此构建未包含 LGPL FFmpeg 后端）」，有后端显示后端名/地址/尺寸帧号 + 预览）；`connection_controller.dispose()` 释放 backend。验证：`flutter analyze lib test` 无问题，`flutter test` 231 通过 / 4 跳过（+7 用例），`dart tools/check_ui_tokens.dart` 通过，`tools/check_ffi_contract.py` 309 声明 / 131 绑定一致（+2 视频导出），`API_CATALOG.md`/`docs/capabilities.md` 已重生成。**诚实边界**：真帧 `ui.decodeImageFromPixels` 未在 widget 测试覆盖（`flutter_tester` 下异步平台通道污染 `runAsync`），该 S3 收口时本机尚无 FFmpeg/pkg-config，`MGCORE_HAS_FFMPEG` 分支未编译验证（S4 已在 macOS 用本地 LGPL FFmpeg 7.1.1 构建编译并跑通真实码流），真实相机/云台硬件回读仍需真机。**S3 退出条件仍未达成**：至少两端平台播放一路真实 RTSP/UDP 流并落能力矩阵，现已有 macOS 一端；达成前其余平台标 `unsupported`。**方向 4 的 S4（录像/截图落盘 + 容量管理，对齐 QGC `maxVideoSize`/`enableStorageLimit`）随后完成，见下方条目。**

- **阶段 3 方向 4 S4（本地录制/截图落盘 + 容量管理）完成（2026-10-11），方向 4 收口**。**（1）native**——`core/include/core/video/video_decoder.h` + `core/src/video/video_decoder.cc` 新增 `start_recording`/`stop_recording`/`recording_status`/`capture_frame`：录制是**码流复制**（`av_interleaved_write_frame`，不重编码），容器按扩展名决定（UI 固定 `.mkv`/Matroska），源无时间戳时按帧率补合成时间轴；写帧在 `rec_mtx` 内，打开/写头/收尾（`av_write_trailer`/`avio_closep`）都在锁外（A7），写失败立即收尾并把原因留在 `RecordingStatus.error`；截图取帧缓冲最新帧，按扩展名编码 PNG（RGBA）或 JPEG（YUVJ420P）。**（2）FFI**——新增 `mg_video_start_recording`/`mg_video_stop_recording`/`mg_video_recording_status`/`mg_video_capture_frame`；契约：空路径 `INVALID_PARAM`（先于后端检查）、无后端 `NOT_SUPPORTED`、有后端但无运行源/无可用帧 `INVALID_STATE`、未录制时 `stop_recording` 是幂等成功；`recording_status` 恒为成功信封，字段 `available`/`recording`/`path`/`error`/`bytes`/`duration_us`，停止后保留最终结果。**（3）Flutter**——`payload_models` 加 `VideoRecordingStatus`；`payload_backend`（含不可用替身）与 `payload_ffi_backend` 加四方法；新增 `flutter_shell/lib/payload/video_storage.dart`（`VideoStorageSettings` JSON 存应用支持目录、平台上不可用退回临时目录；默认上限 10240 MB、下界 100 MB，对齐 QGC `maxVideoSize`；`listRecordings`/`pruneOldRecordings` 越旧越先删且不删正在写的文件）；`payload_controller` 加 `startLocalRecording`/`stopLocalRecording`/`captureLocalFrame`/`refreshRecordingStatus`，这些本机操作走 `_send(..., requiresVehicle:false)`——**不依赖飞控连接**（相机命令仍要求连接）；`camera_gimbal_page` 在后端可用时增加录制面板（状态 + 目录 + 保存 + 容量上限 + 开始/停止 + 截图），后端不可用时整块不画。**（4）测试**——`core/tests/test_video_pipeline.cc` 新增「video recording/capture FFI contract」组（45 断言），`test_new_modules.cc` 的 video FFI 改为按后端能力分支（145/145）；Flutter 新增 `test/video_storage_test.dart`（7 用例）、`payload_controller_test.dart` 的「本地录制与截图」组（6 用例）与 `camera_gimbal_page_test.dart` 的 2 个页面用例（页面新增可注入 `storageLoader`，Widget 测试不依赖平台通道）。验证：`flutter analyze` 无问题、`flutter test` 247 通过 / 4 跳过、`dart tools/check_ui_tokens.dart` 通过、`tools/check_ffi_contract.py` 314 声明 / 136 绑定一致（+4 导出，`API_CATALOG.md`/`docs/capabilities.md` 已重生成）、`tools/preflight.sh` 全量通过（含 build_check 后端口禁与 macOS debug 构建）。**（5）端到端**——在 macOS(arm64) 用本地 LGPL FFmpeg 7.1.1（`--disable-gpl`）构建跑通真实码流：`testsrc` 经 TCP/MPEG-TS 喂给 `mg_video_start`，`mg_video_poll_frame` 取到 640x480 帧；`mg_video_start_recording` 落出可被 `ffprobe` 读取的 Matroska（mpeg4 640x480）；`mg_video_capture_frame` 落出 640x480 PNG 且画面与预览一致；`storageLoader` 之外无测试桩。**（6）文档**——`docs/features/video-camera-gimbal.md` 标 S3/S4 完成并新增第 11 节「手动验证步骤（真实码流）」；`docs/adr/0008-video-decode-backend.md` 平台矩阵更新 macOS 行为「开发构建可用」；`docs/constraints/parity.md` 新增「视频 / 相机 / 云台」对照；`docs/constraints/ui-pages.md` 登记录制面板；`CHANGELOG.md` 同步。**方向 4 已完成；仍开放的是「至少两端平台」的退出条件（现只有 macOS 一端）与把 LGPL FFmpeg 打进各平台发布包（R6）。**



| 顺序 | 任务 | 依赖 | 交付物 | 完成信号 |
| --- | --- | --- | --- | --- |
| 0 | 能力清单与 KD 台账复核 | 无 | `docs` 中的能力状态/owner/test 表 | 导出函数和 UI 功能状态无歧义 |
| 1 | Native freshness snapshot → FFI → fleet → UI | 0 | per-vehicle validity model + contract tests | 过期/未知状态不能触发危险操作 |
| 2 | command operation/result 统一与 late ACK 隔离 | 1 | transaction interface、mock adapter、ACK tests | send/ACK/reject/timeout/duplicate/late ACK 均有区分 |
| 3 | calibration attempt/cancel/compass set | 1、2 | lifecycle model、capability map、multi-compass tests | 取消失败和旧进度不再虚报完成 |
| 4 | parameter write/download generation | 1、2 | parameter transaction interface、stale frame tests | 错 target/旧回复不能确认新请求 |
| 5 | core 锁/回调/连接超时整改 | 0 | 小型分项 ADR 或实现说明、TSan/故障注入 | 锁外回调、bounded I/O、线程错误传播 |
| 6 | ArduPilot Copter vertical slice | 1–5 | tested workflow 和 SITL scenario | 规划、上传、核验、执行、断链恢复完整 |
| 7 | 单项产品扩展 | 6 | feature brief + parity + tests | 每个功能单独验收，未完不显示 supported |
| 8 | 平台 CI/发布可靠性 | 1–7 持续 | platform evidence table、artifact smoke | 每平台有明确证据等级和 release check |

任务可以在不同模块独立实现，但 interface/state vocabulary 必须先合并；异步操作不能由 UI 直接读取全局 active vehicle 再于稍后发命令。

## 5. 统一 Definition of Done

功能只有同时满足以下条件才从 `planned` 移至 `delivered`：

1. 对应用户工作流和 firmware/vehicle capability 有清楚说明，用户可从实际 UI 到达。
2. 成功、拒绝、发送失败、超时、断链、取消、重连和 target change 的结果定义明确；UI 不推测成功。
3. Native/core 是权威状态源；所有异步结果带 Vehicle/session 身份，旧响应不能改变新焦点。
4. 测试覆盖正常路径和至少一条真实风险失败路径；安全相关规则必须有 behavior assertion，不使用 `CHECK(true)` 或恒真 OR。
5. FFI 更新同步接口目录/类型/所有权/错误格式；动态库公开符号和版本一致。
6. 在声明支持的平台有 build、bundle、runtime smoke 证据；未测平台只可标实验性。
7. 文档写当前事实、许可证与外部依赖；偏差台账仅留未解决的问题。

## 6. 暂不列入近期承诺

- 和 QGC 在所有复杂任务项、所有 PX4 setup 页、所有相机与 GStreamer 平台组合上逐项持平。
- 多机规模扩展或无人值守机队控制，直到 Vehicle/link/ACK/session 边界完成压力与竞态验证。
- 自动固件下载/刷写，直到有可恢复的板型/签名/断电方案和硬件测试流程。
- 用 FFI 导出数、源码行数或 UI 页面数量当里程碑指标。
- 在没有产品决策前重构整个 Flutter `ConnectionController` 或全量重写 core 模块。

## 7. 计划复审

每个阶段验收后复审一次：更新能力表、`known-deviations.md`、对比报告和下一阶段排序。QGC/MP 本地基准提交变更时，只重核受影响的功能域，不把旧源码行号继续当现行证据。
