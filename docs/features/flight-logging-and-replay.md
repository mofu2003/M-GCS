# 功能简报：飞行日志与回放

状态：S1 可用闭环、S2 录制/回放、P1（`mg_fs_*` FTP 异步化）与 S3（FTP 文件浏览页 + 本地索引/缓存）均已落地（native 修 P2/P3/P4/P5/P7/P6 + 擦除 `mg_onboard_log_erase` + 下载完整性 `size_declared` + tlog 录制接线 + `mg_tlog_replay_*` 导出 + FTP 异步状态机；Flutter「机载日志」「飞行记录」「机载文件」三页均已接入）。本方向收口，剩下 `.bin` 解析/图表与断点续传两个独立议题（见第 10 节）
日期：2026-10-11
来源：`docs/plans/mgcs-development-plan-2026-10-10.md` 阶段 3 第 1 条；`docs/capabilities.md` 对 `ffi_api_logging.h` 的「待接入」说明。

本文件是阶段 3「能力扩展，逐项立项」的第一个方向的需求与上游研究记录。它只定义要做什么、参考了上游的什么行为、验收标准和平台范围，不在此实现。实现前先按本文件确认范围。

## 1. 目的

把 `ffi_api_logging.h` 的 25 个公开导出（机载日志列表/下载/记录、本地会话日志、MAVLink FTP 文件访问）从「已实现但无入口」变成一条可信的用户链路：**列出机载日志 → 选择下载 → 看到进度 → 可取消 → 落盘且能校验完整性 → 断链有确定结果**，并提供离线回放/基本消息检索的入口。目标不是完整的日志分析器（图表、MAVExplorer 级检索），那是后续方向。

## 2. 范围

- 范围内：机载 DataFlash 日志列表、下载、取消、进度、完整性校验、落盘失败可见；本地 `.tlog` 录制；离线打开本地日志并做基本消息检索；三套日志 API 的命名与职责统一。
- 范围外：DataFlash `.bin` 解析与图表（QGC `LogViewer` / MP `LogBrowse` 级别）、`.tlog` 作为「回放链路」注入实时管线、日志自动上传到外部服务、日志擦除批量 UI（可作后续）、LogAnalyzer 式自动诊断。

## 3. M-GCS 现状

25 个导出实际分成五组，职责有重叠，命名容易混淆：

| 组 | 导出 | 实现 | 实际语义 |
|---|---|---|---|
| 机载 DataFlash | `mg_onboard_log_request_list`、`mg_onboard_log_get_entries`、`mg_onboard_log_download`、`mg_onboard_log_download_cancel`、`mg_onboard_log_download_status`、`mg_onboard_log_erase` | `bridge/ffi/ffi_log_commands.cc`、`core/src/mavlink/mavlink_senders.cc:992-1105`、`core/src/mavlink/mavlink_adapter_log.cc` | `LOG_REQUEST_LIST`/`LOG_ENTRY` 列表；`LOG_REQUEST_DATA` 异步下载到文件 |
| tlog 录制 | `mg_tlog_start/stop/status` | `bridge/ffi/ffi_log_commands.cc`、`core/src/settings/mavlink_log_writer.cc` | 把链路上的 MAVLink 帧写成 `.tlog`；写入点挂在 `MavlinkAdapter` 的帧投递路径（ADR-0006） |
| 会话文件日志 | `mg_session_log_start/stop/status` | `bridge/ffi/ffi_telemetry.cc:229-262`、`core/src/settings/flight_log_writer.cc` | 把事件写成 JSONL/CSV 文本 |
| 事件日志服务（导出已删除） | — | `core/src/settings/flight_logging_service.cc` | STATUSTEXT/连接/命令结果的结构化事件流 |
| MAVLink FTP | `mg_fs_list_directory`、`mg_fs_download_file`、`mg_fs_upload_file`、`mg_fs_transfer_status`、`mg_fs_cancel` | `bridge/ffi/ffi_log_commands.cc`、`core/src/ftp/mavlink_ftp.cc` | 通用机载文件系统浏览/收发；异步状态机（起线程 → 轮询 `mg_fs_transfer_status` → 取消），已接 FFI 与「机载文件」页（S3） |

