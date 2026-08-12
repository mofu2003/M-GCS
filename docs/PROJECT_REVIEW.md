# M-GCS 全局代码优化报告

审阅日期：2026-08-12
审阅方式：全仓静态审阅 + 后端离线/模拟门禁复跑。未执行真实飞控写入、校准、解锁、切模式或任务上传。

## 结论

**不建议重写整体架构。**

M-GCS 的主链路已经是适合长期演进的形状：

```text
可替换前端（Flutter / Swift / React / Web / CLI）
    -> 应用层契约与控制器
    -> C FFI + JSON 契约
    -> C++ Core
    -> MAVLink / Transport / 飞控
```

这里的关键模块是 `core`、`bridge/ffi` 和 `ConnectionSessionBackend`。它们已经形成了清晰的接缝：飞控协议、连接、参数、任务、校准和安全敏感状态机留在后端；Flutter 页面主要是当前的交互适配器。未来替换 UI 时，应该重建呈现和调用适配，而不是重写后端领域能力。

当前的优先事项不是“换技术栈”或“拆掉后端”，而是：修复可复现门禁、避免跨飞控会话残留参数、收紧前端任务语义和轮询归属、再逐步拆分过大的页面库。以下建议都可以局部完成，不需要改变总体架构。

## 审阅范围与事实基线

| 项目 | 当前结果 |
| --- | --- |
| Git 跟踪文件 | 441 |
| C/C++ 源码与头文件 | 约 37,644 行 |
| Flutter `lib` 与 `test` | 约 21,072 行 |
| C++ 测试源码 | 约 8,219 行 |
| 默认离线/模拟 CTest | `0.0.2` 发布门禁 29/29 通过 |
| Flutter `analyze` / `test` | 静态分析无问题，测试 79/79 通过 |

`0.0.2` 发布整理同时加入 Docker 开发环境、统一 MAVLink 生成脚本和 CI 门禁；报告中的构建结论已经按发布候选复跑结果更新。

## 架构评价

| 维度 | 评分 | 判断 |
| --- | ---: | --- |
| 后端领域边界 | 8.5 / 10 | `transport`、`mavlink`、`vehicle`、`firmware`、`config`、`mission`、`calibration` 分域合理。 |
| 前后端解耦 | 8 / 10 | Flutter 未直接导入 MAVLink 或 `dart:ffi`；FFI 集中在 `application/mgcore`。仍有少量任务和参数规则停留在页面层。 |
| FFI 契约 | 7.5 / 10 | C ABI、JSON envelope、字符串释放规则、API catalog 已建立；需要自动化 ABI/schema 契约检查。 |
| 测试与构建 | 7.5 / 10 | 后端 29/29、Flutter 79/79 与静态分析均通过；CI 已生成 MAVLink 头并覆盖 Flutter，仍缺 macOS 和桌面打包门禁。 |
| Flutter 模块局部性 | 6 / 10 | 应用层接缝健康；后台页面通过 `part` 聚合成大库，页面逻辑和轮询仍偏分散。 |
| 跨平台交付 | 6 / 10 | Docker 方向正确；Linux 容器不能替代 Windows/macOS 原生桌面打包和串口验收。 |

总体评分：**7.3 / 10，具备继续深化的基础，不应进行推倒式重构。**

## 已确认的优势

### 1. 领域能力没有绑定 Flutter

`core/` 承担传输、MAVLink、命令队列、参数缓存、任务传输、校准、固件 profile、遥测和多机；`bridge/ffi/ffi_api.h` 以 C ABI 对外暴露。Flutter 的 FFI 绑定集中在 `app/lib/application/mgcore/`，页面经由 `ConnectionSessionBackend` 及其能力接口访问。

这是一条有深度的模块链路：前端只需理解“连接、参数、任务、校准”等应用语义，复杂协议、超时、消息格式和飞控差异被封装在后端。应继续保留这条接缝。

### 2. 安全敏感测试已与默认门禁隔离

