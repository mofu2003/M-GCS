# M-GCS 质量加固轮交接文档（第二份）

**写于**：2026-10-12
**基线 commit**：`3c5bdc7`（`chore(release): 0.2.1`）
**当前状态**：本工作区已提交 **8 个本地提交**（`b14751f`…`b32270e`），工作树 **0 项未提交**，**未推送**
**上一份交接**：`docs/workflow/HANDOFF-2026-10-12.md`（同日的"大清洗"轮，其 43 项改动已由本轮提交固化）
**上游参考**：QGC `~/Projects/qgroundcontrol`（`bf0096d97`）、MP `~/Projects/MissionPlanner`（`a2fcd74d6`）、ArduPilot `~/Projects/ardupilot`（`2d534eb842`，实为 4.8.0-dev）、PX4 `~/Projects/PX4-Autopilot`

---

## 0. 这份文档怎么用

1. **先读第 1 节**，理解这轮做了什么、交付了什么。
2. **读第 2 节**，确认工作区状态与验证方式（**先跑一遍验证命令确认基线**）。
3. **第 3 节是给下一个 agent 的手把手步骤**——从那里开始做。
4. 第 4 节是剩余待办全景（已分级、已定位）。
5. **第 5 节务必读**——这轮踩的坑里，有一半是**流程性**的（我自己犯的契约错误与一次真实回归），能直接省掉重复劳动和误判。

**铁律（延续上一份，本轮被验证有效）**：
- **一次只做一个**。做完、验证、给用户看，再问下一步。
- **动手前先看 QGC/MP/ArduPilot 怎么做**（源码在本机 `~/Projects/`）。不要凭直觉猜。
- **不确定是"死代码"还是"待接线"时，默认不删**。项目历史上误判过 3 次；本轮由独立验证者反向把关，**确认零误删**。
- 每个修复都要有**能证明旧代码会失败的验证**，不能"改完就说好了"。本轮把这条做成了硬标准：**单次通过不足以判"通过"，单次失败也不足以判"不通过"**（并发构建会产生伪失败，见 5.4）。

---

## 1. 这轮做了什么

分两部分：**(A) 我在组建团队之前亲自完成的 8 项修复**，**(B) 随后用 6 人 agent 团队完成的 16 个任务**。

### A. 亲自完成的 8 项修复（全部双向验证）

| # | 缺陷 | 危害 | 验证方式与结果 |
|---|---|---|---|
| 1 | `send_setup_signing` 解锁后仍用共享 `impl_->msg`（`core/src/mavlink/mavlink_adapter.cc`） | 并发发送时发出撕裂帧；这是上一份交接声称"已修 21 处"时**遗漏的第 8 处** | TSan：旧码报**多处 data race**（`mavlink_finalize_message_buffer`）→ 新码 0；回归测试 `test_setup_signing_does_not_share_scratch_buffer` |
| 2 | 日志下载 32 位回绕导致**堆越界写**（`core/src/mavlink/mavlink_adapter_log.cc:39`） | 全项目唯一的内存安全缺陷；飞控给 `ofs≈UINT32_MAX` 时 `end` 回绕，`download_size==0` 时越界写 | ASan：旧码 **SEGV @ 0x0000fffffff0**，崩溃点正是越界那一行 → 新码干净；新增偏移守卫 + 循环上界 |
| 3 | 盘旋半径校验写成"必须 > 0"（`mission_command_catalog.cc`、`mission_validation.cc`） | 会**误拒合法任务**并阻断上传 | 依据：MAVLink 定义正值=顺时针、负值=逆时针；QGC `MavCmdInfoCommon.json` 给 param3 `userMin=-100`。改为 `[-100,100]` + 非有限值才报错 |
| 4 | ADSB `callsign` 越界读（`mavlink_adapter_handlers.cc`） | `memcpy(sizeof(dst))` 目标 `char[10]`、源 `char[9]`，**多读的正是紧跟其后的 `emitter_type`** | 回归测试：旧码 `callsign[9]` 被写入 emitter_type 字节 → FAIL；新码走已有的 `copy_str()` |
| 5 | 串口发现手写 JSON 扫描器（`serial_port_discovery.{h,cc}`） | 用 `find(']')` 找数组结尾，遇正则里的 `]` 就截断 → `boardDescriptionFallback` **只读到 35/51 条，静默丢 16 条板卡**；且 `androidOnly`（bool）被当成带引号字符串读，**恒为 false**，那条标注 "too dangerous for non-android" 的兜底规则本应在桌面端被排除却从未生效 | 用真实 JSON 复现 + 回归测试：旧码 `ARK BL FMU v6S`/`FT231X USB UART` 均匹配不上 → FAIL；新码 51/51 且护栏生效。改用仓库已有的 cJSON 封装 |
| 6 | `HIGHRES_IMU` 气压被多除 100（`mavlink_adapter_handlers.cc`） | 对外读数恒小 100 倍 | 方言头确认字段单位**本来就是 hPa**；对照同文件 `handle_scaled_pressure()` 对同为 hPa 的 `press_abs` 就是直接赋值。回归测试双向验证 |
| 7 | 参数重映射表 5 条**编造条目**（`core/src/firmware/param_remapping.cc`） | `TECS_CLMB_MAX_M`/`ARSPD_FBW_MIN_M`/`CRUISE_SPEED_M` 等在上游**不存在**，会让飞控报的真实参数被规范化成不存在的名字 | 机械化审计（78 条映射逐条比对 ArduPilot 4.8-dev 索引）+ 交叉核对 QGC 完整表；另清掉 3 条恒等映射与 1 条重复 |
| 8 | Sub 模式表缺 `SURFTRAK=21`（`core/include/core/firmware/modes/ardu_sub_modes.h`） | 模式 21 解码成 `"Mode(21)"` 且界面选不到 | 依据 `ArduSub/mode.h:53` + QGC `availableFlightModes`（带 `CanBeSet` 列）。顺带全表交叉核对：现有 10 条 Sub 条目的 `can_be_set` 与 QGC **逐条一致** |