`core/include/core/log/flight_log_replayer.h` 的 `FlightLogReplayer`（`.tlog` 按时序回放）曾只在 `core/tests/test_new_modules.cc` 被测试；S2 已将其导出为 `mg_tlog_replay_open/step/seek/close` 并接入 Flutter「飞行记录」页，不再是内部能力。

### 已发现的真实问题（P1/P2/P3/P4/P5/P6/P7 本轮已修）

- **P1 同步阻塞的 FTP 导出（已修）。** 原状：`mg_fs_list_directory`/`mg_fs_download_file`/`mg_fs_upload_file` 在 FFI 调用线程上直接跑完整传输（`MavlinkFtpClient::download_file` 在条件变量上循环到传输结束才返回），大文件会阻塞整个调用线程，传输中读不到进度、也无法可靠取消，违反 `docs/constraints/architecture.md` A3 在 FFI 层面的等价要求。现改为异步：`MavlinkFtpClient::start_list_directory/start_download_file/start_upload_file` 把阻塞传输放到后台线程，`transfer_status()`/`transfer_active()`/`wait_for_completion()`/`cancel()`/`shutdown()` 提供状态、等待与取消；FFI 侧 `mg_fs_*` 变为「发起 → 轮询 `mg_fs_transfer_status` → `mg_fs_cancel`」，与 `mg_onboard_log_*` 的异步模式一致。
- **P2 完成状态早于落盘，写失败被吞。** `handle_log_data` 在收到满足完成条件的帧时，把文件写入排进 `deferred_cbs` 后**立即**置 `download_completed = true`（`core/src/mavlink/mavlink_adapter_log.cc:62-82`），随后 `mg_onboard_log_download_status` 就报告 `state="completed"`。真正的 `fopen`/`fwrite` 在延迟回调里执行，失败只写日志（`:70-79`），既不回退完成状态，也不通过 `status.error` 或 JSON 暴露。磁盘满、路径无权限、写入短写时用户会看到「下载完成」但文件不完整。计划要求的「文件完整性」目前不成立。
- **P3 三套日志 API 命名重叠（已随未绑定导出清理解决）。** `mg_event_log_start`（事件服务）、`mg_tlog_start`（tlog 录制）、`mg_session_log_start`（JSONL/CSV 会话文件）从名字上无法区分，正是需要在阶段 3 一并统一的「新旧命名/重复概念」。三者在 core 是三个独立类（`FlightLoggingService`、`MavlinkLogWriter`、`FlightLogWriter`）。**后续处置**：事件日志与会话文件日志的 FFI 导出（`mg_event_log_*`、`mg_session_log_*`）在 2026-10 的未绑定导出清理中删除，三个类保留但只有 `mg_tlog_*`（tlog 录制）与 `mg_onboard_log_*`（机载日志）有导出，命名重叠不再存在。
- **P4 状态查询带副作用。** `MavlinkAdapter::log_download_status()`（`core/src/mavlink/mavlink_senders.cc:1069-1105`）在下载中且 500 ms 未收到数据时会自动补发 `LOG_REQUEST_DATA`。一个「查状态」的导出因此会改变链路行为，且每 500 ms 只补一个 90 字节块，大范围丢包恢复很慢。补包应移出状态查询路径，改为下载 state 自己的定时逻辑。
- **P5 计数用 `int` 截断。** `mg_onboard_log_get_entries` 的 `size_bytes`、`mg_onboard_log_download_status` 的 `received_bytes`/`total_bytes`、`mg_tlog_status` 的 `bytes` 都以 `static_cast<int>` 进入 JSON（`ffi_log_commands.cc:46,96-97,126`）。DataFlash/`.tlog` 超过 2 GiB 时显示为负数。应收敛为 64 位并统一单位。
- **P6 缺周边能力（本轮已补日志擦除、下载完整性与本地索引/缓存）。** 原状：没有日志擦除（QGC `eraseAll`/`eraseSelected`、MP `BUT_clearlogs`）、没有本地日志索引/缓存（MP `Log/LogIndex.cs`）、下载后没有大小/校验核对，重复进入页面会每次重新向飞控拉列表。
- **P7 一致性小问题。** `mg_onboard_log_get_entries`（`ffi_log_commands.cc:30`）没有 `CHECK_CONNECTED`，与同组其它导出不一致；`mg_onboard_log_download` 依赖先调用 `mg_onboard_log_get_entries` 才知道 `download_size`，否则完成判定退化为等待 `LOG_DATA` 的空帧。

