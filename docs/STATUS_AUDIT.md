# 现状审计

> 本文是当前滚动审计，不再冻结某一次发布。历史快照保存在
> [`docs/audits/`](audits/)。

## 审计范围

- 代码基线：`v0.0.10`（commit `0c8b193`）之后再叠加 `fix:` 校准提交与本轮审计修订。
- 本文更新时间：2026-09-29（第三轮：关闭 `CAL-001`、`CAL-002`、`TEST-001`、`TEST-002`、`TOOL-002`、`CONTRACT-001`）。
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
| 测试触达 | 185 |
| 既无前端调用、也无测试触达 | 95 |

按领域头拆分（三列互斥，逐行求和等于导出数）：

| 领域头 | 导出 | 前端在用 | 仅测试 | 两者皆无 |
| --- | ---: | ---: | ---: | ---: |
| configuration | 43 | 5 | 19 | 19 |
| connection | 21 | 6 | 10 | 5 |
| core | 12 | 3 | 8 | 1 |
| environment | 5 | 0 | 4 | 1 |
| firmware | 7 | 3 | 3 | 1 |
| flight | 32 | 0 | 32 | 0 |
| geofence | 16 | 0 | 4 | 12 |
| logging | 22 | 0 | 3 | 19 |
| mission | 40 | 15 | 9 | 16 |
| params | 20 | 6 | 3 | 11 |
| payload | 17 | 0 | 13 | 4 |
| telemetry | 46 | 10 | 30 | 6 |

说明：

- 导出数包含新增的任务规划 FFI，因此不再等于 v0.0.8 快照中的 266。
- “无人区”不等于可删除代码。专用公开面仍需按 `API_CATALOG.md` 的稳定性规则处理。
- 完整 95 个接口清单可由 `tools/audit_status.sh` 重新生成。
- 早期版本的逐域表格三列不互斥（“仅测试”含前端已用的项），已在修正 `TOOL-001` 时一起修好。

## 4. 当前问题登记

### ARCH-001：core 逆向依赖 bridge

- 状态：`OPEN`
- 实测：`core/include/core/vehicle/vehicle.h` 直接 include `bridge/ffi/telemetry_cache.h`。
- 影响：违反 `core → third_party`、`bridge → core` 的单向依赖约束。
- 原因：`mgcore` 把 `bridge/ffi` 加到了 PUBLIC include 路径。
- 修复方式：把 `TelemetryCache` 下沉到 core，或为 Vehicle 定义 core 侧缓存接口，并收紧 CMake include 可见性。
- 回归保护：给 `test_bridge_architecture_guards.cc` 增加 core→bridge 方向断言。
- 现状：`TelemetryCache` 依赖 `json_serializers.h` / `telemetry_freshness.h` 等 bridge 私有头，
  下沉需要连同这组序列化器一起迁移，属于独立重构；本轮不动，避免半个迁移让依赖更乱。

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

- 状态：`OPEN`
- 接口：`mg_params_prepare_change_set`、`mg_params_commit_change_set`。
- 风险点：去重、未知参数、未变化参数、逐项失败和 `_ENABLE` 最后写入。
- 影响：这是写参数路径中最容易造成部分应用状态的接口组。

### DEPRECATION-001：兼容接口没有退役状态

- 状态：`OPEN`
- FFI 中 deprecated 标记仍为 0。
- 兼容别名、空壳 `mg_rc_calibrate_save` 和返回空列表的 `mg_log_list` 没有明确退役计划。
- 处理：建立 deprecated 标记和替代关系，不直接删除公开符号。

### MISSION-001：Flutter 任务规划层尚未连接后端

- 状态：`OPEN`
- 实测：`flutter_shell/lib/mission/`（`mission_document_models.dart`、`_backend.dart`、`_controller.dart`）
  三个文件只互相 import，既不引入 `dart:ffi`，也不依赖 `ConnectionController`。
- 现有验证：只有 `test/mission_document_controller_test.dart` 的纯 Dart 单元测试。
- 影响：任务规划的 Dart 侧目前是纯本地模型层，后端 `core/src/mission/cad/` 与 `bridge/ffi/ffi_mission_*.cc`
  提供的能力还没被任何 UI 触达；mission 领域 40 个导出中仍有 16 个落在无人区。
- 下一步：明确 “编辑在 Dart、编译与上传在后端” 的调用边界，再补 FFI 绑定，避免前端复制一份几何或编译逻辑。

### CLEANUP-001：组件陈列页无人引用

- 状态：`OPEN`
- 实测：`flutter_shell/lib/component_gallery_demo.dart`（1,047 行）没有被任何页面或测试 import。
- 影响：它仍持有 `MacosColors.*` 引用，会让 token 扫描的豁免范围长期存在。
- 处理：确认是否已无展示价值；若保留，应在文件中注明用途并加入扫描豁免清单，否则删除。

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
- 验证：修复后脚本输出 281 / 48 / 179 / 101。叠加本轮 `TEST-001` / `TEST-002`
  的新测试后，当前值为 **281 / 48 / 185 / 95**（以第 3 节与脚本实跑为准）。

### CI-001：远端 CI 运行状态未在本地审计确认

- 状态：`UNVERIFIED`
- 原因：本审计不访问远端。
- 已知历史：v0.0.8 时 CI 因私有仓库计费被拦截，作业从未真正运行。
- 完成标准：在 GitHub Actions 页面确认 4 个 job 实际执行，而不是只看 workflow 是否触发。

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
| Windows 串口 | 修复 ANSI/UTF-8 越界写、补齐 DCB/清理/部分写处理、加强读线程退出 |
| Windows 飞控发现 | USB 遥测电台 `SiK Radio` 现在也会标记为飞控候选 |
| Windows 打包 | Flutter Windows 构建显式继承生成器/架构，安装阶段校验 DLL 和数据文件 |
| Windows CI | 新增 `windows-shell` job，构建并上传 Windows debug 产物 |

## 6. 建议顺序

1. 把 `MISSION-001` 的后端任务规划真正接到 Flutter（先定边界，再绑 FFI）。
2. 修正 `ARCH-001` 的 core→bridge 逆向依赖。
3. 接入 `UI-001` / `UI-002` 聚合诊断和长任务取消。
4. 补 `TEST-003` 批量参数变更集的验证。
5. 清理 `CLEANUP-001` 的孤儿陈列页，或明确其豁免地位。
6. 启动 `DEPRECATION-001`，让兼容接口进入明确退役流程。
7. 远端 CI 恢复后，以一次真实运行关闭 `CI-001`。

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
| 本轮改动 | 关闭 `CAL-001`、`CAL-002`、`TEST-001`、`TEST-002`、`TOOL-002`、`CONTRACT-001`；刷新门禁与统计 |
| 复核状态 | **未经人工复核**；本地门禁（含 macOS debug 构建）已实跑通过 |

> 按 [协作流程](COLLABORATION.md) 中“不要由实现者自证”的约定，本文所列修改建议
> 应由另一人复跑同一验收命令后再采纳。
