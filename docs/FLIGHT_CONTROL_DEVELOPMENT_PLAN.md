# 飞行控制与前后端解耦开发计划

> 编制日期：2026-09-22
> 目标：在不引入过度工程化的前提下，让前端可以替换、让后端可以独立演进，并补齐航点、飞行模式、解锁/锁定和多机批量控制的安全闭环。

## 1. 当前架构结论

### 1.1 已经做对的地方

- Flutter 页面主要通过 `ConnectionController`、`ParameterController` 和应用层模型工作，没有直接读取 FFI 指针或 native JSON。
- `ConnectionSessionBackend`、`MissionSessionBackend` 等接口已经形成了应用层接入契约。
- `MgCoreConnectionSessionBackend` 是一个明确的 Adapter，负责把 `MgCoreConnectionClient` 的 native 数据转换为 Flutter 需要的语义模型。
- C++ 核心、FFI C ABI 和 Flutter 应用已经有清晰的技术分层；未来可以增加 Swift、Web、CLI 或另一个 Flutter 前端，而不必重写核心飞控逻辑。
- 参数同步已经抽成独立模块，连接异常、遥测临时失败和会话代次隔离也已有测试保护。

### 1.2 当前仍存在的耦合

这不是“前后端完全绑死”，而是“接口边界已经存在，但还没有完全稳定”：

1. **UI 对 capability interface 有感知**：页面会使用 `backend is MissionSessionBackend` 之类的判断。这样虽然没有直接依赖 MgCore，但 UI 仍然知道某个后端是否实现了某个具体接口。
2. **连接模型以单一活动飞行器为中心**：`ConnectionSessionSnapshot` 和 `ConnectionController` 主要管理一个 active vehicle，不能表达多个飞行器的独立状态、任务和命令结果。
3. **任务接口仍然偏传输型**：当前 `MissionSessionBackend` 主要是下载和上传 `List<MissionItemSnapshot>`，本地任务草稿、任务校验、地图编辑和飞控任务传输之间还没有明确分开。
4. **飞行命令尚未进入 Flutter 应用层**：C++/FFI 已有 arm、disarm、set mode、RTL、takeoff、land 等能力，但 Flutter 没有统一的高风险命令接口、确认策略和状态确认流程。
5. **DTO 和错误语义仍需收敛**：不同能力的结果、超时、取消、部分成功和飞控拒绝需要统一的可跨前端表示，而不是让每个页面自行解释字符串。

### 1.3 解耦目标

目标不是增加大量层次，而是固定一个足够深的应用层接口：

```text
任意前端
   ↓
应用层 Facade / Controller + 稳定 DTO、错误码、事件
   ↓
后端 Adapter（MgCore、SITL、回放、Fake、未来的其他实现）
   ↓
稳定 FFI C ABI / C++ core
```

前端应该只知道“连接、任务、车辆、飞行命令”的业务语义；不能知道 MgCore JSON 字段、FFI 函数名、活动车辆的内部选择方式，也不能自行决定任务序号、命令目标或安全校验。

## 2. 设计原则和明确不做的事情

- 保留现在的 `ConnectionSessionBackend` 体系，优先加深已有模块，不另起一套 Repository/Service/UseCase 大框架。
- 一个真实变化点才引入一个接口；先有第二个 Adapter（例如 Fake/SITL/回放）或明确的外部前端需求，再抽象新的 seam。
- 业务结果返回结构化 DTO；人类可读消息用于展示，稳定错误码用于逻辑判断。
- 所有危险命令都必须：检查前置条件、禁止重复执行、支持超时/取消、显示明确结果，并等待遥测状态确认。
- 批量操作必须显式传入目标车辆列表，不能因为 active vehicle 改变而意外作用到另一架飞机。
- 本阶段不做通用事件总线、全局状态机、过度拆分的 Repository 层，也不把 Flutter widget 状态下沉到核心库。

## 3. 分阶段计划

### 阶段 0：固化应用层接口和契约测试（当前下一步）

**目标**：让更换前端只需要实现同一组应用层接口，不需要修改后端实现。

工作项：

