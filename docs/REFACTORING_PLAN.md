# M-GCS 分阶段重构计划

> 状态：执行中的工程计划。版本号是里程碑建议，不自动等同于发布承诺。
> 所有重构先保持公开 FFI、JSON 契约和用户可见行为不变，再逐步优化内部实现。

## 目标

当前代码已经能够覆盖任务、参数、校准、连接和平台主路径，但核心文件持续增长，
新旧任务入口并存，部分模块职责混合。重构目标不是换框架或重写产品，而是：

1. 降低单个文件的职责密度，让领域边界可测试、可替换。
2. 冻结公开契约，把兼容层和当前实现明确分开。
3. 按 MP/QGC 的上游质量归属选择实现，不照搬 UI 或全局单例。
4. 每个里程碑独立通过门禁，持续保持可发布状态。

## 执行原则

- 每次只拆一个领域，不在同一个提交里改协议行为。
- 公开 `mg_*` 符号、JSON 字段和错误 envelope 保持稳定。
- 先移动、抽象和加测试，再优化算法或改变状态。
- 兼容入口只修正确性，不再承载新功能。
- 版本提交必须包含 CHANGELOG、版本号同步和真实门禁结果。

## 已完成基线

以下阶段已经在当前代码中完成，后续只修正确性，不重复拆分：

- `0.0.14`：任务校验层拆分。
- `0.0.15`：任务编译与估算拆分。
- `0.0.16`：`.waypoints` 与 QGC `.plan` 互操作拆分。
- `0.0.17`：Mission FFI 拆分。
- `0.0.18`：operations 与 diagnostics 拆分。
- `0.0.19`：磁力计校准按 MP/QGC 的 MAV_CAL 流程修复，并完成任务 Home/启动状态对齐。
- `0.0.20` 到 `0.0.24`：Flutter 控制器拆出任务、遥测、参数、校准和飞行命令适配器。
- `0.0.25`：MAVLink adapter 状态容器拆出 `mavlink_adapter_state.h`。
- `0.0.26`：30 个消息处理器拆出 `mavlink_adapter_handlers.cc`。
- `0.0.27`：任务协议和发送辅助函数拆出 `mavlink_adapter_mission.cc`。
- `0.0.28`：参数、日志、命令发送和 `process()` 分发拆出独立实现文件。

## 后续里程碑

| 版本 | 重构范围 | 完成标准 |
| --- | --- | --- |
| 已完成 A | 任务验证与差异诊断 | 回读差异在 Flutter 中显示 `index/field/expected/actual`；归一化 frame、默认参数和 Home；失败时保留本地任务并给出恢复路径 |
| 已完成 B | Flutter 控制器拆分 | 将 `ConnectionController` 内部拆成连接、遥测、参数、校准、飞行命令和任务适配器；保留旧门面，逐步迁移页面 |
| 已完成 C | MAVLink 内部拆分 | 将 `MavlinkAdapter::Impl` 的消息分发、遥测处理、任务协议、参数协议、命令和日志处理拆成内部模块；不改变 FFI |
| 当前 D | 参数系统增强 | 参考 QGC FactSystem 的 pending/confirmed/failed 语义，参考 MP 的 ArduPilot 元数据和参数覆盖 |
| 下一阶段 E | 任务领域补齐 | QGC `.plan` ComplexItem 保留、GeoFence/Rally 文档模型、ArduPilot 能力矩阵和往返测试 |
| 后续 | 平台真实验收 | Windows 串口/任务/参数/校准、Linux 串口、Android USB 生命周期；构建成功不能替代设备验收 |

### 当前执行进度

第二阶段 B 已完成：任务文档 FFI 绑定已迁移到
`flutter_shell/lib/mission/mission_document_ffi_backend.dart`。`ConnectionController`
继续持有共享 Core 和连接状态，但不再直接实现 `MissionDocumentBackend`，任务页面契约
保持不变。遥测 FFI 绑定现已迁移到
`flutter_shell/lib/connection/telemetry_ffi_backend.dart`，位置、GPS、VFR HUD、Home、
Servo/RC、系统状态和 freshness 的状态仍由 `ConnectionController` 管理。后续按同样方式
拆参数、校准和飞行命令适配器，每次只迁移一个领域并保持门禁通过。参数 FFI 绑定现已
迁移到 `flutter_shell/lib/connection/parameter_ffi_backend.dart`，参数缓存、元数据批处理、
写入队列和确认逻辑仍由 `ConnectionController` 管理。
校准 FFI 绑定现已迁移到 `flutter_shell/lib/connection/calibration_ffi_backend.dart`，
校准状态机、轮询、六面校准姿态确认和 UI 状态仍由 `ConnectionController` 管理。
飞行命令 FFI 绑定现已迁移到 `flutter_shell/lib/connection/flight_command_ffi_backend.dart`，
RTL、降落、起飞、解锁、模式切换、任务启动、引导点、暂停、环绕和设置 Home 的安全
门控及状态确认仍由 `ConnectionController` 管理。