`CMakeLists.txt` 把测试区分为 `offline`、`simulation`、`hardware` 和 `manual`。真实飞控相关测试默认不会注册到 CTest，只有显式启用 `MGCORE_REGISTER_HARDWARE_TESTS` 才会纳入。这一点非常重要，不能为了提高“测试数量”而把真实硬件测试加入 CI。

### 3. 连接和参数下载已有应用层防护

`ConnectionController` 已在应用层处理心跳失联、参数下载停滞和有限重试；`PollingTask` 防止单个轮询任务重叠。这个方向是正确的：UI 不应该自行复制 MAVLink 状态机。

### 4. 任务 CAD 的领域归属已定义正确

`docs/MISSION_CAD.md` 和 ADR 0005 已明确：`MissionCadDocument`、ENU/WGS84 换算、约束、编译、诊断应属于后端；`MissionPlan` 是面向飞控的编译产物。这个决策能保证 Flutter、Swift、React、Web 或 CLI 共享同一套权威航线逻辑。

## 需要优先处理的问题

### P0：跨飞控会话的参数/草稿可能残留

`ParameterController` 在应用生命周期内创建，内部缓存 `_parameters`、`_staged`、`_descriptors`；而 shell 只在参数列表为空时加载。断开后重连、或切换活动飞控时，旧飞控的参数和草稿有机会保留，并被写到新会话。

涉及位置：

- `app/lib/features/shell/mgcs_app.dart:24`
- `app/lib/features/shell/mgcs_app.dart:48`
- `app/lib/application/parameter_controller.dart:12`
- `app/lib/application/parameter_controller.dart:219`

建议：在 `ConnectionController` 暴露单调递增的 `sessionEpoch` 或稳定的活动飞控 identity；发生连接、断开、重连、active vehicle 变化时，调用 `ParameterController.resetSession()` 清空参数、描述元数据、草稿和错误。等 bootstrap 完成后再重新加载。该修复只影响 Flutter 应用层，不改 Core/FFI。

必须补测：飞控 A 断开后连接 B、同一飞控重连、参数同步失败重试；确认旧会话参数和未写入草稿绝不会展示或发送到新会话。

### P0：多飞控解析状态需要实例隔离

`MavlinkAdapter::feed_raw()` 的解析路径使用 `MAVLINK_COMM_0`。MAVLink C 库的解析状态按 logical channel 保存，而不是按 `MavlinkAdapter` 对象保存；多飞控同时读入时，串口读线程可能共享帧缓冲，造成交叉帧、丢包或误解码。

涉及位置：

- `core/src/mavlink/mavlink_adapter.cc:52`
- `core/src/mavlink/mavlink_adapter_internal.h:40`

建议：为每个 adapter 使用私有 `mavlink_frame_char_buffer`/`mavlink_status_t`，或受控分配唯一解析通道。保持现有 Core、FFI 和 Flutter 接口不变，并用双 Vehicle 交错帧 fixture 回归验证。

### P0：参数、任务和命令超时不能只依赖入站消息

参数/任务超时检查目前主要在 `MavlinkAdapter::Impl::process()` 的消息处理路径中；`tick()` 没有完整驱动 ParamCache 和 MissionTransfer 的超时泵。飞控完全不回包时，状态机可能一直停留在等待状态，前端只能依赖自己的停滞计时。

涉及位置：

- `core/src/mavlink/mavlink_adapter_internal.h:1319`
- `core/src/mavlink/mavlink_adapter.cc:501`
- `core/src/mavlink/mission_transfer.cc:33`

建议：抽出一个统一的私有 timeout pump，由 `process()` 和周期性 `tick()` 共同调用；再由 `Vehicle`/`ConnectionManager` 现有循环定期驱动。失败、重试和取消结果仍从现有 FFI 快照暴露，不把状态机搬到 Flutter。

### P1：实时参数写入需要“飞控确认”状态

`ParamCache` 已定义 pending write、重试和超时概念，但当前 `set_param_status()` 主要完成发送，未见完整注册 pending write 的路径。因此 `mg_params_set()` 容易被理解为“写入成功”，实际可能只是“PARAM_SET 已发出”。这正是实时写入页面最需要明确的语义。

涉及位置：