#### 本轮进展（P1，2026-10-11）

- **FTP 异步状态机**：`MavlinkFtpClient` 新增 `FtpTask`/`FtpTransferStatus` 与 `start_list_directory`/`start_download_file`/`start_upload_file`/`transfer_status`/`transfer_active`/`wait_for_completion`/`cancel`/`shutdown`。传输跑在后台线程，状态由 `status_mtx_` 保护，进度用原子量高频更新，状态查询不抢传输锁；同一时刻只允许一个传输（协议单会话），第二次发起返回 false。
- **取消语义**：`cancel()` 同时置 `cancelled_` 与 `cancel_requested_`——前者供正在跑的循环读取，后者覆盖「刚 start 但传输体还没进入循环」的窗口，由传输体开头 `exchange(false)` 接力，避免刚点取消就丢请求。取消以 `cancelled=true` 汇报，和超时/被拒的 `failed` 区分开。
- **适配器与生命周期**：`MavlinkAdapter` 改为 `file_list_start`/`file_download_start`/`file_upload_start`/`file_transfer_status`/`file_transfer_cancel`/`file_transfer_wait`；`Impl::~Impl()` 先 `ftp.shutdown()`（cancel + join），保证后台线程不会碰到已析构的发送成员。
- **FFI**：`mg_fs_list_directory`/`mg_fs_download_file`/`mg_fs_upload_file` 立即返回（失败返回 `MG_ERROR_BUSY`）、`mg_fs_transfer_status` 轮询（`task`/`state`/`active`/`completed`/`cancelled`/`received_bytes`/`total_bytes`/`error_code`/`message`，list 任务带 `items` 数组），`mg_fs_cancel` 取消（无活动传输返回 `MG_ERROR_NOT_SUPPORTED`）；删除旧的 `mg_fs_download_cancel`。`transfer_status` 故意不做 `CHECK_CONNECTED`，断链后仍能看到最终结果。
- **测试**：`core/tests/test_new_modules.cc` 新增异步下载/列表/上传、单会话拒绝、取消中断与取消后复用（124 通过）；`core/tests/test_sim_ui_paths.cc` 新增 `mg_fs_*` 端到端（发起 → 轮询 completed → 落盘、NAK `FILE_NOT_FOUND` → failed/error_code 10、无响应时 `mg_fs_cancel` → cancelled），MockFlightController 补 `ListDirectory` 支持。

#### 本轮进展（S2，2026-10-11）

