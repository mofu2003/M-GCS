# 多机连接与焦点管理实施计划

状态：MV1 多链路单机框架与 UI 已落地；MV2 的 UDP 共享链路已完成；MV3 的链路句柄、自动归并、route health、精确关闭、重发现抑制和 primary 命令源切换已落地；MV4 状态隔离部分完成；MV5 第一版多机地图已接入
规划日期：2026-10-08
适用范围：多链路、多飞控发现、活跃飞控切换、分机状态隔离和多机地图
暂不包含：编队算法、蜂群任务分配和自动协同

> 当前实际实现和审核边界见 [多机框架当前状态审核](MULTI_VEHICLE_STATUS_REVIEW.md)。
>
> 本文补充 [多机关联研究](../research/MULTIVEHICLE_CONNECTION_RESEARCH.md)。
> 研究文档回答“QGC 和 MP 怎么做”，本文回答“M-GCS 按什么顺序做、取哪些优点、
> 每一阶段如何验收”。

---

## 实施进度（2026-10-08）

本轮已完成 MV1 多链路单机闭环，并将 UI 入口开放；UDP 共享链路已接通，
串口/TCP 本地共享链路验证已接通；多机地图和批量控制仍未交付：

- `flutter_shell/lib/fleet/fleet_models.dart`：独立连接配置、不可变车辆摘要/快照、
  带焦点 revision 的命令目标凭证。session ID 与 MAVLink system ID 明确区分。
- `fleet_backend.dart`：连接、列表、精确关闭和选择的可测试接口。
- `fleet_ffi_backend.dart`：复用现有 C ABI；新增链路/route 读取与精确关闭适配。
- `fleet_controller.dart`：首链路选择、后续链路不抢焦点、重复端点拒绝、
  精确关闭、焦点失效和异步确认的目标保护；测试用内存 backend 与 FFI 共用接口。
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
- 新增 `mg_command_execute_for_vehicle()` 显式目标分派；native 以线程局部覆盖
  选择目标车辆，不调用 `mg_vehicle_select()`，所以命令执行期间 UI active focus
  不会被临时改写。Flutter 飞行命令全部改走该接口，旧 `mg_command_*` 保留兼容。
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

### 本轮验证与尚未开放的能力

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
  MAVLink signing 适配器核心、单机 FFI、命名密钥库本地持久化、设置页和 SETUP_SIGNING 下发已完成；取消旧飞控长操作、多机地图完整图层和批量控制仍未全部完成。每个 `MavlinkAdapter` 的 TX sequence（含 FTP 发送）及 signing 状态已实例隔离，自动探测待补；SETUP_SIGNING 已接通。
  分机状态隔离的取消/超时边界继续按 MV4 验收。
- legacy `mg_command_*` 仍依赖 active focus，但 Flutter 已统一使用
  `mg_command_execute_for_vehicle()`；参数、任务和遥测读取继续通过当前 native
  active focus，并在 UI 层用 session/焦点校验和长操作门控隔离。

### 下一切片的进入条件

MV1 已完成多链路独立 session、车辆选择器、链路列表、精确断开和按 session 的
运行时/任务文档隔离；MV2 已完成 UDP 共享 endpoint 的单次解析、多 sysid
Vehicle 发现和 peer-aware 回复；MV3 已补同机多链路合并、route 级精确关闭和链路句柄模型。下一步是完成 MAVLink 自动探测和异步操作隔离，再进行多机地图与批量动作。参数写入/任务传输/校准执行中仍阻止焦点切换；
没有确认协议取消语义前，不自动取消飞行中的任务或正在执行的校准。批量动作严格后置。

本轮构建/检查：`flutter analyze lib test`、UI token 扫描、macOS debug 构建通过；
后端 offline/simulation 37/37 通过；111 个常规 Flutter 测试通过，实际 Fleet FFI 的
三个 UDP 测试由 `MGCS_FLEET_LIBRARY` 单独启用后通过，覆盖两个独立端口和同一端口
sysid 1/2。运行实际 FFI 测试：

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

MV6 之前不增加批量命令入口。批量接口必须显式携带目标车辆 ID 列表，禁止前端调用一个隐式“发给所有飞机”的 API。

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

- 第一版仍只显示 active 车辆的主任务和 Home。
- 多机地图阶段显示所有车辆位置和航向。
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
- MV4 未完成前，不开放批量飞行命令。
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
- [开发路线图](DEVELOPMENT_ROADMAP.md)
- [功能差距分析](FEATURE_GAP_ANALYSIS.md)
- [上游实现选择策略](../specs/upstream/SELECTION_STRATEGY.md)
- [架构说明](../architecture/ARCHITECTURE.md)
- [FFI API 清单](../../bridge/ffi/API_CATALOG.md)