- `core/src/mavlink/param_cache.h:69`
- `core/src/mavlink/mavlink_senders.cc:473`
- `bridge/ffi/ffi_params.cc:155`

建议：保留现有公开函数，内部补齐 pending write 注册、PARAM_VALUE 确认、重试和 timeout；在 JSON 结果中增加 `state: sent|confirmed|timeout|rejected` 和可选 operation/ticket。这样不破坏 ABI，却能让页面显示真实状态。

### 已处理：默认后端门禁恢复全绿

初次审阅时 `offline|simulation` 为 27/29；`0.0.2` 发布整理已修复以下两项，并在独立 `build_release_002` 目录复跑为 29/29 通过。

1. `test_sim_params`：发布测试现在明确验证 `BATT_MONITOR` 的中英并列枚举与实时显示契约，翻译覆盖链复跑通过。
2. `test_transport_links`：测试改为使用系统分配的可用本地端口，避免宿主机固定端口占用导致门禁误红。

涉及位置：

- `core/tests/test_sim_params.cc:163`
- `core/src/config/param_descriptors.cc:342`
- `core/tests/test_transport_links.cc:52`
- `core/src/transport/udp_link.cc:92`

处理结果：参数测试明确验证中英并列的枚举契约；UDP 测试使用系统分配的可用本地端口，不再依赖固定 `14550`。

### P1：FFI 参数下载状态过于简化

当前进度接口主要返回 `received/total`，诊断层再据此推断 idle/running/complete；Core 内已有 Waiting、Receiving、Recovering、Complete、Failed 等状态，却没有完整暴露给前端。参数缺包、恢复或失败时，页面无法区分“尚未请求”和“下载失败”。

建议在现有 JSON 中向后兼容地增加 `state`、`retry_count`、`error` 和最后进度时间；保留旧字段，先补 FFI contract fixture。

### P1：校准事件与回调锁边界需要收紧

校准服务已有事件回调和 `fire_event_safe()`，但当前调用路径仍可能在持锁时触发外部回调，且 Vehicle 初始化中没有形成完整的校准事件接线；前端因此主要依赖轮询。若后续接入事件，回调重入会放大死锁风险。

涉及位置：

- `core/src/calibration/calibration_service.cc:391`
- `core/src/vehicle/vehicle.cc:146`

建议：先复制状态快照，解锁后再调用回调；再把校准事件作为现有 FFI event channel 的可选能力接通，并用模拟测试覆盖 start/progress/success/failure/cancel。不要让 UI 自己推断校准终态。

### P1：磁力计兼容解析需要严格检查消息长度

兼容解析路径在长度检查不足时读取 `payload[12..16]`，畸形或截断的 `MAG_CAL_PROGRESS` 可能造成越界读取；同时，手写兼容解析和生成头 helper 存在重复路径。

涉及位置：

- `core/src/mavlink/mavlink_adapter_internal.h:575`

建议：至少按 `MAVLINK_MSG_ID_MAG_CAL_PROGRESS_MIN_LEN` 检查，优先复用已生成的 MAVLink helper，并为截断帧和重复派发增加协议回归测试。

### P1：Mission 简版编辑器仍在 Flutter 直接持有 MAVLink 任务细节

`MissionTransferPanel` 在 UI 中直接创建 `command: 16`、`frame: 3`、`NAV_WAYPOINT`，并分配 sequence。这会让其他前端重复实现任务语义，也与 Mission CAD 的“后端为权威”的约束冲突。

涉及位置：

- `app/lib/features/frontstage/mission_transfer_panel.dart:70`
- `app/lib/application/connection_session_backend.dart:69`

建议：短期先把“添加默认航点”和 sequence 规范化收进 `MissionSessionBackend` 的语义方法；长期让 Mission CAD 通过 core/FFI 接收语义化编辑请求，前端只显示和编辑 CAD 文档。上传继续是独立、显式确认的硬件操作。

### P1：多飞控对象生命周期需要内部 lease

FFI session 解析会取得裸 `Vehicle*`；如果另一个线程同时关闭活动飞控，跨调用使用裸指针存在生命周期风险。现有 C ABI 可以保持不变，内部可引入 shared ownership/短生命周期 session guard，或在一次 FFI 调用内完成解析和操作。