### B. 团队完成的 16 个任务（13 完成 / 3 被基础设施打断）

| 任务 | 内容 | 状态 |
|---|---|---|
| t1 / t11 | 文档漂移同步 + 收口（死链、越界行号、"37→39"计数、coordinate/hoverSpeed 声明、ui-pages 补登记 10 页 + terrain 模块、ADR-0004 许可状态、KD-45 计数 108/19→111/16） | ✅ |
| t2 | 5 组安全/正确性修复：`mg_param_file_import_csv` 假成功、gyro/baro/accel 校准子串判成功、RTL 高度校验、mission 多机串扰、F4 响应契约 | ✅ |
| t3 | 3 项并发：断链悬垂适配器（改"窄修法"持租约）、Android 持锁回调、命令队列槽位可回收 | ✅ |
| t4 | 门禁：**TSan 进 CI**、S6 守卫补插值与 core+bridge 盲区、Windows 门禁对齐、testing.md 去掉不存在的自动检查 | ✅ |
| t5 | 死代码与重复清理：FFI 6 个内部死符号、2 个校准转发导出、手写 JSON 扫描器→cJSON、telemetry JSON 转义、Dart 168 行死弹层、`_read` 10 份中合并 7 份（3 份按契约差异保留） | ✅ |
| t6 | 测试基建：`CHECK` 宏 31 份归一为共享头、S3/S5/S7/S8 缺口测试、**把 testing.md/ffi-contract.md 的空承诺落成真脚本**（`check_fake_assertions.py`、`check_api_change_set.py`）、F3 契约测试 | ✅ |
| t7 | 独立验证 t4+t1 | ✅ 通过（另发现 3 处引用不精确 + 4 处残余漂移 → 由 t11 收口） |
| t8 | 独立验证 t2/t3/t5/t6 + **反向把关死代码删除** | ✅ 通过，**零误删** |
| t10 | core 死代码清理 33 个符号 / 28 文件（A–D 四批，全部外科式删除） | ✅ |
| t12 | 本轮决定不修的 8 项登记为 **KD-49…KD-56** | ✅ |
| t13 | 复核 t6 改动后 t4 三份产物的**当前字节**（t7 判定已过期） | ✅ 由我接手完成（pass） |
| t15 | 收口 2 条 low 偏差：`tests/manual` 最后一处 CHECK 归一、Android 守卫去掉注释锚点依赖 | ✅ |
| **t9** | 最终集成（冲突检查 + 完整门禁 + 给用户的汇总） | ❌ 429 打断（我用自己的汇总替代，**缺独立复核**） |
| **t14** | 断链后空指针解引用（**实测 EXC_BAD_ACCESS**）+ `test_ffi_comprehensive` 约 1/7 flake 定位 | ❌ 429 打断（改动已提交、自洽，但**未经独立验证**，原定目标未完成） |
| **t16** | 测试固定临时路径导致的并发互相覆盖 | ❌ 429 打断（6 处已修 5 处，余 `test_new_modules` 的 tlog 相对路径，见 4.3） |

