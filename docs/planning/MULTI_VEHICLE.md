# 多机连接与焦点管理

> 基线版本：`0.0.80`
> 最后更新：2026-10-09
> 适用范围：多链路、多飞控发现、活跃飞控切换、分机状态隔离、多机地图与批量动作
> 非目标：编队算法、蜂群任务分配、自动协同

本文是多机能力的**单一权威文档**，整合了原实施计划、状态审核和能力对标三份材料。
上游调研（QGC/MP 源码对照）见
[多机关联研究](../research/MULTIVEHICLE_CONNECTION_RESEARCH.md)。

**一句话状态**：多端点、多 route、同机多链路归并、UDP/TCP loopback/POSIX PTY 多机闭环、
signing 密钥库与候选密钥自动探测、按 Vehicle 的长操作取消与超时隔离、多选和批量动作
第一版均已完成；后端剩余主要是产品取舍、真实总线验收和 ComplexItem 语义编辑。

---

## 阶段状态总览

| 阶段 | 状态 | 结论 |
| --- | --- | --- |
| MV0 契约与基线 | 已完成 | 研究文档、实施计划和验收矩阵齐备 |
| MV1 多链路单机 | 已完成 | 多 endpoint、链路列表、active 切换、状态重绑定和基本 UI 已接通 |
| MV2 共享链路解复用 | 已完成（本地验证） | 同一 endpoint 按 `sysid/compid` 发现多架飞机；UDP 有真实 socket 验证，TCP loopback 与 POSIX PTY 通道回归通过 |
| MV3 生命周期与多链路 | 已完成（通用框架） | 独立 link handle、同 sysid 跨 endpoint 归并、per-route health/primary、route 精确关闭、30 秒重发现抑制、多 route 遥测汇聚 |
| MV4 前端状态隔离 | 后端完成，前端/现场验收待用户 | 主要状态按 session 保存/恢复；参数、任务、围栏、Rally、校准、日志可按明确 Vehicle 取消；协议超时由适配器定时驱动 |
| MV5 多机 UI 与地图 | 已完成基础交付 | Fleet 快照携带位置/Home/任务路径/模式/armed/电量/GPS/链路健康；地图支持非 active 弱化图层与点击 |
| MV6 多选与批量动作 | 已完成第一版 | 独立 `selectedVehicles`；批量解锁/上锁/返航/开始任务复用长按确认；逐机结果区分成功/拒绝/超时/失败 |

---

## 实施进度（2026-10-09）

本轮已完成 MV1 多链路单机闭环，并将 UI 入口开放；UDP 共享链路已接通，
串口/TCP 本地共享链路验证已接通；多机地图和批量控制第一版已交付：

- `flutter_shell/lib/fleet/fleet_models.dart`：独立连接配置、不可变车辆摘要/快照、
  带焦点 revision 的命令目标凭证。session ID 与 MAVLink system ID 明确区分。
- `fleet_backend.dart`：连接、列表、精确关闭和选择的可测试接口。
- `fleet_ffi_backend.dart`：复用现有 C ABI；新增链路/route 读取与精确关闭适配。
- `fleet_controller.dart`：首链路选择、后续链路不抢焦点、重复端点拒绝、
  精确关闭、焦点失效和异步确认的目标保护；独立 `selectedVehicles` 集合、批量
  目标冻结和批量执行期间焦点锁；测试用内存 backend 与 FFI 共用接口。
- `mg_fleet_batch_panel.dart` / `mg_command_confirmation.dart`：多机控制面板支持
  逐机勾选、全选/清空、active 切换与批量选择分离；批量动作复用 0.5 秒长按确认，
  确认条显示目标飞机数量和系统 ID；返航、解锁/上锁、开始任务均通过显式 Vehicle 目标的异步 submit/poll 接口逐机下发并
  保留逐机结果。
- 现有单机 `ConnectionController` 的连接/选择/关闭接到同一 Fleet 适配层，
  轮询不再把 native focus 强制切回旧 session；支持连接面板中的链路列表、
  精确断开非活动链路和顶部状态栏车辆选择器，单机仍不显示选择器。