### P1：固件差异和配置语义仍散落在页面中

遥控映射、串口协议、PID 分类、GPS 参数别名等页面仍按参数名、前缀或规则推断 ArduPilot/PX4 行为。短期能工作，但会让新 UI 重复这些规则，并增加固件版本更新后的分叉风险。

涉及位置：

- `app/lib/features/backstage/backstage_remote_control.dart:153`
- `app/lib/features/backstage/backstage_serial_ports.dart:144`
- `app/lib/features/backstage/backstage_pid_tuning.dart:128`
- `app/lib/features/backstage/backstage_calibration.dart:854`

建议：不要立即把全部页面改为“后端生成 UI”。先在应用层增加配置 workflow/view-model，集中页面所需字段、选项、依赖条件和实时写入策略；随后将稳定的 firmware/profile 规则逐项下沉 FFI。这样既避免一次大重构，也让 Swift/React 能复用同一语义契约。

### P1：高频轮询和长操作调度分散在多个 Widget

舵机、遥控、校准页面各自启动轮询。单个 `PollingTask` 能防重叠，但 `IndexedStack` 保活后台页，页面隐藏后可能仍继续采样；这些调用也不统一通过 `ConnectionController` 的操作串行化。

涉及位置：

- `app/lib/features/shell/mgcs_app.dart:84`
- `app/lib/features/backstage/backstage_servo_output.dart:23`
- `app/lib/features/backstage/backstage_remote_control.dart:80`
- `app/lib/features/backstage/backstage_calibration.dart:50`
- `app/lib/application/polling_task.dart:3`

建议：保留 `PollingTask`，但把状态与启停归属移到应用层的领域控制器，例如 `ServoOutputController`、`RemoteControlController`、`CalibrationController` 或统一 operation scheduler。页面只订阅状态，并在当前页不可见时停止高频轮询。这不需要改变后端架构。

### P1：参数批量写入 fallback 存在永久忙碌风险

`ParameterController.writeStaged()` 的 change-set 分支有 `try/finally`，但逐项 fallback 分支没有。如果 `backend.writeParameter()` 抛出异常，`_writing` 可能不复位，页面之后会保持不可编辑状态。

涉及位置：

- `app/lib/application/parameter_controller.dart:277`

建议：两个分支共用 `try/finally`，并增加“某一项写入抛异常后仍可再次操作”的单测。优先使用已有的 change-set 后端能力，fallback 只保留兼容用途。

## 可维护性优化

### 1. 拆分后台大库，但保留 `BackstagePage` 入口

`backstage_page.dart` 通过 8 个 `part` 聚合后台功能，相关代码约 8,844 行。`PID` 甚至复用了名称属于“校准”的私有 widget，说明共享视觉组件缺少明确模块归属。

涉及位置：

- `app/lib/features/backstage/backstage_page.dart:21`
- `app/lib/features/backstage/backstage_pid_tuning.dart:180`

建议分两步：

1. 先提取 `_GlassPanel`、页面标题、参数编辑器、配置表格、section header 到 `features/backstage/widgets/`，改为普通 import。
2. 再把 calibration、PID、serial、servo、remote control、safety 变为独立 Dart library；`BackstagePage` 仅负责导航、依赖注入和路由。

这是提升局部性，不改变 UI、FFI 或 Core 的外部接口。

### 2. 给 FFI 加自动契约检查

FFI 的方向是正确的，但接口已较多且有 compatibility 别名。当前 `API_CATALOG.md` 是人工目录，尚不能保证声明、实现、Dart binding、JSON 字段和所有权规则同时变化。

建议新增自动检查：

- `ffi_api_*.h` 声明与导出实现的一致性；
- preferred API 的 JSON fixture/schema 回归；
- `mg_free_string()` 的调用约束；
- Dart decoder 对未知字段保持兼容；
- compatibility API 不新增新消费者。

不要合并成一个“大 FFI 框架”；现有按领域拆分的接口组织应保留。

### 3. 缩小过宽的 MAVLink 公开接口