第二阶段 C 已完成并发布到 `0.0.28`：`mavlink_adapter_state.h` 承接状态，
`mavlink_adapter_handlers.cc` 承接消息处理器，`mavlink_adapter_mission.cc` 承接任务协议，
`mavlink_adapter_command.cc`、`mavlink_adapter_params.cc` 和
`mavlink_adapter_log.cc` 承接剩余协议辅助，`mavlink_adapter_dispatch.cc` 承接原始帧兼容
解析和消息分发。`mavlink_adapter_internal.h` 当前只保留 `Impl` 状态、声明和底层发送。

第三阶段 D 已开始并发布到 `0.0.29`：参数写入超时会真正重发 PARAM_SET，`PendingWrite`
记录目标 sysid/compid，发送失败会清理 pending。pending/confirmed/failed 的后续工作
转向元数据、批量刷新和固件差异，不改变现有 FFI 字段。

`0.0.30` 又补上了标准 `PARAM_ERROR` 明确拒绝路径：写入被飞控拒绝时立即进入 failed，
不再等待超时；错误原因按 MAVLink 枚举映射。后续 D 阶段继续处理元数据覆盖和批量刷新。

`0.0.31` 让描述库成为单写和批量写的共同校验入口，阻止 read-only 写入，并让批量
响应携带逐项错误；下一项转向批量刷新策略和固件参数覆盖。

`0.0.32` 新增 `mg_params_refresh`，支持精确名称和前缀解析、去重、未知项报告，并用
现有 PARAM_REQUEST_READ 发送刷新。重试轮次、异步完成状态和失败通知仍待补。

`0.0.33` 补上刷新任务状态和重试：PARAM_VALUE/PARAM_ERROR 会收敛单个请求，超时按
1 秒间隔最多重发 2 次，并通过 `mg_params_get_refresh_status` 暴露聚合状态。下一项
转向分固件参数覆盖。

`0.0.34` 让参数写入和精确刷新经过 ArduPilot 版本重映射，复用现有 ParamRemapping
表和 firmware plugin，不再让旧固件收到 4.7 重命名后的新参数名。

`0.0.35` 给描述库未覆盖的已加载参数增加 `other` 分组和基础类型回退，保证参数不会
因为缺少本地元数据而从参数页消失；`found:false` 保留元数据缺口信息。

`0.0.36` 给参数前缀刷新补上版本反查：旧固件上请求 `WP_*` 时会匹配缓存中的
`WPNAV_*`，从而复用同一套刷新、重试和状态跟踪流程。

`0.0.37` 增加参数元数据覆盖报告，按 active profile 统计 described/fallback/覆盖率并
列出缺失名称，为后续按真实 ArduPilot 参数集补库提供可复现依据。

`0.0.38` 让覆盖率报告使用 canonical 参数名，并补充 QGC 4.7 的常见 ArduPilot 重命名。

`0.0.39` 根据覆盖率结果补齐 `ARMING_CHECK`、`THR_MID`、`TELEM1_BAUD` 和旧版
`WPNAV_*`/`PILOT_SPEED_DN` 的基础描述，模拟参数覆盖率提升到 97%。

`0.0.40` 补全 Rover、Submarine、AntennaTracker 的 profile 识别，后续可在这些归属下
逐步增加机型专属元数据，而不是继续落入空 profile。

阶段 E 已开始并发布到 `0.0.41`：任务文档增加 opaque `extensions`，QGC `.plan` 的
ComplexItem、GeoFence 和 Rally 可无损保留文本 round-trip，但尚不能编辑或校验内部字段。

`0.0.42` 将 QGC Rally version 2 从 opaque 区段提升为正式 `MissionRallyPoint` 模型，
支持文档 JSON 和 `.plan` 双向映射；ComplexItem 与 GeoFence 仍保持 opaque。

`0.0.43` 将 GeoFence version 2 提升为正式模型，覆盖 polygon、circle、inclusion 和
breach-return。

`0.0.44` 将 QGC ComplexItem 从 opaque `extensions` 提升为正式
`MissionComplexItem` 模型，保存 `id`、`item_type`（`complexItemType`，回退 `type`）
和原始 JSON；`.plan` 往返无损，内部字段暂为只读。

`0.0.45` 为 `MissionComplexItem` 增加 `entity_insert_index`，`.plan` 导出时复杂项与
SimpleItem 按原始顺序交错，不再固定追加到末尾；`doJumpId` 对两者统一重新编号。

`0.0.46` 在任务校验层新增 `complex_item.unsupported` 错误：含复杂项的文档不能通过
`validate`/`compile`/上传，阻断“编译只展开普通实体、上传静默丢弃复杂项”的隐患。

`0.0.47` 补齐围栏与 Rally 的几何校验（多边形顶点数量、经纬度范围、圆半径、
breach-return 点及高度），区分于阻断上传的复杂项：围栏/Rally 是独立 mission type，
仍是合法文档。