- `ConnectionController` 按 session 保存/恢复遥测、参数、任务、消息、Home、
  Go To、飞行统计和波形偏好；切换后重新拉取 native cache，旧的 Go To 显示目标
  不会因返回旧飞机而复活。
- 任务文档在 core 内按 session 持有独立 `MissionDocumentService`；离线草稿只在
  首次连接时复制，不与其他飞机共享 revision/undo 历史。前端旧任务回调会因
  焦点校验失败而拒绝写入当前飞机。
- 地图悬浮确认记录 session 与焦点 revision；A → B → A 后旧确认仍然无效。
  指令执行期间拒绝通过 Fleet 切机/关闭，飞行命令每次 native 下发前检查目标。
- 保留 `mg_command_execute_for_vehicle()` 作为同步兼容入口；新增
  `mg_command_submit_for_vehicle()` / `mg_command_poll()` 作为 Flutter 飞行命令主路径。
  native 以显式 Vehicle 目标和线程局部覆盖路由，不调用 `mg_vehicle_select()`，所以
  命令执行期间 UI active focus 不会被临时改写。
- 双模拟飞机测试验证 RTL 只进入目标飞机的命令队列，非目标飞机无命令，
  UI active focus 前后不变；不存在的 `vehicle_id` 明确拒绝。
- Core 使用官方 `mavlink_frame_char_buffer` 和适配器自有 RX buffer/status，
  两条独立链路交错接收半帧不会污染同一个 `MAVLINK_COMM_0` 解析状态。
- MV2 已新增 `core/mavlink/mavlink_router.*` 与
  `core/transport/link_session.*`：一个共享链路只解析一次字节流，按
  `sysid/compid` 路由到独立 `MavlinkAdapter`，支持未知心跳发现、重复身份拒绝、
  分片/粘连帧恢复和 peer 感知回复。
- `ILink` / `UdpLink` 增加 peer-aware 回调与 `send_to()`；
  `ConnectionManager` 支持挂接共享 session，`MultiVehicleManager` 支持
  UDP 共享 endpoint。首个心跳绑定占位 Vehicle，后续 sysid 自动创建独立
  Vehicle，关闭其中一架不会误关仍在使用的共享 socket。
- FFI 的真实 UDP 测试已验证同一端口上 sysid 1/2 各自建立 Vehicle，两个独立
  sender peer 都能收到回复；现有 Fleet/Flutter 双端点回归仍通过。
- MV3 基础层新增 `VehicleLinkManager` 和 30 秒重发现抑制：支持 route 心跳时间、
  `comm_lost`、primary route 状态模型；`Vehicle` 核心层支持多条 shared route
  并汇聚遥测。`MultiVehicleManager` 已以独立 link handle 自动归并同一
  `sysid/compid` 的多个 managed endpoint，并提供 `close_route()` 精确保留共享
  socket；FFI `mg_connection_list()` 与 Flutter Fleet link/vehicle 模型已接通。
- `mg_vehicle_list` 改用 shared_ptr 注册表快照，列表读取期间车辆移除不使指针失效。
- 真实双 UDP 销毁测试发现并修正 Core 清理顺序：先停 GCS 心跳线程，解除事件
  回调，再关闭共享引用快照里的车辆，避免析构时访问已经释放的 core 事件对象。

### 本轮验证与仍需后续验收的能力

- Flutter 回归覆盖焦点不被新连接抢走、重复端点、串口不同波特率排他、
  A → B → A、异步操作锁定、外部断链与错误传播。
- 后端回归覆盖两个独立 Vehicle 的 FFI 摘要/焦点/关闭隔离、注册表生命周期租约、
  共享 Router 的分片/粘连/重复身份/`MAG_CAL_PROGRESS` 兼容路由，以及独立解析器
  的半帧交错和损坏帧恢复。
- 真实动态库 loopback 验证脚本：
  `.venv/bin/python tools/tests/multivehicle_udp_smoke.py build/mgcore.dylib 3`。
  使用 OS 分配的临时端口和 sysid 41/42，不使用用户 14550/SITL；覆盖真实 socket
  注册、心跳、焦点查询、关闭隔离与销毁，3 轮通过。