`MavlinkAdapter` 的公开头约 826 行，直接暴露命令、任务、围栏、日志、相机、云台、视频和文件传输等大量方法；内部共享状态头约 1,363 行。PIMPL 已隐藏数据实现，但公开 interface 仍偏宽。

涉及位置：

- `core/include/core/mavlink/mavlink_adapter.h:410`
- `core/src/mavlink/mavlink_adapter_internal.h:76`

建议先做内部整理：按 command、mission、payload、log/file 等私有 helper 分组，并为上层模块使用更语义化的 manager。不要在当前阶段拆 ABI 或移动大量实现文件，避免高风险改动。

### 4. 性能基线优先于异步包装

`MgCoreConnectionSessionBackend` 的方法形式上返回 `Future`，但其内部存在同步 FFI 和 JSON decode。大量参数、描述目录或高频仪表快照可能阻塞 Flutter UI isolate。

建议先记录参数数量、`mg_params_get_all`、描述目录读取和 dashboard snapshot 的 P50/P95 耗时；超出交互阈值后，再选择 native 聚合快照、增量事件或 isolate 解码。不要仅在 Widget 上再套一层 async 来掩盖问题。

## 构建、测试与交付

### 已验证

- `cmake --build build_check --config Release -j2` 成功。
- CTest 默认安全过滤正确，硬件/手动测试未纳入普通门禁。
- `git diff --check` 通过。

### 当前缺口

1. `third_party/mavlink` 不提交 C 生成头；本地、Docker 与 CI 均依赖固定 `pymavlink 2.4.49` 的统一生成步骤。
2. CI 已覆盖 Ubuntu/Windows 后端和 Linux Flutter `analyze`/`test`，但尚未覆盖 macOS smoke build 与桌面打包。
3. Docker 能统一 Linux 下的生成、编译与静态测试，但不能替代 Windows/macOS 原生打包或串口验收。
4. Docker 首次构建仍需要访问 Docker Hub、Flutter SDK 和 pub 依赖源；本次本地镜像构建在 Docker Hub 匿名认证阶段超时，容器内完整 `check` 仍需在网络畅通环境补验。

建议顺序：保持现有后端与 Flutter 门禁全绿；再增加 macOS smoke build；Windows/macOS 真机串口只读验收继续由显式人工流程执行。

## 推荐实施路线

### 第一阶段：恢复可信基线

1. 保持后端 29/29 与 Flutter analyze/test 门禁全绿。
2. 补参数会话 reset 与批量写入 fallback 的 `finally`，补相关 Flutter 测试。
3. 保持 CI 中 MAVLink 生成版本、后端测试和 Flutter 静态门禁同步更新。

### 第二阶段：收紧应用层模块

1. 将 Mission 简版默认航点和序号逻辑从 Flutter 页面移到语义接口。
2. 让后台高频采样拥有统一的应用层控制器和页面可见性生命周期。
3. 抽取后台共享 widget，逐页消除 `part` 聚合。

### 第三阶段：扩展可替换前端能力

1. 为 serial、servo、RC、PID、安全、校准建立稳定的 workflow/profile 契约。
2. 按 Mission CAD 文档先实现 core 数据模型、JSON round-trip、固定编译 fixture 和诊断映射测试。
3. CAD core/FFI 稳定后，再实现 Flutter 画布或其他 UI；不要让每个前端各自拥有几何和任务编译逻辑。

## 当前不建议做的事

- 不重写 C++ 后端，不把 MAVLink 逻辑搬到 Flutter。
- 不为了“统一”把全部 FFI 合成单文件或单一巨大接口。
- 不把真实飞控测试、校准、写参数、任务上传加入默认 CI。
- 不在 Mission CAD UI 完成前，在 Flutter 再实现一套权威几何或 MAVLink 任务生成器。
- 不以 Docker 成功替代 Windows/macOS 的原生桌面发布验收。

## 最终判断

M-GCS 的长期资产是后端领域模型和 FFI 契约，不是某一套 Flutter 页面。现有架构已经能支撑 UI 更换和多端扩展；接下来的工作应提升接缝的可信度、模块深度和维护局部性，而不是推倒重来。