- **tlog 磁盘格式契约统一**：新增 `core/include/core/log/tlog_format.h`，明确每条记录为 `[8 字节大端 Unix 微秒][完整 MAVLink 帧]`，无文件头；写入端（`mavlink_log_writer`）与读取端（`flight_log_replayer`）共用该契约，与 QGC/Mission Planner 的 `.tlog` 布局一致。
- **录制接线（ADR-0006）**：`MavlinkAdapter` 新增 `set_log_sink()` 与 `record_frame()`（`mavlink_adapter.cc`），在帧投递路径记录收发帧，`Vehicle::wire_telemetry_callbacks_for` 幂等挂载；此前 `MavlinkLogWriter` 除单元测试外**没有调用点**，录制实际从未生效。签名帧按 QGC 行为剥签名并重算 CRC，跳过 `SETUP_SIGNING`。见 `docs/adr/0006-tlog-recording-sink.md`。
- **回放导出**：`mg_tlog_replay_open/step/seek/close` 四个导出（`bridge/ffi/ffi_api_logging.h`、`ffi_log_commands.cc`）。`open` 全扫描统计消息数与时间范围并回报 `truncated`；`step` 默认 200、上限 2000 条，回报 `messages/count/progress/last_timestamp_us/eof/truncated`；`seek` 按文件比例跳转，越界返回 `NOT_FOUND`；`close` 释放句柄。消息名经 `mavlink_message_names.{h,cc}`（`mavlink_get_message_info`）解析，未知消息为 `UNKNOWN`。
- **Flutter「飞行记录」页**：`flutter_shell/lib/logging/tlog_models.dart`、`flutter_shell/lib/logging/tlog_backend.dart`、`flutter_shell/lib/logging/tlog_ffi_backend.dart`、`flutter_shell/lib/logging/tlog_controller.dart`、`flutter_shell/lib/logging/flight_log_page.dart`；入口在设置菜单「飞行记录」→ 配置工作区 `flightLog` 页。页面支持链路录制开关（未连接拒发、1 s 轮询已写字节）、默认目录 `.tlog` 文件列表、手动输入路径、打开/暂停式分步回放、进度条与按比例跳转（拖动防抖）、消息过滤（消息名子串或纯数字 msgid）、最近 5000 条保留上限、文件尾部截断提示。
- **头文件命名/契约**：`FlightLogReplayer` 的 `truncated_` 语义修正（打开扫描保留截断结果、成功 seek 清零），删除无用的 `bad_records_`。
- **seek 语义修正（真实缺陷）**：旧实现从目标字节起做裸字节扫描找下一个 `MAVLINK_FRAMING_OK`，会把 8 字节时间戳前缀喂进解析器，落点随录制时间戳字节漂移——3 条消息的固定文件在 30 次跑里错 2 次。现改为按记录边界前进：定位到起点偏移不小于目标的第一条完整记录并挂起，seek 后第一次 step 返回落点记录本身；percent == 1.0 无记录可读，返回 `NOT_FOUND`。`core/tests/test_ffi_contract.cc` 第 12 组新增落点顺序与中部跳转断言。
- 测试：`core/tests/test_new_modules.cc::test_tlog_recording_wiring`、`test_ffi_contract.cc` 第 12/13 组（回放正常路径 + 截断文件）；`flutter_shell/test/tlog_controller_test.dart` 覆盖未连接拒发、start/stop 轮询、空路径/打开失败、step 批量与 eof、过滤、seek、截断、close、保留上限丢弃、dispose 释放句柄。

#### 本轮进展（S1，2026-10-11）