- 多机 UI 入口已开放：连接面板显示当前链路并支持选择/精确断开，顶部状态栏在
  多机时显示车辆选择器；不自动打开第二个端点，也不操作用户正在连接的飞控。
- 已实现显式 vehicle ID 飞行命令 ABI、UDP 同一 socket 多 sysid、
  同机多链路合并、route 级精确关闭和冗余 primary 切换；串口/TCP 共享语义、
  MAVLink signing 适配器核心、单机 FFI、命名密钥库本地持久化、设置页和 SETUP_SIGNING 下发已完成；长操作可按明确 Vehicle 取消，切机会自动取消旧机可取消操作。多机地图的非 active 摘要、任务/Home/围栏/Rally 图层已完成；MV6 多选、批量确认和逐机结果已完成第一版。每个 `MavlinkAdapter` 的 TX sequence（含 FTP 发送）及 signing 状态已实例隔离；候选密钥 HMAC 自动探测、命中后全 route 同步和状态回传已接通。
 分机状态隔离的后端取消/超时边界已完成，前端交互和现场结果按 MV4 验收。
- legacy `mg_command_*` 仍依赖 active focus，但 Flutter 飞行命令已统一使用
  `mg_command_submit_for_vehicle()` / `mg_command_poll()`；
  `mg_command_execute_for_vehicle()` 保留给同步兼容调用。参数、任务和遥测读取继续
  通过当前 native active focus，并在 UI 层用 session/焦点校验和长操作门控隔离。

### 下一切片的进入条件

MV1 已完成多链路独立 session、车辆选择器、链路列表、精确断开和按 session 的
运行时/任务文档隔离；MV2 已完成 UDP 共享 endpoint 的单次解析、多 sysid
Vehicle 发现和 peer-aware 回复；MV3 已补同机多链路合并、route 级精确关闭和链路句柄模型；MV5/MV6 已完成多机地图基础层、围栏/Rally 图层、选择集合、批量确认和逐机结果；候选密钥自动探测和异步超时隔离已完成。下一步是产品取舍、真实总线验收和 Survey/Corridor 等复杂项语义编辑。参数写入/任务传输/校准执行中仍阻止焦点切换；
没有确认协议取消语义前，不自动取消飞行中的任务或正在执行的校准。

本轮构建/检查：`flutter analyze lib test`、UI token 扫描、macOS debug 构建通过；
后端 offline/simulation 37/37 通过；123 个常规 Flutter 测试通过，实际 Fleet FFI 的
四个 UDP 测试由 `MGCS_FLEET_LIBRARY` 单独启用后通过，覆盖两个独立端口、同一端口
sysid 1/2、批量 `mission_start` 显式目标分派和候选密钥自动探测。运行实际 FFI 测试：

```bash
cd flutter_shell
MGCS_FLEET_LIBRARY="$PWD/../build/mgcore.dylib" \
  flutter test --no-pub test/fleet_ffi_smoke_test.dart
```

当前实现参考本地 QGC `bf0096d97` 的 `MultiVehicleManager`、`MultiVehicleSelector`
和 MP `a2fcd74d6` 的 `MAVList`、`ConnectionControl`；借用生命周期与显式地址语义，
不复制上游实现源码。

---

## 一、目标与非目标

### 目标

1. 一条链路可以发现和控制多架飞控。
2. 一个 M-GCS 实例可以同时打开多条独立链路。
3. 同一架飞控可以关联多条链路，单条链路失效不立即删除飞控。
4. UI 区分 `activeVehicle` 和 `selectedVehicles`：
   - `activeVehicle` 决定当前地图、遥测、参数、任务和消息面板显示哪一架。
   - `selectedVehicles` 作为后续批量操作的独立集合。
5. 所有飞行命令都必须明确落到目标 `sysid/compid`，不能依赖模糊的全局连接状态。
6. 单机连接和现有单机工作流保持兼容，不要求用户先进入多机页面才能连接。

### 非目标

- 不做蜂群 leader/follower、编队算法和任务自动分配。
- 不在第一版复制 QGC 的完整移动端多机面板。
- 不在第一版实现多链路主备自动切换。
- 不为相机、云台、ADSB 等非飞控组件单独创建“飞机”条目。
- 不通过隐藏的全局广播命令实现批量操作。