`0.0.48` 把文档围栏接到 fence mission 传输：`compile_geo_fence_plan` 按 QGC
`GeoFenceManager` 编码编译，新增 `mg_mission_doc_compile_fence` /
`mg_mission_doc_upload_fence` / `mg_mission_doc_clear_fence`；Rally 上传仍待补。

`0.0.49` 以同一模式接通 Rally：`compile_rally_plan` 按 QGC `RallyPointManager` 编码
编译，新增 `mg_mission_doc_compile_rally` / `mg_mission_doc_upload_rally` /
`mg_mission_doc_clear_rally`。

`0.0.50` 让 mock 飞控按 mission type 分库并回显 mission type，新增 `test_sim_fence_rally`
端到端验证围栏/Rally 上传与清除，补上 0.0.48/0.0.49 新增传输路径此前缺失的车辆级覆盖。

`0.0.51` 补齐围栏/Rally 回读：`geo_fence_from_plan` / `rally_points_from_plan` 解析飞控
任务项，新增 `mg_mission_doc_import_fence_from_vehicle` / `import_rally_from_vehicle` 和
适配器 `fence_get_plan_snapshot` / `rally_get_plan_snapshot`；端到端测试扩展为上传 +
下载 + 回读闭环，并因此修掉下载时 mission type 写死为 0 的 bug。

`0.0.52` 补齐围栏/Rally 校验：新增 `mg_mission_doc_verify_fence` /
`mg_mission_doc_verify_rally`，返回 `matches` 与结构化 differences，与任务 verify 对称。

`0.0.53` 打通 ComplexItem 编译：复用 QGC 存入的 `TransectStyleComplexItem.Items`，
不再自己重算 Survey 几何；`expand_document_entities` 让编译/估算按插入位置展开复杂项，
`complex_item.unsupported` 收窄为“确实无法展开”的情况。

## 首轮结果

`0.0.14` 已完成任务校验层拆分：

- `mission_document.cc` 保留文档服务、历史、序列化和实体辅助逻辑。
- `mission_validation.cc` 负责文档、实体、Survey 和 MAV_CMD 参数校验。
- `mission_document_internal.h` 只暴露 core 内部共享的数值、位置和诊断辅助函数。
- 公开头文件、FFI 和外部 JSON 没有任何变化。

`0.0.15` 已完成任务编译与估算拆分：

- `mission_compile.cc` 负责 Survey/Corridor 展开和 `MissionDocument -> MissionPlan`
  编译。
- `mission_estimate.cc` 负责距离、时间、航点、高度和拍照数量估算。
- 编译与估算共用 `expand_survey_entity`，避免参数化任务出现两套展开逻辑。
- `mission_document.cc` 继续缩小，当前只保留文档服务、历史、序列化、持久化和实体
  辅助逻辑。

`0.0.16` 已完成任务文件互操作拆分：

- `mission_interchange.cc` 只保留格式识别和推荐策略。
- `mission_waypoints.cc` 负责 `QGC WPL 110` / MP `.waypoints` 读写。
- `mission_qgc_plan.cc` 负责 QGroundControl `.plan` JSON 读写、字段映射和警告。
- 公开导入导出函数及 JSON 错误语义保持不变。

`0.0.17` 已完成 Mission FFI 拆分：

- `ffi_mission_document.cc` 只保留文档编辑、校验、编译、上传、回读和验证。
- `ffi_mission_interchange.cc` 负责 `mg_mission_doc_export_format/import_format`。
- `ffi_mission_shared.cc` 提供 FFI 内部共用的文档快照、warnings、校验错误和
  operation 状态序列化。
- FFI 导出总数保持 285，公开 C ABI 和 JSON 响应结构不变。

`0.0.18` 已完成 operations 与 diagnostics 拆分：

- `ffi_operations.cc` 负责 params、mission、calibration、log 的统一 operation 快照，
  以及 `mg_operations_get_snapshot` / `mg_operations_cancel`。
- `ffi_diagnostics.cc` 只保留连接诊断、遥测 dashboard、参数写入历史和系统诊断聚合。
- `ffi_operations_internal.h` 提供系统诊断需要的 operations 快照，不形成对外 ABI。
- FFI 导出总数仍为 285，operation JSON 字段和取消语义不变。

## 非目标

- 不在重构中顺便改 UI 设计或替换状态管理框架。
- 不删除未使用的 FFI 符号；先按 API Catalog 判断兼容等级。
- 不把 QGC 的 Qt/QML/QObject 或 MP 的 WinForms/全局单例搬进 Core。
- 不为减少行数牺牲诊断信息、错误语义或平台能力门控。

## 关联文档

- [MP / QGC 上游实现择优准则](UPSTREAM_CODE_SELECTION.md)
- [QGC / MP 可借鉴项对照表](QGC_MP_BACKPORT_MAP.md)
- [当前状态审计](STATUS_AUDIT.md)
- [开发约束](CONSTRAINTS.md)