### 关键独立验证结论（本轮最硬的产出）

`t8`（独立验证者，不采信自述）：
- **公开 FFI 表面 `3c5bdc7→HEAD` 为 282→282、`removed=[]`** ⇒ 没有删掉任何未绑定/待接线导出
- 从提交里提取**真正消失的 40 个 core 符号 + 6 个 FFI 符号**，逐一全仓 grep 零命中
- KD-48 三处断链、`mg_command_arm` 等 JSON 分发目标、`mg_rc_get_channels` 簇**全部在**
- 亲自做两条"旧代码会失败"：还原 t2 的三个策略文件 → `test_sim_calibration` 由 143/0 变 **134/9**；按 t3 修复前语义在 TSan 下重编 → **exit=134 报 data race + SEGV**
- 用 mtime 证明自己的门禁结果对应已提交 HEAD、**未被并发 WIP 污染**

`t7`：亲自复现了 t4 的 TSan 负向证明（`mavlink_ftp.h:169` 去原子 → `sanitize.sh --tsan-only` **exit=8**、报 `data race mavlink_ftp.cc:447`、2/39 abort），并按 SHA256 确认已还原。

---

## 2. 当前工作区状态

### 2.1 提交清单（8 笔，全部本地）

```
b32270e  test: 临时路径每进程唯一消除并发碰撞；Android 守卫去掉注释锚点依赖；tests/manual 断言宏归一   (11 files)
aa5496a  test(gates): 把 testing.md/ffi-contract.md 声称的验证方式落成真脚本，补齐 S3/S5/S7/S8 缺口测试  (13 files)
5152058  docs(deviations): 登记本轮决定不修的 8 项后续为 KD-49…KD-56                        (1 file)
0e535df  build: 从 MGCORE_SOURCES 移除已删除的 firmware_flasher.cc 及其空分类注释             (1 file)
6137e90  docs: 同步文档漂移（死链、越界行号、计数与声明不符）                                  (17 files)
6e781c0  test: 新增回归测试并统一 CHECK 宏定义                                            (40 files)
27b5dfb  fix(core,ffi,ui): 修复安全与正确性缺陷并清理已核实死代码                            (85 files)
b14751f  chore(gates): TSan 进 CI、S6 守卫补盲区、Windows 门禁对齐                           (8 files)
```

**未推送**——按 `release-and-hardware.md` R3，远端操作需用户明确指示。

### 2.2 验证方式（**接手时先跑一遍确认基线**）

```bash
# 常规 + 并行
cmake --build build_check -j8
ctest --test-dir build_check -L "offline|simulation" -LE "hardware|manual" --no-tests=error        # 期望 39/39
ctest --test-dir build_check -L "offline|simulation" -LE "hardware|manual" -j4 --no-tests=error    # 期望 39/39

# TSan（本轮已接入 CI）
cmake --build build_tsan -j8 && ctest --test-dir build_tsan -L "offline|simulation" -LE "hardware|manual"

# 三个门禁脚本
python3 tools/check_ffi_contract.py        # 期望 280 declarations / 136 bindings，debt 16
python3 tools/check_firmware_literals.py   # 期望"未新增"；基线 16 文件 / 66 处
python3 tools/check_fake_assertions.py     # 期望"未发现 T2 禁止的假断言写法"

# 全量后端门禁（会重编 + 跑 ctest，含上面三个脚本）
tools/preflight.sh --backend-only
```

**本轮最后一次实测结果**：串行 39/39、并行 `-j4` 39/39、三个脚本全 exit 0、`preflight.sh --backend-only` exit 0。

### 2.3 已知的验证强度差异（**接手时须知**）