---

## 二、QGC 与 MP 的优点取舍

| 主题 | QGC 优点 | MP 优点 | M-GCS 采用方式 |
| --- | --- | --- | --- |
| 链路模型 | `LinkManager` 管理链路，MAVLink 先于 Vehicle 存在 | 端口列表直观，一个端口可查看多个 MAV | 采用 QGC 的共享链路生命周期；保留 MP 的多 `sysid/compid` 语义 |
| 车辆发现 | 心跳合格后才创建 Vehicle，首机设 active | 端口内统计 heartbeat 并自动选择当前 MAV | 采用 QGC 的创建/过滤规则，使用 MP 的 `sysid/compid` 作为路由键 |
| 车辆身份 | 每架 Vehicle 有独立子系统，命令目标天然明确 | `MAVState` 明确保存 sysid/compid | 内部稳定 `vehicle_id`，MAVLink 路由使用 `(sysid, compid)` |
| 焦点模型 | `activeVehicle` 与 `selectedVehicles` 分离 | `sysidcurrent/compidcurrent` 符合操作员切换习惯 | 采用 QGC 双层集合，UI 交互参考 MP 的快速切换 |
| 多链路 | `VehicleLinkManager` 支持一机多链路与健康切换 | 多端口和每端口状态可见 | 先实现多链路关联和独立失效，冗余自动切换后置 |
| 命令路由 | Vehicle 对象携带目标，安全门控清晰 | 命令 API 显式携带 sysid/compid | 新接口显式携带车辆 ID；活跃车辆只是 UI 焦点 |
| 状态机与测试 | 生命周期、断链、状态分层和 mock 测试成熟 | ArduPilot 现场语义成熟 | 结构、状态机和测试参考 QGC；ArduPilot 行为参考 MP |
| UI | 多机地图、活跃强调、批量面板完整 | 下拉选择直接高效 | 保留 M-GCS macOS 风格，不复制 QML/WinForms 布局 |

---

## 三、目标架构

```text
Endpoint / Serial / TCP / UDP
              |
              v
        LinkSession
   transport + parser + peers
              |
              v
       MavlinkRouter
   one parse, route by identity
              |
      +-------+--------+
      |                |
 VehicleSession A  VehicleSession B
 (sysid=1)         (sysid=2)
      |                |
      +-------+--------+
              |
       FleetManager
 activeVehicle + selectedVehicles
              |
              v
       FFI snapshot/events
              |
              v
        Flutter FleetModel
```

### 1. LinkSession

职责：

- 拥有底层 Serial/TCP/UDP 传输。
- 维护链路连接状态、远端 peer、链路质量和 GCS 心跳。
- 对收到的字节流只解析一次 MAVLink。
- 允许绑定零个或多个 `VehicleSession`。

约束：

- 持有链路锁时不能进行阻塞发送或调用外部回调。
- UDP 必须支持多个远端 peer，不能只保存第一个发送方。
- 关闭链路时先停止读线程，再释放 transport 和解析器。

### 2. MavlinkRouter

职责：

- 按 `(sysid, compid)` 把完整 MAVLink 消息路由到车辆。
- 在心跳到达时参与车辆发现。
- 过滤 GCS、云台、ADSB、`sysid=0` 等非飞控心跳。
- 保证命令 ACK 只能匹配对应目标车辆。

风险点：

- 已有适配器使用 caller-owned RX 状态；共享链路新增 Router 后，每个 Vehicle 的
  parser、TX sequence 和 signing state 已实例隔离；UDP、TCP loopback 与 POSIX PTY
  均已接入本地多 sysid 回归。
- 当前遥测缓存不按 `sysid` 分区，必须先解复用再更新状态。

### 3. VehicleSession

职责：

- 保存稳定 `vehicle_id`、`system_id`、`component_id`。
- 关联一条或多条 `LinkSession`。
- 拥有独立遥测、参数、任务、消息、校准和波形状态。
- 所有发送命令携带明确的 `sysid/compid`。

### 4. FleetManager

职责：