- **已修 P2**：落盘改为「先写文件、成功后再置完成」。`LogState` 新增 `download_write_pending`/`download_saved_bytes`；写盘错误（打不开、短写、flush 失败）写入 `download_error`，`mg_onboard_log_download_status` 暴露 `saving`/`saved_bytes`/`error`，不再出现「已完成但文件不存在/不完整」。
- **已修 P3**：四组导出统一为按对象命名，删除旧名、不加兼容别名——`mg_onboard_log_*`、`mg_tlog_*`、`mg_session_log_*`、`mg_event_*`；`mg_fs_*` 保留为通用 FTP。头文件、实现、Dart 绑定、目录与文档一次改齐。
- **已修 P4**：`log_download_status` 改为只读；丢包补发移到 `process_timeouts()` 的下载定时逻辑（500 ms 失速后从文件头补第一个缺口）。
- **已修 P5**：`size_bytes`/`received_bytes`/`total_bytes`/`saved_bytes`/`bytes` 等改为 64 位进入 JSON。
- **已修 P7**：`mg_onboard_log_get_entries` 补 `CHECK_MAVLINK` + `CHECK_CONNECTED`。
- **已修 P1（本轮完成）**：`mg_fs_*` 改为异步状态机（见上「本轮进展（P1）」），`MavlinkAdapter`、FFI、测试与文档一次改齐。
- **已修 P6（本轮完成）**：新增 `mg_onboard_log_erase`（`bridge/ffi/ffi_api_logging.h`、`ffi_log_commands.cc`）——`CHECK_CONNECTED`，下载进行中拒绝并返回 `MG_ERROR_NOT_SUPPORTED`，否则发 `LOG_ERASE`；Flutter「机载日志」页新增「擦除全部」（确认弹窗，完成后清空列表并提示）。下载完整性：`LogDownloadStatus` 增 `size_declared`，`LOG_ENTRY` 声明大小后若提前收到空 `LOG_DATA` 不再挂死也不再谎报完成，改为 `state="failed"` + `log truncated: received X of Y bytes` 且不落盘；未声明大小则以空帧正常收尾但标注「无法校验完整性」。本地日志索引/缓存已随 S3 落地（见下「本轮进展（S3）」）。
- **已接入 Flutter 日志页**：`flutter_shell/lib/logging/log_backend.dart`、`flutter_shell/lib/logging/log_ffi_backend.dart`、`flutter_shell/lib/logging/log_models.dart`、`flutter_shell/lib/logging/log_controller.dart`、`flutter_shell/lib/logging/onboard_log_page.dart`；入口在设置菜单「机载日志」→ 配置工作区 `logs` 页。页面支持刷新列表、逐条下载、进度条、取消、失败/断链提示、完成显示落盘路径；列表用 `expected`/`complete` 收齐（无 `num_logs` 时按条目数稳定兜底），按编号倒序；下载结果绑定发起会话，切换飞机丢弃旧状态。`mg_onboard_log_*` 六个导出已进 Dart 绑定（含 `mg_onboard_log_erase`），`docs/capabilities.md` 该模块状态更新为「部分接入」。
- **列表收齐判定**：`handle_log_entry` 按 id 去重并记录 `LOG_ENTRY.num_logs`，`mg_onboard_log_get_entries` 输出新增 `expected`/`complete`。
- 测试：`core/tests/test_sim_ui_paths.cc` 日志用例覆盖写盘后置完成 + `saved_bytes`、写失败可见、状态查询无副作用、取消发 `LOG_REQUEST_END`、按 `num_logs` 收齐/去重；`flutter_shell/test/log_controller_test.dart` 覆盖收齐排序、断链、下载成功、取消、写失败、未连接拒发、会话切换丢弃旧结果。


#### 本轮进展（S3，2026-10-11）