- 保留 `ConnectionSessionBackend` 作为连接会话入口；新增或整理 `FlightCommandSessionBackend`、`FleetSessionBackend` 和任务编辑/传输接口。
- 把 capability 检测从页面移到应用层 facade。页面读取 `capabilities` 和操作结果，不直接判断具体 backend 类型。
- 统一 `OperationResult`、`OperationError`、超时、取消、拒绝和部分成功的数据结构。
- 明确 DTO 的版本兼容规则：枚举未知值可降级，新增字段不破坏旧前端，稳定错误码不依赖中文文案。
- 为每个接口补 Fake backend contract tests；同一套测试未来可用于 MgCore、SITL 和回放 Adapter。
- 保持 FFI 细节位于 `app/lib/application/mgcore/`，页面禁止新增对 `mgcore_*` 文件的依赖。

验收标准：

- 页面代码不再通过 `backend is SomeCapability` 决定业务流程。
- Fake backend 可以驱动连接、任务和命令页面，不需要加载 native library。
- MgCore Adapter 和 Fake backend 通过同一组契约测试。
- 现有 Flutter 测试、`flutter analyze` 和 macOS Debug 构建继续通过。

### 阶段 1：连接、会话和飞控健康闭环

**目标**：把连接成功定义为“会话可用”，而不是“某个 connect 函数返回成功”。

工作项：

- 完成连接、断开、重连、心跳超时、串口拔出、参数同步停滞和恢复的统一错误策略。
- 将连接状态、参数同步状态、飞控健康状态和链路质量分别建模，避免一个全局 error 覆盖所有信息。
- 增加 session refresh、取消和旧会话结果隔离的契约测试。
- 对串口、TCP、UDP、SITL 和回放适配器定义相同的最小生命周期。

验收标准：

- 临时遥测错误不会误断开健康会话；会话刷新失败会停止轮询并进入可恢复错误。
- 断开异常也会完成本地清理，不会残留旧飞行器、旧参数或旧任务状态。
- 连续重连和拔插设备测试不产生旧请求污染新会话。

### 阶段 2：航点和任务规划闭环

**目标**：让航点从“可上传的列表”发展为“可编辑、可校验、可恢复的任务文档”。

工作项：

- 将本地任务草稿与飞控传输计划分开：草稿负责编辑和保存，编译结果负责 MAVLink 传输。
- 支持航点增删改、排序、复制、批量调整高度/速度，以及起飞点、返航点、降落点等常用命令。
- 为每个任务项提供稳定 ID、命令类型、坐标/高度约束、自动继续策略和源诊断信息。
- 上传前执行离线校验：坐标有限性、航点数量、序号、坐标系、高度、机型能力和命令参数。
- 上传/下载提供进度、超时、取消、失败项和重试；上传完成后通过下载或状态查询确认结果。
- 接入现有 Mission CAD 方向：几何、测绘和编译逻辑放在 `core`，地图只是一个前端 Adapter。
- 支持任务保存、导入、导出和版本迁移；文件格式不依赖 Flutter widget 或地图 SDK。

验收标准：

- 不打开地图也能创建、校验、保存和导出任务。
- 不同前端对同一任务文档生成一致的编译结果和诊断。
- 真实飞控或 SITL 能完成下载、编辑、上传、再次下载比对。
- 上传失败可明确区分连接失败、飞控拒绝、超时、取消和校验失败。

### 阶段 3：飞行模式、解锁和锁定

**目标**：补齐高风险飞行命令，并建立安全的状态确认闭环。

建议的应用层接口（名称可在实现时微调）：

```dart
abstract interface class FlightCommandSessionBackend {
  Future<ConnectionOperationResult> arm({bool force = false});
  Future<ConnectionOperationResult> disarm({bool force = false});
  Future<ConnectionOperationResult> setFlightMode(String mode);
  Future<List<FlightModeOption>> loadAvailableModes();
}
```

工作项：

- Flutter client 暴露已有 `mg_command_arm`、`mg_command_disarm`、`mg_command_set_mode` 和相关模式查询能力。
- 模式列表使用结构化 ID、显示名、飞控栈和可用条件，不让前端拼接 `base_mode/custom_mode`。
- 解锁前检查连接、心跳、飞控健康、GPS/EKF、遥控和已知 pre-arm 失败；`force` 仅在明确的高级操作流程中开放。
- 解锁、锁定和模式切换做命令去重，按钮在请求期间禁用；超时后显示“状态未知”，不能直接当作失败或成功。
- 命令 ACK 只表示飞控接受/拒绝请求；最终结果必须等待遥测中的 `armed` 或 `mode` 变化确认。
- 为 RTL、起飞、降落和紧急停止预留同一命令结果模型，但本阶段先完成模式/解锁/锁定。