- 车辆注册、移除、查询和排序。
- 维护 `activeVehicle`。
- 维护 `selectedVehicles`。
- 当活动车辆被移除时，按确定性顺序选择替代者。
- 对外发布车辆新增、移除、状态变化和焦点变化事件。

### 5. Flutter FleetModel

职责：

- 保存车辆摘要、活跃车辆 ID 和选中车辆集合。
- 活跃车辆变化时重新绑定遥测、参数、任务和消息后端。
- 切换前取消旧车辆的长操作和待确认飞行命令。
- 单机时隐藏车辆选择器，保持当前界面不变。

---

## 四、阶段计划

| 阶段 | 目标 | 主要工作 | 完成标准 |
| --- | --- | --- | --- |
| MV0. 契约与基线 | 冻结范围、字段和兼容边界 | 完成本计划；明确 FFI 字段、生命周期和验收测试；更新路线图 | 计划评审通过；不修改公开 ABI；实现切片的依赖顺序明确 |
| MV1. 多链路单机 | 先支持多个 endpoint，每个 endpoint 一架飞机 | Flutter FleetModel；连接面板增加链路列表；支持同时打开多个 session；顶部状态栏增加车辆选择器；切换时重绑状态 | 两个 UDP 端点连接两架 SITL；可切换 active；位置、模式、参数和任务不串车；单机流程无变化 |
| MV2. 共享链路解复用 | 一个 endpoint 发现多架飞控 | Core 引入 `LinkSession` 与 `MavlinkRouter`；MAVLink 只解析一次；按 `sysid/compid` 分发；UDP 支持多个 peer | 同一 UDP 端口上的 sysid=1/2 各自更新状态；跨车辆 ACK 和参数数据不能互相匹配 |
| MV3. 生命周期与多链路 | 心跳发现、合并、失效和移除完整闭环 | 合格心跳过滤；同机多链路合并；单链路失效只降低 health；全部链路失效才移除；活动车辆删除后选择替代者 | 断一机不影响另一机；重连不重复创建；同机双链路正确合并；无效心跳不生成车辆 |
| MV4. 前端状态隔离 | 切换 active 不留下旧车数据 | 参数、任务、消息、波形和操作进度按车辆隔离；切机取消旧车长操作；清空待确认命令；任务文档重新绑定 | 连续切换车辆后无旧 Home、旧参数、旧消息或旧任务；待确认命令不会发到新 active |
| MV5. 多机 UI 与地图 | 让 operator 清楚看到 fleet | Fleet 快照 FFI；地图显示所有飞机；active 高亮、非 active 降强调；车辆选择菜单显示模式、armed、链路和电量 | 地图无闪烁；active 明确；非 active 默认不绘制复杂任务层；窗口尺寸变化不遮挡 |
| MV6. 多选与批量动作 | 建立独立于 active 的批量操作 | `selectedVehicles` UI；选择/取消；批量解锁、返航、开始任务；逐机结果汇总 | 批量确认显示飞机数量与 sysid；拒绝、超时和失败逐机展示；不允许把部分成功显示为整体成功 |

---

## 五、FFI 契约计划

### 保留现有接口

- `mg_connection_open`
- `mg_connection_close`
- `mg_connection_get_session_state`
- `mg_vehicle_list`
- `mg_vehicle_select`
- `mg_vehicle_get_active`

不删除已导出的函数。兼容入口不能获得新的矛盾语义。

### 建议扩展

`mg_vehicle_list` 的单车对象增加：

- `vehicle_id`
- `system_id`
- `component_id`
- `firmware_type`
- `vehicle_type`
- `name`
- `endpoint`
- `connection_state`
- `connected`
- `heartbeat_age_ms`
- `link_count`
- `is_active`
- `is_selected`

建议增加事件：

- 车辆列表变化
- active vehicle 变化
- selected vehicles 变化

事件属于 FFI 契约变更，必须同时更新头文件、实现、测试、`API_CATALOG.md` 和本文档。

### 批量命令边界

MV6 已完成批量命令入口。批量接口必须显式携带冻结的目标车辆 ID 列表，禁止前端调用
一个隐式“发给所有飞机”的 API；每架飞机的 ACK、拒绝、超时和状态确认结果必须单独保留。