- `t9`（最终集成）与 `t13` 的复核**由我（captain）自己完成**，**不构成独立验证**。`t7`/`t8` 的独立结论**绑定的是它们跑过的那份字节**；此后 `t6` 又改过 `docs/constraints/testing.md`、`tools/preflight.sh`、`tools/preflight-windows.ps1`（这三份已由我 t13 复核过当前字节），`t14`/`t15`/`t16` 还改过若干测试文件（**这些改动没有独立验证者复核过**）。
- 因此：**若要给出"本轮全绿"的结论，必须由接手者在当前 HEAD 上重跑一遍完整门禁，并明确它对应哪个 commit。**

---

## 3. 当前任务（给下一个 agent 的手把手步骤）

**本轮的主线已收口**（工作树干净、门禁绿）。所以下一步不是"继续清理"，而是**把被 429 打断的三件事做完，并补上独立验证**。按优先级：

### 待办 #1：重开 `t14`——断链后的空指针解引用（**实测崩溃**）

这是最该先做的，因为它是**唯一一个被实测出来的崩溃**。

- **位置**（由 `t6` 在完成自身任务后上报，它因不在自己的写作用域而未改）：`get_active_conn()` → `Vehicle::conn()` → `*primary_connection()`，而 `Vehicle::disconnect()` 会把 `connections_` 整个 swap 走。
- **触发条件**：「飞机仍在 registry 里但已被 disconnect」时，下一次 `mg_connection_get_state()` 会在 `ConnectionManager::state_.load()` 上崩。`t6` 实测到 **EXC_BAD_ACCESS（地址 0x74）**。
- **为什么危险**：生产路径目前**靠调用顺序侥幸安全**（`mg_connection_disconnect` → `close_vehicle` 先摘掉飞机）。这是"顺序一变就炸"的脆弱点，不是已被消除的缺陷。
- **做法**：先独立复现（`t6` 的复现细节有限，**不要直接采信**）。能复现就走最小修复 + 一条能证明旧码会崩的测试；**若判断需要设计决策**（"断链后 `conn()` 该返回什么"）就停下问用户，不要擅自定调。
- `t14` 此前已提交的改动在 `core/include/core/vehicle/vehicle.h`（21 行）与 `core/tests/test_ffi_comprehensive.cc`（92 行）——**coherent 但未经独立验证**，接手时先看它们做到哪一步。

### 待办 #2：`t16` 余项——`test_new_modules` 的 tlog 相对路径

- `core/tests/test_new_modules.cc:478` 的 `const char* test_file = "test_replay.tlog";` 与 `:542` 的 `"test_replay_truncated.tlog"` 是**相对当前工作目录**的路径。并发跑同一个测试二进制时仍会互相覆盖。
- **注意**：这是**并发跑同一个二进制**才触发；`ctest -j4`（并行跑**不同**二进制）现在是绿的。所以优先级低于待办 #1。
- 改法：用 `mgcs_test::unique_temp_name()`（已在 `core/tests/mgcs_test_check.h` 里提供，见第 5.2 节的教训）。注意 `test_file`/`truncated_file` 是多处使用的 `const char*`，改成 `std::string` 后需连带处理 `fopen` 等需要 `const char*` 的调用点——**所以不要盲改**（我正是因为没有验证余量而没动它）。
- **同类还有一处（核心侧）**：`core/src/calibration/calibration_service.cc:776` 的 `MGCORE_TEST_BUILD` 分支用固定的 `temp_directory_path()/"M-GCS-test"/"calibration-history.v1"`。生产路径本身是**正规的每用户目录**（Application Support / `XDG_STATE_HOME` / `LOCALAPPDATA`），**生产无此问题**；只有测试构建才落到固定临时路径。若要修，必须**同时**改核心那处与 `core/tests/test_sim_calibration.cc` 里对应的路径（两者必须一致，见 5.2）。

### 待办 #3：补独立验证（把"我自证"换成"别人验我"）

- `t9`/`t13` 是我自己做的。请在当前 HEAD 上重跑完整门禁，并把结论**绑定到具体 commit**。
- 尤其要复核 `t14`/`t15`/`t16` 改过的测试文件（`test_ffi_comprehensive.cc`、`test_transport_links.cc`、`test_sim_mission_document.cc`、`test_sim_ui_paths.cc`、`test_new_modules.cc`、`tests/manual/test_sitl_mission_scenario.cc`）——**这 6 个文件没有独立验证者看过**。