验收标准：

- 未连接、健康检查失败或命令进行中时，危险按钮不可执行。
- 飞控拒绝、ACK 超时、遥测未确认和成功确认分别有明确状态。
- SITL/Mock 能覆盖 arm/disarm/mode 的成功、拒绝、超时和重复点击场景。
- 日志能追踪目标车辆、命令、请求时间、ACK 和最终遥测确认。

### 阶段 4：多机模型和批量控制

**目标**：从“单一 active vehicle”升级为可观测、可选择、可审计的机队模型。

首批应用层模型：

```text
VehicleId
VehicleSummary
VehicleTelemetrySnapshot
FleetSnapshot
VehicleSelection
BatchCommandResult
```

工作项：

- 在 Flutter 应用层暴露车辆列表、活动车辆、选择/取消选择和每架飞机独立快照。
- 单机命令先统一为显式 `vehicleId` 目标；active vehicle 只作为 UI 默认选择，不作为隐式命令目标。
- 批量命令返回每架飞机的 `success / rejected / timeout / cancelled / unknown`，并保留错误码和可读消息。
- 参数、任务、日志和告警按车辆隔离；切换车辆不能复用上一架飞机的缓存状态。
- 批量解锁默认关闭，若业务确实需要，必须加入更强确认、目标摘要和逐机结果；模式切换可支持多选，但必须明确部分成功。
- 对车辆上线、离线、system ID 变化和重复发现建立稳定去重规则。

验收标准：

- 两架以上 SITL/Mock 可同时连接并显示独立状态。
- 批量命令不会把 active vehicle 切换误当作目标切换。
- 一架飞机失败不会覆盖其他飞机的成功结果。
- 车辆断开后其任务、参数和命令状态被隔离清理，并可恢复重连。

### 阶段 5：真实硬件、SITL 和长期稳定性验证

**目标**：把“接口可用”提升为“飞控工作流可安全交付”。

验证矩阵：

- 串口设备插拔、权限变化、波特率错误、TCP/UDP 断链和自动重连。
- 参数同步停滞、恢复、取消、重复连接和长时间遥测。
- 航点下载、离线编辑、校验失败、上传中断、取消、重试和结果比对。
- 模式切换、解锁/锁定、拒绝、超时和遥测确认。
- 两架以上飞机的同时连接、单机控制、批量模式切换和部分失败。
- 资源占用、内存增长、日志完整性和应用退出时 native 资源释放。

交付物：

- 自动化 contract/integration fixtures。
- 按飞控栈和版本维护的能力矩阵。
- 可导出的诊断包：连接信息、车辆摘要、命令记录、错误码和时间线。
- 发布前的 macOS、Windows 和真实串口验收清单。

## 4. 推荐执行顺序

下一步从**阶段 0 的接口边界固化和契约测试**开始，顺序如下：

1. 把页面对 capability 的判断收敛到一个应用层 facade，并补齐 Fake backend contract tests。
2. 先接通 `FlightCommandSessionBackend` 的模式查询、arm/disarm 和 mode command，再做危险操作 UI。
3. 将现有任务传输升级为“任务草稿 → 校验/编译 → 上传/下载”的闭环。
4. 在单机命令稳定后引入 `FleetSnapshot` 和显式 vehicle-targeted command。
5. 最后做真实硬件、多机和长时间稳定性验收。

这个顺序可以避免两类返工：一是先做 UI 后发现后端接口不能替换，二是先做批量控制后发现单机命令没有明确的目标和结果模型。

## 5. 阶段完成定义

每个阶段完成时必须同时满足：

- 应用层接口和 DTO 已有文档，且不泄漏 native/Flutter 专属类型。
- 至少有 Fake/离线测试覆盖主要成功和失败路径。
- 直接相关的 Flutter 测试、`flutter analyze`、macOS Debug 构建和 C++ 离线测试通过。
- 若涉及危险命令或真实链路，必须补充 SITL/硬件验收记录，不能仅以编译通过作为完成标准。
- 新增能力不要求页面知道 MgCore 的具体实现，也不要求后端知道页面布局。
