# M-GCS 与 QGroundControl 代码质量审计

> 状态：2026-10-01 的历史审计快照，记录当时的差距和评审依据；不代表后续版本状态。

> 更新时间：2026-10-01。范围是 M-GCS 后端 `core/`、`bridge/ffi/`、测试，以及
> QGroundControl 的 Mission、FactSystem、Comms、Vehicle、Calibration、
> LogManager 等相关模块。不是对整个 QGC 仓库做总体评分，也不替代 MP 与 QGC
> 的逐模块比较；上游选择规则见
> [MP / QGC 上游实现择优准则](../../UPSTREAM_CODE_SELECTION.md)。

## 结论

**总体上 QGroundControl 的代码质量、成熟度和工程覆盖高于当前 M-GCS。** QGC 经过多年真实机型、固件、平台和多语言场景验证，在任务模型、参数系统、链路管理、自动重连、固件差异和测试规模上明显领先。

但 M-GCS 并不是全面落后。它在一个更小、更清晰的边界内，有几个做得更好的地方：

- FFI 契约和 API Catalog 更集中，前后端边界比 QGC 的 Qt/QML 对象模型更容易替换。
- `MissionDocument` 与编译后的 `MissionPlan` 分离，比直接编辑派生任务更清晰。
- 本地 preflight、离线/模拟测试、界面 token 检查和架构守卫比较系统化。
- 文档更小、更贴近当前代码，不容易出现“文档描述的设计早已不存在”。

当前更合适的定位是：**保留 M-GCS 的边界和工程纪律，逐块选择质量更高的上游实现。**
QGC 在本文涉及的多数通用架构和任务系统中领先，但 ArduPilot 专用命令、参数、校准、
固件和飞行行为仍应优先参考 MP，不能把本文结论扩大成“所有模块 QGC 优先”。

## 量化基线

本次统计只比较相关模块，不比较整个仓库。

| 项目 | 规模 |
| --- | ---: |
| M-GCS `core/src + core/include + bridge/ffi` | 约 37,475 行（未含 JSON 元数据行） |
| M-GCS `core/tests + tests` | 约 11,819 行 |
| QGC 选定后端模块 | 约 71,236 行 |
| QGC `test/` | 约 72,006 行 |
| M-GCS 默认 offline/simulation | 34 个测试 |
| M-GCS Flutter 测试 | 20 个测试 |

行数不等于质量，但说明 QGC 的测试和领域覆盖远高于 M-GCS。M-GCS 的优势是体积小、修改路径短、单次审计可以读完主要模块。

当前基线为 `0.0.71`。Flutter 领域 FFI 适配器和 MAVLink adapter 内部职责已经拆分
完成，参数写入和批量刷新具备校验、拒绝、重试、状态跟踪、版本重映射（含前缀）、
canonical 覆盖率报告和多机型 profile 标识；QGC ComplexItem、GeoFence 和 Rally 均已
正式建模并接入编译/上传/回读/verify（ComplexItem 复用 QGC 存入的
`TransectStyleComplexItem.Items` 展开，内部字段仍只读）。任务真实编辑模型、
参数页深度、链路和平台覆盖仍是主要差距。收口判定见
[完成定义与后端维护收口](../DEFINITION_OF_DONE.md)。

## 评分

评分为 1-5 分，5 分代表成熟生产系统，不代表 M-GCS 当前没有改进空间。

| 维度 | M-GCS | QGC | 判断 |
| --- | ---: | ---: | --- |
| 分层和边界 | 4.3 | 4.5 | M-GCS 的 Core/FFI/Flutter 更清楚；QGC 的 Vehicle/Manager/Plugin 分层更深，但历史耦合更多 |
| 接口稳定性 | 4.7 | 3.8 | M-GCS 有 API Catalog、兼容名称和 ABI 规则；QGC 主要依赖源码内接口 |
| 任务规划深度 | 2.8 | 5.0 | QGC 有 Simple/Complex/Survey/Corridor/VTOL/Fence/Rally/任务树和完整文件格式 |
| 参数系统 | 3.0 | 5.0 | QGC FactSystem 的参数元数据、类型、范围、枚举、批量刷新和写确认更成熟 |
| 连接和链路管理 | 3.2 | 5.0 | QGC 有多链路、链路优先级、切换、重连和链路诊断；M-GCS 以单链路 session 为主 |
| 校准系统 | 3.6 | 4.7 | M-GCS 的 Strategy + Validator 结构清晰；QGC 固件和机型覆盖更广 |
| 诊断和长任务 | 4.0 | 4.5 | M-GCS 已统一 operation snapshot；QGC 各 manager 的字段和状态更贴近真实工作流 |
| 测试质量 | 3.3 | 5.0 | M-GCS 有很好的模拟测试骨架；QGC 有更大规模、更多平台和真实场景测试 |
| 跨平台成熟度 | 3.0 | 4.8 | QGC 的 Android/Windows/Linux/macOS、USB 串口和多链路已长期使用 |
| 可维护性 | 4.2 | 3.6 | M-GCS 小、边界清楚；QGC 功能广但历史包袱和交叉依赖更多 |
| 文档可维护性 | 4.5 | 3.8 | M-GCS 文档集中且当前；QGC 文档量更大但历史层更多 |
| **综合** | **3.7** | **4.6** | QGC 胜在生产成熟度、领域覆盖和测试；M-GCS 胜在边界、体积和当前工程纪律 |

## M-GCS 强于 QGC 的地方