### 待办 #4（用户决策项，本轮已给建议但未定案）

1. **S6 豁免清单**（16 文件 / 66 处）：本轮裁定**接受**（棘轮机制只允许收缩 + `t7` 独立抽查 8 条理由成立 + 用本机 QGC 源码核对了 4.0/4.5/4.7 改名表）。用户若要重新过一遍清单，见 `tools/firmware_literals_baseline.json`。
2. **`formatByteSize` 口径**：本轮统一为「`>=100` 取 0 位小数 + 单位到 TB」，影响 15 个调用点 / 4 文件。这是**显示偏好**而非技术优劣，用户可一句话改回另一口径（只需改 1 个函数 + 1 个测试）。
3. **S6 数值编码**：本轮**决定不写数值扫描器**（那等于重复造 MAVLink 已提供的东西），改走"换成 `MAV_AUTOPILOT_*` / `MAV_CMD_*` 官方枚举 + 模式号集中到插件表"——已核实这些枚举在 bundled 方言里确实存在（`ardupilotmega.h:206/361/364`）。登记为 **KD-50**，需单独立项。
4. **团队归档**：`mgcs-quality-hardening` 仍在 roster 上未归档（按协议，未完成的工作不应未经授权丢弃）。

---

## 4. 剩余待办全景

### 4.1 已登记为已知偏差（`docs/constraints/known-deviations.md`）

本轮新增 **KD-49…KD-56**（每条含「位置 / 现状 / 本轮不修原因 / 建议处置方向」）：

| ID | 规则 | 内容 | 状态 |
|---|---|---|---|
| KD-49 | A3/A4 | `send_cb` 在 `send_mtx` 内执行（`mavlink_adapter_internal.h:181-190`），与 `mtx→send_mtx` 锁序构成 ABBA 面。**ADR-0005 只承诺了"出 mtx"、未覆盖 send_mtx**，需先修订 ADR 才能动 | 待决定 |
| KD-50 | S6 | 守卫只拦参数名，模式号/命令编码无守卫（`ffi_flight_commands.cc:467/475`、`ffi_internal.h:242/252/273`、`ffi_calibration.cc:78/82/95/167/179`） | 待决定 |
| KD-51 | 架构 | 其余 FFI 入口仍用借用指针调阻塞方法（实测 **110 处** `get_active_mavlink(` vs **2 处**租约） | 待修复 |
| KD-52 | 架构 | `wire_callbacks` 捕获裸适配器指针，仅靠 `reset()` 里"先 close 链路再 reset 适配器"的顺序安全 | 待修复 |
| KD-53 | 功能 | PX4 陀螺校准回读只用 `INS_GYROFFS_*` → 恒失败（**既存**，本轮无新增回归） | 待修复 |
| KD-54 | 一致性 | `radio_calibration.cc` 的 `parse_statustext` 仍是子串匹配，与 gyro/baro/accel 改动后的口径不一致 | 待修复 |
| KD-55 | 功能 | 测绘相机模型**不区分横竖拍**、无 turnaround 处理（对照 QGC 行为，按 `upstream.md` P3/P4 **不得复制其源码**） | 待决定 |
| KD-56 | 合规 | `USBBoardInfo.json` 与上游同名文件**共享 51/51 条描述、52 组相同 VID/PID**，但仓库内无对应 LICENSE、ADR-0002 未登记；该文件自 `74972da`（0.0.1）起即存在，属**历史遗留** | 待决定 |

### 4.2 被 429 打断、尚未收口

- `t14` 的原始目标（断链空指针、以及 `test_ffi_comprehensive` 约 1/7 flake 的**定位**）——见待办 #1
- `t16` 的 tlog 相对路径 + 核心测试构建分支的固定路径——见待办 #2
- `t9` 的独立复核——见待办 #3

### 4.3 已知但本轮未处理（记录以免丢失）

- **`test_ffi_comprehensive` 的偶发失败**：约 7 次里 1 次报 "114 PASSED, 1 FAILED"，重跑与 6 路并发重跑全绿，**未定位到具体断言**。`t14` 被 429 打断在定位中途。已排除与 CHECK 宏归一有关（该文件断言数与语义未变）。
- **`test_shared_link_transports` 的一次 SEGFAULT**：`t15` 在并发构建干扰下看到 1 次，直跑连 3 次 11/11 全绿，未复现。
- **编辑工具会去掉被改文件首行 BOM**：本工作树中 `ffi_actuator.cc`、`mavlink_senders.cc` 等受影响。提交前值得扫一遍。