---

## 六、UI 计划

### 顶部状态栏

- 只有车辆数大于 1 时显示车辆选择器。
- 单机时保持当前布局。
- 菜单项建议显示：`飞机 2 · 系统 ID 2 · UDP`。
- 次级状态显示飞行模式、在线状态、armed 和电量。
- 切换 active 使用单击，不做长按确认，因为切换本身不下发飞行命令。

### 连接面板

- 从单一连接表单改成“当前链路列表 + 新建链路”。
- 每条链路显示 transport、endpoint、连接状态和发现的车辆数。
- 断开某条链路不能误关其他链路。
- 发现多架飞控时，不把新车辆自动设为 active；首架仍可采用现有首机默认行为。

### 地图

- 多机地图第一版已显示所有车辆位置和航向。
- active 车辆显示主任务和 Home，非 active 车辆使用弱化图层；复杂任务图层仍需后续补齐。
- active 飞机使用现有强调色；其他飞机使用弱化色。
- 点击非 active 飞机时先选择车辆，不直接执行动作。
- 所有安全命令仍只作用于 active 或明确选中的车辆集合。

---

## 七、安全与命令路由规则

1. 切换 active vehicle 时，取消当前待长按确认的命令；返回旧飞机不恢复旧凭证。
2. 切换 active vehicle 时，长操作必须隔离；没有安全取消语义时阻止切换，而不是静默终止飞控任务或校准。
3. 任何飞行命令执行前，记录目标 `vehicle_id + sysid + compid`。
4. 命令 ACK 必须匹配目标系统、组件与命令上下文，不能让另一架飞机的 ACK 完成任务。
5. 断链必须传播到该车辆的命令、任务、参数和校准状态。
6. 批量操作必须提前显示目标飞机数量和系统 ID。
7. 批量结果按飞机分别报告成功、拒绝、超时和失败。
8. 单机连接不得增加额外的选择或确认步骤。

---

## 八、验收测试矩阵

| 编号 | 场景 | 预期结果 |
| --- | --- | --- |
| MV-T1 | 两个 UDP endpoint，各自一架 SITL | 列表显示两架；可独立连接和断开 |
| MV-T2 | 同一 UDP 链路的 sysid=1/2 | 两个 VehicleSession 状态互不覆盖 |
| MV-T3 | 同一架飞机同时通过 USB 和遥测电台出现 | 合并为一架，显示两条链路 |
| MV-T4 | GCS、云台、ADSB、sysid=0 心跳 | 不创建飞机条目 |
| MV-T5 | active=1 时命令由 sysid=2 返回 ACK | 命令 1 不能误判成功 |
| MV-T6 | active 飞机断链 | 自动选择剩余飞机；旧状态和待确认命令被清理 |
| MV-T7 | 连续切换两架飞机 | 遥测、Home、参数、任务、消息和波形全部切换正确 |
| MV-T8 | 单机连接回归 | 不显示多机选择器；现有连接和飞行工作流不增加步骤 |
| MV-T9 | 批量操作一架拒绝、一架成功 | 结果逐机显示，不显示为整体成功 |
| MV-T10 | 窗口尺寸变化 | 多机 UI、地图标记、遥测和面板不互相覆盖 |

---

## 九、实施顺序与依赖

推荐顺序：

1. MV0：计划和契约。
2. MV1：多链路单机，建立 Flutter FleetModel。
3. MV2：共享链路解复用。
4. MV3：生命周期和多链路关联。
5. MV4：状态隔离和安全切换。
6. MV5：多机 UI 和地图。
7. MV6：多选和批量动作。

硬依赖：

- UDP、TCP loopback 和 POSIX PTY 的多 sysid 共享链路均已通过聚焦回归；真实飞控总线仍需现场验收。
- MV4 后端取消/超时隔离已完成；前端交互和现场验收通过后再扩大批量操作范围。
- MV5 多机地图不能先于 MV2 的车辆状态隔离。
- MV6 不能复用 active vehicle 作为隐式批量目标。

---

## 十、主要风险