- **「机载文件」页**：`flutter_shell/lib/logging/file_backend.dart`、`flutter_shell/lib/logging/file_ffi_backend.dart`、`flutter_shell/lib/logging/file_models.dart`、`flutter_shell/lib/logging/file_index.dart`、`flutter_shell/lib/logging/file_controller.dart`、`flutter_shell/lib/logging/file_browser_page.dart`；入口在设置菜单「机载文件」→ 配置工作区 `files` 页（`configuration/configuration_workspace.dart`、`widgets/mg_settings_menu.dart`、`main.dart`）。页面支持路径行与常用目录跳转（`@SYS`/`/APM/LOGS`/`/`）、刷新/上一级、目录进入、单文件下载（进度条 + 取消）、本地上传到当前目录、失败/断链提示。结果只按 `mg_fs_transfer_status` 呈现，页面不推测完成。
- **异步状态机**：`FileController` 复用 native 的单会话模型——已有传输在跑时第二次发起直接拒绝并提示；每次传输属于发起时的会话（`bindSession`），切换飞机丢弃进行中的传输，迟到的旧状态不写回；状态轮询带 idle 去抖与超时（列表 8 s）。
- **本地索引/缓存**：`FileIndexStore` 把「上次成功列出的目录」与「已下载的远程→本地映射」写到应用支持目录 `files/file_index.json`（`schema_version: 1`，损坏时按空索引恢复）。断链或列表失败时页面回填缓存内容并明确标注「可能已过期」；条目上标出本地副本路径，副本大小与机载声明不一致时显式提示而不是谎报完整。索引写入失败只提示，不阻断传输。
- **共享提示组件**：把机载日志页与飞行记录页各自的私有 `_Notice`/`_DismissButton` 提取为 `flutter_shell/lib/widgets/mg_inline_notice.dart`（`ui.md` U5），三个页面共用。
- 测试：`flutter_shell/test/file_controller_test.dart`（20 用例：列表成功/失败/取消/超时、缓存回填、下载完成与落盘索引、下载失败、上传、busy 拒绝、取消、断链拒绝、目录不可下载、会话切换、索引往返与损坏恢复、状态/进度解析）、`flutter_shell/test/file_browser_page_test.dart`（渲染条目、缓存提示、传输卡片与取消按钮）、`configuration_workspace_test.dart`（十二个路由）、`mg_settings_menu_test.dart`（「机载文件」入口）、`architecture_ffi_boundary_test.dart`（`file_ffi_backend.dart` 进 FFI 白名单）。Flutter 全量 184 通过 / 4 跳过。

## 4. 上游行为研究

参考版本见 `docs/constraints/parity.md` 顶部（QGC `bf0096d97`、MP `a2fcd74d6`）。只记录行为事实，不复制源码。

### QGroundControl

- 机载日志列表与下载由 `src/AnalyzeView/OnboardLogs/OnboardLogController.{h,cc}`（1347 行）统一处理，UI 是 `OnboardLogPage.qml`。列表走 `_requestLogList`（`OnboardLogController.cc:526,821`）。
- **两种传输**：MAVLink 日志消息（`LOG_REQUEST_LIST`/`LOG_ENTRY`/`LOG_REQUEST_DATA`，`:204,272`）与 **MAVLink FTP**（`:55,943`），运行时按能力选择，FTP 失败会回退到消息传输（`:1151-1173`）。QGC 因此能列出并下载 ArduPilot 的机载 dataflash。
- 支持**全选/多选下载**、**按日期排序**、**压缩**（`compressLogs`、`compressionProgress`）、**取消**（`cancel()`），以及**擦除全部/擦除选中**（`eraseAll`/`eraseSelected`，`:761`「选择擦除需要 FTP 传输」）。UI 有 `requestingList`/`downloadingLogs` 状态属性。
- 离线分析是独立模块 `src/AnalyzeView/LogViewer/`：`LogViewerController.h:44-46` 提供 `openTLog`/`openBinLog`/`openULogFile`，`LogFileParser.cc`（568 行）+ `APMDataFlash/APMDataFlashLogParser.cc`（822 行）解析，QML 画图与字段面板。
- `.tlog` 回放是**链路**而非离线视图：`src/Comms/LogReplayLink.{h,cc}`（534 行）把 tlog 当 LinkInterface 注入，支持播放/暂停/速度/进度（`LogReplayLink.h:48,103,115`）。
- `src/Vehicle/Vehicle.cc:726-730` 把 `LOG_ENTRY` 转成 `logEntry(...)` 信号；`src/Vehicle/MAVLinkLogManager.{h,cc}` 处理机上日志的启动/停止与上传到外部服务（本方向不涉及）。

### Mission Planner