---

## 5. 踩过的坑（**务必读**）

### 5.1 基础设施故障：三种，都不是代码问题

| 故障 | 表现 | 处置 |
|---|---|---|
| `workbuddy-ai` 额度耗尽 | `hard_credit 429 Credits exhausted`，`gate-harden`/`verifier` 直接失效 | 发现**新增成员不指定路由时会继承 captain 的路由**，据此新增 `verifier-alt` 落在可用路由上 |
| `TRANSPORT` 连接错误 | `cleanup`(t10) 与 `safety-fix`(t6) **同时**被掐断 | 实测确认工作树**没有半成品**（构建 exit 0、ctest 39/39、保全 grep 全非零、批 A/C/D 删除完整一致），据此**从断点重试而非回退** |
| 每周用量上限 | `RATE_LIMITED`，重置 **2026-10-17**，最终**全部成员失效** | 我接手做完了不需要模型产能的验证类任务（t13） |

**教训**：任务**终态**才能触发依赖级联派发；中途等待不会。所以"卡住"时要先看任务是否处于终态。

### 5.2 我犯的契约错误：4 次同类，根因都是"单向编辑 inScope"

| 次 | 任务 | 错误 | 后果 |
|---|---|---|---|
| 1 | t3 | `inScope` 没算"改访问器签名"的**传递影响面**（`get_mavlink()` 波及 40+ 调用点，其中 `test_sim_calibration.cc` 还是别人的写作用域） | 实现者被迫越界改文件 → **构建全红**，阻塞了 t4 |
| 2 | t5 | 验收条款要求的测试落点（`test_sensor_decode_offline.cc`、`test_telemetry_ring_buffer.cc`）**不在 `inScope`** | 实现完成却被 scope 校验拒绝提交 |
| 3 | t5 | 补 `inScope` 时**忘了 `outOfScope` 里还留着两条目录级条目**把文件级白名单盖掉 | 同样提交被拒 |
| 4 | t10 | `latest_command_ack` 的**定义文件**不在 `inScope`（只列了声明所在头文件） | 实现者提前预警，我补上 |

**修正动作（写进流程）**：**每次 amend 必须同时复查 `inScope` 与 `outOfScope` 两个方向的包含关系**，并且要按"改动的传递影响面"划范围，而不是按"预计要改的文件"划。

### 5.3 我犯的一次**真实回归**（比契约错误更该记住）

- 我把 `test_sim_calibration.cc` 的临时路径改成"每进程唯一"，**结果该用例单独跑就红**。
- 查明真相：那条路径**必须与核心在 `MGCORE_TEST_BUILD` 下的 `history_storage_path()` 一致**——测试读的是核心实际写入的那份文件。改动让两者分叉。
- 已回退，并**在原处加注说明**为什么不能改。
- **根因是我在上下文将尽、无法边改边验时动的手**。教训：**没有验证余量的改动，风险大于收益；宁可不做，也不要盲改。**

### 5.4 并发构建造成的**伪失败**（会反复咬人）

- 多个成员同时在同一目录构建会抢 object 文件、relink 时序错乱 → 报出与源码无关的失败。
- 实例：`t5` 报 TSan `92% / 3 tests failed`（随后两次干净复跑全绿）；`t9` 的门禁与 `t15` 的验证重叠 → 3 套 Failed（单独直跑却 349/349、67/67、145/145 全绿）；`t15` 还看到一次 `test_shared_link_transports` SEGFAULT（直跑 3 次全绿）。
- **判据（已写进 t9 的纪律）**：**单次通过不足以判"通过"，单次失败也不足以判"不通过"**；跑之前先 `ps -eo command | grep -E 'cmake --build|ctest'` 确认无人构建。
- **已交付的缓解**：`t16` 把测试的固定临时路径改成每进程唯一（`mgcs_test::unique_temp_name()`），并实测 `ctest -j4` 由会红变 39/39。

### 5.5 两次"结论过期"