| 风险 | 影响 | 处理方式 |
| --- | --- | --- |
| 当前 UDP 只记住一个远端 | 多个 UDP 发送方无法稳定通信 | LinkSession 维护 peer 表，按 datagram 来源和目标 sysid 路由 |
| 当前 MAVLink 解析无 sysid 分区 | 多机数据互相覆盖 | 解析一次后统一进入 MavlinkRouter，按身份分发 |
| Flutter 当前固定单 session | 无法表达 fleet | 引入 FleetModel，单机模式仍沿用现有界面 |
| 长操作跨车辆残留 | 任务或参数可能发错飞机 | 切机时取消旧操作，命令绑定明确 vehicle_id |
| ACK 匹配不严 | 另一架飞机的 ACK 可能误结束命令 | 命令队列同时匹配 sysid/compid 和命令上下文 |
| 多机地图导致性能下降 | 高刷新率下卡顿 | 第一版只绘制位置/航向；任务、Home 和轨迹默认只画 active |
| UI 过早复杂化 | 单机用户被多机功能干扰 | 单机隐藏车辆选择器，多机能力按需出现 |

---

## 十一、需要后续确认的产品决策

1. 多机是否进入当前 `0.1.0` 范围，还是作为独立版本。
2. 第一版是否要求“一机多链路自动主备切换”，还是只做多链路状态合并。
3. 批量操作第一版包含哪些动作，建议只做解锁、返航、开始任务。
4. 非 `AUTOPILOT1` 组件是否需要在高级诊断页显示。
5. 多机地图默认显示全部车辆还是只显示 active 与选中车辆。

在这些决策确认前，可以完成 MV0、MV1 的协议与 UI 基础工作，但不能提前承诺蜂群或自动任务分配。

---

## 十二、关联文档

- [多机关联研究](../research/MULTIVEHICLE_CONNECTION_RESEARCH.md)
- [路线图与功能差距分析](ROADMAP.md)
- [路线图与功能差距分析](ROADMAP.md)
- [上游实现选择策略](../specs/upstream/SELECTION_STRATEGY.md)
- [架构说明](../architecture/ARCHITECTURE.md)
- [FFI API 清单](../../bridge/ffi/API_CATALOG.md)

---

## 十、与 QGC / MP 的架构对比

参考实现：QGroundControl 4.x、Mission Planner 1.3.x、本地 checkout 见
[关联研究](../research/MULTIVEHICLE_CONNECTION_RESEARCH.md)。

### QGC：链路与车辆解耦（M-GCS 主要参考结构）

- 链路先于车辆存在；心跳合格后才创建 Vehicle。
- 同一 sysid 可由多条 Link 报告，合并到一个 Vehicle（`VehicleLinkManager`）。
- primary/secondary 失效自动切换；通信丢失分“单链路丢失 / 全链路丢失”。
- 忽略列表避免手动关闭后立即重发现。

### MP：单链路单机 + ArduPilot 语义（M-GCS 主要参考行为）

- 一个 `MAVLinkInterface` 对应一个 sysid/compid；多机靠多个实例。
- 无共享链路，UDP 多机需多个 socket。
- 命令 API 显式携带 sysid/compid，ArduPilot 现场语义成熟。

### M-GCS：QGC 结构 + MP 单机稳定性

```cpp
LinkSession {                 // 传输 + 单次解析 + peer
  MavlinkRouter               // 按 (sysid, compid) 路由
}
VehicleSession {              // 稳定 vehicle_id + 独立子系统
  // 遥测/参数/任务/消息/校准/波形/signing 全部实例隔离
}
FleetManager {                // activeVehicle + selectedVehicles
}
```

### 能力对标