- 机载下载有两条实现：`Log/LogDownload.cs`（662 行，`public partial class LogDownload : Form`）与 `Log/LogDownloadMavLink.cs`（602 行）。后者用 `MainV2.comPort.GetLogList()` 列日志（`:80`），`GetLog(no, fileName)` 取单条（`:196`），`DownloadThread` 异步逐条下载（`:324,347`），带进度回调（`ComPort_Progress`，`:292`）。
- 离线分析是 `Log/LogBrowse.cs`（3919 行），打开 `*.log;*.bin`（`:273`），用 `dataflashlog.xml` 定义消息（`:835`）——这是 ArduPilot dataflash 分析最深的一条链路。
- 本地 `.tlog` 由 `Log/MavlinkLog.cs`（1519 行，`tlogfilemask = "Telemetry Log|*.tlog;*.tlog.*"`，`:32`）读写，并配套 `Log/LogIndex.cs`（446 行）在本地扫描 `*.tlog` 建索引（`:62,131`），以及 `Log/MavlinkLogBase.cs`。
- 实时链路按连接落盘 `.tlog` 与 `.rlog`（`MainV2.cs:1592-1630`）。

## 5. 差距与决策点

| 维度 | QGC | MP | M-GCS 现状 | 结论 |
|---|---|---|---|---|
| 列表 | LOG_* 或 FTP，运行时选择 | LOG_REQUEST_LIST（`MAVLinkInterface.GetLogList`） | 只有 `LOG_REQUEST_LIST` | 有差距，可先做 LOG_* 单路径，FTP 列表作为后续 |
| 下载 | 多选 + 进度 + 取消 + 压缩 | 逐条异步 + 进度 | 单条异步 + 进度 + 取消 | 补多选/队列与压缩可选 |
| 擦除 | eraseAll/eraseSelected | 有清除按钮 | 已实现（`mg_onboard_log_erase` + 页面「擦除全部」确认） | 已补（S1 收尾） |
| 完整性 | 按大小核对，压缩包可校验 | 下载后转 `.bin`/建 KML | 已实现：落盘成功才置完成 + `size_declared` 标注，截断显式报错 | 已补（S1 收尾） |
| 传输阻塞 | 全异步 | 异步线程 + 进度 | 日志与 FTP 均为异步状态机（P1 已修） | 已补：`mg_fs_*` 发起/轮询/取消 |
| 离线打开 | LogViewer（bin/tlog/ulog） | LogBrowse（bin）+ MavlinkLog（tlog） | 只有内部 `FlightLogReplayer`（tlog） | S2 做 tlog 开放与消息检索，bin 解析留后续 |
| 本地索引 | 无独立索引 | `LogIndex` | 已实现（S3）：目录缓存 + 远程→本地副本映射，落 `files/file_index.json` | 已补（S3）；不覆盖 MP 的日志消息索引 |

需要先定的三个决策：

1. **命名统一（P3）。** 按对象命名的前缀：`mg_onboard_log_*`（机载 DataFlash：list/entries/download/download_cancel/download_status/erase）、`mg_tlog_*`（录制）、`mg_session_log_*`（事件/会话文件）。这样 `mg_log_*` 不再同时表示三件事。**本轮已按「只保留一套当前接口、不承诺兼容旧版」执行：删除旧名、不加别名。**
2. **FTP 异步化（P1，已做）与 `mg_onboard_log_download` 的完成语义（P2，已做）** 属于同一类：所有长传输都必须是「发起 → 轮询 status → 可取消 → status 携带最终结果」。`mg_fs_transfer_status` 现在返回 `received_bytes`/`total_bytes`/`error_code`/`message` 与 list 的 `items`；机载日志的落盘结果并入 `saved_bytes`/`error`，完成只在写成功后置位。
3. **FTP 在 Flutter 侧的调用位置（已解除）。** native 异步化完成后（P1），`mg_fs_*` 的发起调用立即返回，前端在 UI 线程轮询 `mg_fs_transfer_status` 即可，不再需要 isolate 规避。

## 6. 交付切片

