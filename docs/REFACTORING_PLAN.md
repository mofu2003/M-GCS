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

## 里程碑

| 版本 | 重构范围 | 完成标准 |
| --- | --- | --- |
| 0.0.14 | 任务文档校验层拆分 | 校验规则从 `mission_document.cc` 分离；行为不变；任务相关测试和全量离线/模拟门禁通过 |
| 0.0.15 | 任务编译与估算拆分 | Screen 展开、编译、估算各自独立；共享同一套 Survey 展开入口；编译/估算测试通过 |
| 0.0.16 | 任务文件互操作拆分 | `.waypoints`、QGC `.plan`、格式推荐分别成模块；导入导出测试与 warning/error 语义保持 |
| 0.0.17 | Mission FFI 拆分 | 编辑、传输、文件互操作按源文件拆分；API Catalog 不变；285 个导出检查保持通过 |
| 0.0.18 | operations 与诊断收口 | 任务、参数、校准、日志共用进度、终态和取消模型；不保留重复状态源 |
| 0.0.19 | 参数系统分层 | 描述库、缓存、写入事务和诊断历史分开；QGC 状态模型 + MP ArduPilot 元数据 |
| 0.0.20 | 连接与 session 边界 | 明确 Link、Vehicle session、active focus 的所有权；保留现有 FFI |
| 后续 | Flutter 大型页面拆分 | 后端契约稳定后拆分配置、连接和任务规划页面，不重新设计视觉风格 |

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