| 能力维度 | QGC 4.x | MP 1.3.x | M-GCS 0.0.80 |
| --- | :---: | :---: | --- |
| 共享 UDP 多机解复用 | ✓ | ✗ | ✓ |
| 串口/TCP 共享链路 | ✓ | ✗ | ✓（本地回归，现场待补） |
| 同机多链路合并 | ✓ | ✗ | ✓ route 归并 |
| 心跳自动发现 | ✓ | ✓ | ✓ |
| 链路质量跟踪 | ✓ | 基础 | ✓ route health/primary |
| 通信丢失检测 | ✓ 分级 | ✓ | 部分（需细化分级） |
| 忽略列表 | ✓ | ✗ | 短暂抑制（30 秒） |
| 按车辆状态隔离 | ✓ | ✓ | ✓ |
| 异步操作取消 | ✓ | 部分 | 部分（按 Vehicle 已支持） |
| 任务文档隔离 | ✓ | ✓ | ✓ |
| 地图显示多机 | ✓ | ✓ | ✓ 基础图层 |
| 批量选择 | ✓ | ✗ | ✓ `selectedVehicles` |
| 批量命令 | ✓ | ✗ | ✓ 解锁/上锁/返航/开始任务 |

**总体：已对齐 QGC 的共享链路分发结构，并保留 MP 的单机稳定性。** 剩余差距集中在
ComplexItem 语义编辑、系统 UID/固件产品取舍和真实飞控总线验收。

---

## 十一、验证证据

最近一次本地验证结果：

| 项目 | 结果 |
| --- | --- |
| 后端 `ctest offline|simulation` | 37/37 通过 |
| Flutter analyze | 通过 |
| Flutter 常规测试 | 123 个通过（另有 4 个真实 FFI 测试按环境变量跳过） |
| 真实 Fleet FFI UDP 测试 | 4 个通过，默认无 `MGCS_FLEET_LIBRARY` 时跳过 |
| Python 双 UDP smoke | 2 轮通过 |
| macOS debug build | 通过 |
| UI token scan | 通过 |
| `git diff --check` | 通过 |

真实 UDP 测试覆盖：

- 两个独立 UDP endpoint，各自一架飞机。
- 同一 UDP endpoint 上 sysid 1/2 各自形成独立 Vehicle。
- 关闭共享 endpoint 中一架后，另一架继续接收和切换；最后一架关闭后才关闭共享 socket。
- `MAG_CAL_PROGRESS` ArduPilot 兼容帧仍能到达目标适配器。
- A → B → A 后旧命令确认凭证失效。
- 批量 `mission_start` 显式目标分派；候选密钥自动探测已签名心跳。

运行真实 FFI 测试：

```bash
cd flutter_shell
MGCS_FLEET_LIBRARY="$PWD/../build/mgcore.dylib" \
  flutter test --no-pub test/fleet_ffi_smoke_test.dart
```

---

## 十二、尚未实现或仍不完整的部分

### MV3 生命周期剩余

- 永久忽略 / 显式重连的产品策略仍未定（当前只做 30 秒短暂抑制）。
- 真实串口总线和多机现场验收未完成（UDP/TCP loopback/PTY 已有本地回归）。
- 系统 UID 是否作为车辆身份主键、UID 冲突如何处理，待产品确认。

### MV4 异步操作剩余

- 后端取消/超时边界已完成；旧任务面板、对话框、地图交互的最终形态和真实飞控
  总线验收保留给用户评估。

### MV5 多机地图剩余

- active 地图已接入围栏多边形、圆形围栏、越界返航点和 Rally 点；非 active 只保留
  弱化任务路径。
- Survey / Corridor 等 QGC 复杂项仍需要进一步的编辑与分层控制。

### 产品取舍（阻塞，非技术）

- `DEAD-001` 固件刷写：实现完整流程，或明确标为 out of scope。
- `PARAM-002` 参数元数据：需先确定覆盖率阈值才能判定完成。

---

## 十三、需要审核确认的事项

1. **UDP 行为变化**
   - `mg_connection_open("udp://...")` 现在返回物理 link handle，心跳到达后才创建 Vehicle。
   - 外部旧调用方需要改用 `mg_connection_list()` 读取链路与车辆映射。

2. **一机多链路身份规则**
   - 当前 managed endpoint 按 `sysid/compid` 合并到同一 Vehicle。
   - 是否进一步使用系统 UID 作为身份主键，仍需产品确认。

3. **手动关闭后的重发现策略**
   - 用户主动关闭一架飞机后，同一 sysid 继续发心跳时是否应自动重新加入？
   - QGC 用忽略列表避免立即复活；当前 M-GCS 只做关闭过程中的短暂抑制。