### 1. FFI 边界更清楚

`bridge/ffi/ffi_api.h` 和 API Catalog 提供稳定 C ABI；Flutter 不直接依赖 Vehicle、MAVLink 或 Qt 对象。这个边界比 QGC 的 QObject/QML 属性绑定更容易替换 UI，也更适合做多语言客户端。

### 2. 任务文档与编译计划分离

M-GCS 的 `MissionDocument` 保存编辑意图，`MissionPlan` 只用于传输和回读。这个方向是对的，和 QGC `MissionController`/`PlanManager` 的职责分离相似。

### 3. 门禁更集中

M-GCS 的 `tools/preflight.sh` 同时覆盖后端测试、Flutter analyze/test、UI token、macOS 构建和文档检查。对一个小型项目来说，这比 QGC 的大量分散测试入口更容易维护。

### 4. 代码体量可控

M-GCS 后端可在单次审计中完整阅读；QGC 的问题是优秀实现被庞大的工程历史和跨模块依赖包围，学习和替换成本高。

## M-GCS 明显弱于 QGC 的地方

### 1. 任务领域深度不足

M-GCS 已有普通实体、Survey/Corridor、上传下载和 `.waypoints` / 简单 `.plan`，但缺：

- QGC ComplexItem 体系
- 完整 GeoFence 模型
- Rally 模型
- 任务树、分组、Do Jump 等高级任务关系
- 任务文件和复杂项的无损 round-trip

### 2. 参数系统仍偏轻量

ParamCache 现在有下载取消、写入历史和终态，但还缺 QGC FactSystem 的参数数据库、类型/范围/枚举元数据完整性、批量刷新策略和固件差异处理。

### 3. 链路管理仍是单链路思维

ConnectionManager 有状态机、心跳和 watchdog，但没有 QGC `LinkManager` 的多链路优先级、自动切换、链路故障转移和全链路生命周期管理。

### 4. Android 和真实平台验证仍晚

QGC 的 Android USB 串口已经经过大量设备验证。M-GCS 现在才刚按 QGC 结构接入 `usb-serial-for-android` 和 JNI 回调，权限、拔插、重连、任务和参数流程仍需真机验收。

### 5. 测试规模不足

M-GCS 的模拟测试质量不错，但覆盖面小：

- 没有 QGC 级别的任务复杂项测试
- 没有系统级多链路故障测试
- Android/Windows/Linux 真机或仿真验收不足
- 参数写入、重连和校准的组合场景仍少

### 6. 旧兼容层和新领域模型并存

`MissionManager` 旧任务入口和 `MissionDocument` 新模型同时存在；这在 ABI 兼容上合理，但需要明确“新功能只进新模型，旧入口冻结行为”，否则后续维护会继续分叉。

## 逐步替换策略

### 第一层：低风险对齐

不改 FFI 名称，只改内部语义和字段：

- QGC 任务错误结果映射（已开始）
- QGC MavCmdInfo 参数 label/unit/default/enum
- QGC 参数 pending/confirmed/失败原因语义
- QGC 日志、链路和校准错误文本

### 第二层：领域模型补齐

- 建立 QGC `.plan` ComplexItem 正式保留模型（已完成）
- 建立独立 GeoFence/Rally 文档模型
- 将 Fence/Rally 与任务文件组合保存
- 保留 Survey/Corridor 的设计意图，不导出时强行展开

### 第三层：可替换实现

- 用 QGC 的结构重写参数管理器内部状态，但保留现有 `mg_params_*` FFI
- 用 QGC LinkManager 思路增加链路优先级和自动重连，但保留 M-GCS session 目标模型
- Android USB 继续按 QGC Java + JNI 结构替换，不恢复自造 FD 方案

### 第四层：保持 M-GCS 优势

- 保留 FFI C ABI 和 API Catalog
- 保留 `MissionDocument` / `MissionPlan` 分层
- 保留 preflight、模拟测试和 UI token 检查
- 不把 QGC Qt/QML/QObject 结构搬进 Core 或 Flutter

## 许可约束

QGC 和 MP 都是 GPL 系项目，M-GCS 当前是 MIT。因此“参考和替换”不等于无条件粘贴源码：

- 可以复用协议事实、字段名、状态机行为、错误语义和算法思路。
- 可以直接引用兼容许可证的第三方库，例如官方 MAVLink、usb-serial-for-android。
- 大段 QGC/MP 源码移植前必须单独做许可证决定；否则应写成 M-GCS 模块内的等价实现。

## 推荐下一步

1. 完成任务错误结果、参数状态和日志/校准错误语义对齐（进行中）。
2. 完成 QGC `.plan` 的 ComplexItem、GeoFence、Rally 无损保留（已完成）。
3. 建立 Fence/Rally/ComplexItem 独立文档模型，接入现有 mission document 和 FFI
   （已完成；ComplexItem 可展开参与编译，内部字段仍只读）。
4. 按 QGC AndroidSerial 结构完成 Android 权限、设备发现、拔插和重连验收（需要设备）。
5. 用同一套审计方法检查 Windows/Linux 串口链路（需要设备）。

后端侧剩余项（参数阈值、固件刷写取舍、FFmpeg 并发）与阻塞原因见
[完成定义与后端维护收口](../DEFINITION_OF_DONE.md)。

关联文档：[QGC / MP 可借鉴项对照表](../../specs/upstream/SELECTION_STRATEGY.md)、[后续发展计划](../DEVELOPMENT_ROADMAP.md)。