- `t7` 验过 `docs/constraints/testing.md`、`tools/preflight.sh`、`tools/preflight-windows.ps1`，但 `t6` **随后改了同样三份** → 框架判定 `t7` 的通过"不绑定稳定 revision"。我另开 `t13` 复核当前字节（已 pass）。
- `t8` 的判定同样因工作区在其判定期间变动而失效，需重验。
- **教训**：**验证结论必须绑定到具体字节/commit**；后续改动覆盖了被验文件，就必须重验或在报告里明确标注覆盖关系。

### 5.6 值得保留的好做法（下一轮请继续用）

- **"旧代码会失败"要用隔离副本做**：`t2` 用 `/tmp/mgcs-oldcheck`（rsync 工作树 + `git show HEAD:<文件>` 还原旧实现、保留新测试），`t3` 用 `/tmp/t3red_old`。可比对、可复算，且不干扰共享工作区。
- **等价替换要同时证明"没改行为"和"修了真缺陷"**：`t5` 把 JSON 扫描器换成 cJSON 时，让**同一计数断言在 HEAD 旧解析器上也 PASS**（证明条目数没变），再用"嵌套对象/字符串含 `}`"的合成用例让**旧实现 4 FAIL**。
- **反向把关要可证伪**：`t8` 用"公开 FFI 表面 282→282、`removed=[]`"+ 逐个符号全仓 grep，而不是"读了一遍觉得没问题"。
- **临时改动要 SHA256 自证已还原**：`t4`/`t7` 对 `mavlink_ftp.h` 的去原子实验都这么做了。
- **成员主动自曝干扰源**：`concurrency-fix` 交代了自己的压载测试与门禁窗口重叠；`cleanup` 自曝了全局空行折叠误压两个文件的既有空行段并已按 HEAD 逐段恢复。**这类自曝让结论可复现。**

---

## 6. 关键文件速查

| 文件 | 作用 |
|---|---|
| `AGENTS.md` | 协作入口，各类任务必读的约束清单 |
| `docs/constraints/*.md` | 约束细则（architecture/ffi-contract/safety/testing/ui/upstream/parity） |
| `docs/constraints/known-deviations.md` | 偏差台账，本轮新增 KD-49…KD-56 |
| `docs/workflow/HANDOFF-2026-10-12.md` | 同日"大清洗"轮交接（其 43 项改动已由本轮提交固化） |
| **`core/include/core/firmware/modes/`** | ArduPilot 模式表（**唯一事实来源**）——改模式相关行为只看这里 |
| `core/tests/mgcs_test_check.h` | 全仓唯一的断言宏 `CHECK(条件, 消息)` + `mgcs_test::unique_temp_name()` |
| `tools/check_fake_assertions.py` | T2「禁止假断言」守卫（本轮新增，含 `--self-test`） |
| `tools/check_api_change_set.py` | F1「成套变更」守卫（本轮新增，含 `--self-test`） |
| `tools/check_firmware_literals.py` + `tools/firmware_literals_baseline.json` | S6 守卫与豁免清单（16 文件 / 66 处，只允许收缩） |
| `tools/preflight.sh` / `tools/preflight-windows.ps1` | 全量门禁；CI 委托给同一份脚本（KD-32） |
| `tools/sanitize.sh` | ASan/TSan 复测（本轮已接入 CI 的 `tsan` job） |
| `bridge/ffi/API_CATALOG.md` | FFI 接口目录（280 导出） |

---

## 7. 给下一个 agent 的第一条指令

> 当前工作区**干净**（8 个本地提交，0 项未提交，**未推送**）。请先跑一遍第 2.2 节的验证命令确认基线（期望：串行 39/39、`-j4` 39/39、三个脚本 exit 0）。
>
> 然后从**第 3 节「待办 #1：重开 t14」**开始——那是本轮唯一一个**被实测出来的崩溃**（`EXC_BAD_ACCESS`，地址 0x74），也是风险最高的一项。先独立复现；若判断需要设计决策就停下来问用户，别擅自定调。
>
> **动手前**：读第 5 节，特别是 5.2（我的 4 次契约错误）、5.3（我引入的一次真实回归）和 5.4（并发构建的伪失败判据）。它们能直接省掉重复劳动和误判。
>
> **动手时**：一次只做一个，做完验证，给用户看。做到"能证明旧代码会失败"才算完成——**单次通过不足以判通过，单次失败也不足以判不通过**。遇到"删还是留"的判断，停下来问，别擅自定。