- **S1 机载日志可用闭环（已完成，含 P1）**（进展：native 修复 P1/P2/P3/P4/P5/P7 + P6 擦除/完整性、Flutter「机载日志」页均已落地，见上「本轮进展」）：修 P2/P4/P5；`mg_onboard_log_list/entries/download/download_cancel/download_status` 接入 Flutter「日志」页；列表、勾选下载、进度、取消、失败/断链、落盘校验；测试覆盖正常、取消、断链、写失败、大文件。
- **S2 本地录制与离线打开**（已完成，见上「本轮进展（S2）」）：`mg_tlog_start/stop/status` 已接 UI；`FlightLogReplayer` 已导出为 `mg_tlog_replay_open/step/seek/close`，UI 提供「打开本地日志 + 消息检索」。
- **S3 扩展（已完成，见上「本轮进展（S3）」）**：MAVLink FTP 文件浏览页（「机载文件」页）、本地索引/缓存、下载后压缩（压缩本身未做，见第 10 节）。DataFlash `.bin` 解析/图表作为独立方向另立简报。

## 7. 验收标准

S1 完成需同时满足（沿用阶段 2 的对齐标准）：

- 列表失败、下载被拒、无 `LOG_ENTRY`、下载超时、链路断开、取消后仍收到 `LOG_DATA`、磁盘写失败、目标文件已存在，均返回确定状态，不出现「已下载完成但文件不存在/不完整」。
- 落盘的字节数与飞控 `LOG_ENTRY.size` 一致，或明确报告截断与原因。**（已满足：`download_status.size_declared` + 截断 `state="failed"`）**
- 取消只发送 `LOG_REQUEST_END` 并等待/记录结束语义；取消后不把残留帧计入新一次下载。
- 下载中切换飞机焦点/重连，旧下载结果不得写进新会话的 status。
- FFI 目录、Dart 绑定、编译导出与 `check_ffi_contract.py` 一致；`docs/capabilities.md` 的 `ffi_api_logging.h` 已从「待接入」更新为「部分接入」。
- **P1 附加**：`mg_fs_*` 的发起调用不得阻塞调用线程；`mg_fs_transfer_status` 在传输中/完成后/取消后/断链后都返回确定状态；同一时刻只有一个 FTP 传输，第二次发起返回 `MG_ERROR_BUSY`。**（已满足）**
- **S3 附加（已满足）**：文件浏览页在断链、列表失败、传输失败时都给出确定状态，不出现「显示为已完成但没有文件」；缓存内容必须标注来源与时间；同一会话只允许一个 FTP 传输；切换会话后旧结果不写入新会话。

## 8. 平台

- 本方向是协议与文件逻辑，macOS/Linux/Windows/Android 共用同一 native 实现；差异只在文件路径选择与磁盘错误提示。
- 证据等级按 `docs/plans` 阶段 4 的定义：至少 `build-tested` + `runtime-smoke-tested`（桌面），SITL 上跑一次真实 `LOG_REQUEST_LIST`/下载（ArduPilot SITL 提供机载日志）作为仿真证据。

## 9. 许可与外部依赖

- 只参考 QGC/MP 行为与协议事实，不复制源码，M-GCS 保持 MIT（`docs/adr/0002-upstream-reference-and-license.md`）。QGC `LogViewer` 的 DataFlash 解析若将来复用，须先核对其许可与本 ADR 的补充记录。
- 无新增运行时依赖：DataFlash 下载用现有 MAVLink，`.tlog` 读写已在 core，FTP 已在 core。

## 10. 未决问题

- 机载日志下载是否需要「断点续传」？（QGC/MP 都没有真正的续传，只有重下；M-GCS 是否要更严，需产品决定。）
- 机载日志下载是否需要「下载后压缩」（QGC 有、MP 无）？（S3 未做；当前保留原始 `.bin`。）
- 是否保留 FTP 列表作为 QGC 式回退？（当前只有 LOG_* 单路径；文件浏览页走的是 FTP 目录列表，与 LOG_* 列表是两条独立路径。）
- `.tlog` 回放是否只做离线视图，还是也做 QGC 式「回放链路」注入实时管线？后者改动面大，倾向仅离线。
- P3 改名已完成（头文件/实现/测试/目录/文档一次改齐，旧名删除、不留别名）。
