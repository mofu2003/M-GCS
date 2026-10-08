# 多机框架当前状态审核

> 审核目的：说明 M-GCS 多机框架当前实际实现到哪一步，区分“已实现”“部分实现”“仅规划”，并列出需要人工决策的边界。
>
> 审核日期：2026-10-08
> 基线版本：`0.0.79`
> 基线提交：`fa480f5`
> 当前分支：`main`
> 本轮提交：`17e179d`（`main`，已推送远端）。

---

## 一、结论摘要

| 阶段 | 状态 | 结论 |
| --- | --- | --- |
| MV0 契约与基线 | 已完成 | 已有研究文档、实施计划和验收矩阵 |
| MV1 多链路单机 | 已完成 | 多 endpoint、链路列表、active 切换、详细状态重绑定和基本 UI 已接通 |
| MV2 共享链路解复用 | 已完成（本地验证） | 同一 endpoint 使用 `LinkSession`/`MavlinkRouter` 按 `sysid/compid` 发现多架飞机；UDP 有真实 socket 验证，TCP loopback 和 POSIX PTY 多 sysid 回归已通过 |
| MV3 生命周期与多链路 | 已完成（UDP/TCP/serial 通用框架） | 独立 link handle、同 sysid 跨 endpoint 自动归并、per-route health/primary、route 精确关闭、30 秒重发现抑制、Vehicle 多 route 遥测汇聚和 FFI/UI 链路模型已接通 |
| MV4 前端状态隔离 | 部分完成 | 主要状态按 session 保存/恢复，任务文档按 session 隔离；取消/超时和所有异步操作隔离仍待补 |
| MV5 多机 UI 与地图 | 部分完成 | Fleet 快照已携带每架飞机的位置，地图显示非 active 飞机弱化标记并支持点击切机；任务、模式和完整电池/健康信息仍未接入 |
| MV6 多选与批量动作 | 未开始 | 没有 `selectedVehicles` UI，没有批量飞行命令 |

一句话结论：

> 目前已完成多端点、多 route、同机多链路归并、UDP/TCP loopback/POSIX PTY 多机闭环，以及 signing 密钥库、FFI、SETUP_SIGNING；剩余主要是自动密钥探测、异步操作隔离、MV5 完整图层和 MV6 批量控制。

---

## 二、当前数据流

### 2.1 独立 endpoint 多链路

```text
Flutter FleetController
        |
        | FleetFfiBackend
        v
mg_connection_open() / mg_vehicle_list()
        |
MultiVehicleManager
        |
        +--> Vehicle A --> ConnectionManager A --> UDP/TCP/Serial Link A
        +--> Vehicle B --> ConnectionManager B --> UDP/TCP/Serial Link B
```

一个 endpoint 对应一个 Vehicle session；active 只是 UI 焦点，不会临时改写 native 车辆。

### 2.2 UDP 共享 endpoint 多机

```text
UDP socket / LinkSession
        |
MavlinkRouter（单次解析）
        |
        +--> sysid=1, compid=1 --> MavlinkAdapter A --> Vehicle A
        +--> sysid=2, compid=1 --> MavlinkAdapter B --> Vehicle B
```

每个 Vehicle 仍有独立的：

- 遥测缓存和连接状态。
- 参数、任务、消息、校准状态。
- 飞行命令队列和 ACK 匹配上下文。
- UDP peer 记录，回复使用对应 sender。

---

## 三、已完成部分

### 3.1 Core / FFI

| 能力 | 当前实现 | 关键入口 |
| --- | --- | --- |
| 独立 session | 多 endpoint 多 Vehicle | `MultiVehicleManager::open_vehicle()` |
| 共享 UDP session | 一个 socket 可发现多个 autopilot sysid | `MultiVehicleManager::open_shared_vehicle()` |
| MAVLink 解复用 | 一次解析，按 `sysid/compid` 路由 | `MavlinkRouter`、`LinkSession` |
| 分片/粘连帧 | 单链路 parser state，支持跨 chunk 帧 | `MavlinkRouter::route_bytes()` |
| 重复身份保护 | 同一 `(sysid, compid)` 不能绑定到两个 route | `MavlinkRouter::add_target()` |
| 未知心跳发现 | 无目标心跳触发 Vehicle 创建/占位绑定 | `on_shared_heartbeat()` |
| peer-aware UDP 回复 | 按每个 route 学到的 sender 发回 | `ILink::send_to()`、`UdpLink::send_to()` |
| 精确关闭 | 关闭单架不关闭仍在使用的共享 socket | `MultiVehicleManager::close_vehicle()` |
| 注册表生命周期 | `mg_vehicle_list` 使用 shared_ptr 快照 | `get_all_vehicles_shared()` |
| 显式目标命令 | 命令携带 vehicle ID，不依赖临时切换 active | `mg_command_execute_for_vehicle()` |
| 分机任务文档 | native 按 session 保存独立 `MissionDocumentService` | `ffi_mission_shared.cc` |

### 3.2 Flutter 侧

| 能力 | 当前实现 | 关键入口 |
| --- | --- | --- |
| Fleet 模型 | 不可变车辆摘要、active ID、目标 revision | `flutter_shell/lib/fleet/` |
| 链路列表 | 显示 endpoint、transport、状态、sysid | `connection_panel.dart` |
| 精确断链 | 面板可断开指定链路 | `ConnectionController::disconnectLink()` |
| 顶部车辆选择器 | 多机时显示，单机隐藏 | `mg_top_status_bar.dart` |
| active 切换 | 单击切换，不触发飞行命令 | `FleetController::selectVehicle()` |
| 命令目标保护 | 切机、断链、A→B→A 会失效确认凭证 | `FleetTarget` |
| 待确认命令 | 切机时取消当前长按确认 | `main.dart` |
| 运行时快照 | 按 session 保存/恢复遥测、参数、消息、Home、GoTo、飞行统计 | `connection_controller.dart` |

### 3.3 MV3 本轮新增

- 新增 `VehicleLinkManager`：
  - 管理一个 Vehicle 的多个 route 状态模型。
  - 记录每个 route 的最近心跳时间。
  - 计算 `comm_lost` 和总通信丢失状态。
  - 在健康链路中选择 primary route。
- `Vehicle` 当前单 route 的 heartbeat 已上报到 link manager；GCS 心跳循环每秒执行 health tick。
- `MultiVehicleManager` 手动关闭共享车辆时可写入 30 秒重发现抑制；超时后允许该 sysid 重新出现。
- `Vehicle` 核心层新增多条 shared route 挂接：
  - 第二条 `LinkSession` 可以绑定到同一个 Vehicle。
  - 第二条链路的 MAVLink 回调汇聚到同一 Vehicle 缓存和事件流。
  - 每条 route 仍保留独立 parser、连接状态和 health。
  - `conn()`、校准和飞行模式会随 `VehicleLinkManager` 的 primary route 切换。
- `MultiVehicleManager` 新增显式 `open_shared_link()`、`managed_links()`、
  `close_link()` 和 `close_route()`：`mg_connection_open()` 返回物理 link handle，
  同一 sysid 在不同 endpoint 会自动归并到同一个 Vehicle；关闭一个 route 不会
  关闭仍有其他飞机的共享 socket。
- FFI 新增 `mg_connection_list()` 和 `mg_connection_close_route()`；
  managed-link JSON 包含 `link_id`、`route_id`、`vehicle_id`、`primary`。
- Flutter Fleet 快照区分 link 与 vehicle，连接面板按 route 展示并精确关闭；
  waiting link 在心跳到达前保持可见，不把 link handle 当作 vehicle ID。
- 真实/内存测试已覆盖：
  - 一条链路丢失、剩余链路接管 primary。
  - 全部链路丢失。
  - 新链路恢复。
  - 手动关闭后窗口内不复活、窗口外可重发现。
  - 同一 Vehicle 的第二条 shared route 遥测合并。
  - 同一 sysid 跨 endpoint 归并，以及共享 socket 中单 route 精确关闭。
  - 多个 adapter 各自从独立 TX sequence 开始，互不推进发送序号；FTP 发送也复用 adapter 的实例私有发送器。

### 3.4 MV4 已提前落地的状态隔离

当前 `ConnectionController` 切换 session 时会处理：

- 遥测和 GPS/姿态/Home/位置状态。
- 参数字典、参数写错误和元数据状态。
- 飞控消息列表和消息游标。
- 校准状态和错误。
- GoTo 显示目标及模式过期状态。
- 飞行时间和飞行距离统计。
- 波形实例选择偏好。
- serial hardware mapping 和 firmware request 标志。

任务文档方面：

- Flutter 为每个 session 保留一个 `MissionDocumentController`。
- native core 为每个 session 保留独立任务文档。
- 旧 session 的异步任务回调通过 owner/focus 校验拒绝写入当前飞机。
- 离线任务草稿在首次连接时复制给首架飞机，之后不共享 revision/undo 历史。

---

## 四、验证证据

最近一次本地验证结果：

| 项目 | 结果 |
| --- | --- |
| 后端 `ctest offline|simulation` | 37/37 通过 |
| Flutter analyze | 通过 |
| Flutter 常规测试 | 111 个通过 |
| 真实 Fleet FFI UDP 测试 | 3 个通过，默认无 `MGCS_FLEET_LIBRARY` 时跳过 |
| Python 双 UDP smoke | 2 轮通过 |
| macOS debug build | 通过 |
| UI token scan | 通过 |
| `git diff --check` | 通过 |

真实 UDP 测试覆盖：

- 两个独立 UDP endpoint，各自一架飞机。
- 同一 UDP endpoint 上 sysid 1/2 各自形成独立 Vehicle。
- 关闭共享 endpoint 中一架后，另一架继续接收和切换。
- 最后一架关闭后才关闭共享 socket。
- `MAG_CAL_PROGRESS` ArduPilot 兼容帧仍能到达目标适配器。
- A → B → A 后旧命令确认凭证失效。

---

## 五、尚未实现或仍不完整的部分

### 5.1 MV3 生命周期

已完成的 MV3 核心：

- 同一架飞机通过不同 managed endpoint 出现时，按 `sysid/compid` 归并为一个 Vehicle。
- 每条 route 保留独立 heartbeat、`comm_lost` 和连接状态，单链路丢失只降低该 route health。
- `primary_route_id` 在主链路失效后自动切换到健康 route；`conn()`、校准和飞行模式连接跟随 primary。
- 关闭 route、关闭 link 和关闭 vehicle 的边界已分离；共享 socket 在仍有 route 时保持打开。
- `mg_connection_open()` 返回物理 link handle，`mg_connection_list()` 返回 link → route → vehicle 映射。
- 关闭后 30 秒内抑制同一 endpoint/sysid 的自动重发现。
- MAVLink 适配器已有实例级 signing 密钥、时间戳、stream 和入站签名校验。

仍待补的是协议和产品边界：

- 永久忽略/显式重连的产品策略。
- MAVLink 发送序列号和 signing 状态已按 `MavlinkAdapter` 实例隔离，FFI 可配置/清除/查询单机 signing；Flutter 已有命名密钥库、活动密钥选择、本地持久化、自动应用和地面站配置页。自动探测仍待补；`SETUP_SIGNING` 已通过 FFI 和设置页下发。
- 所有异步操作按 route/vehicle 取消和超时收口。

当前多机能力已不再依赖“一个 endpoint 必须绑定一架飞机”的旧假设。

### 5.2 MV4 异步操作隔离

已经按 session 隔离主要状态，但仍需补齐：

- 参数下载和长任务取消语义。
- 所有异步任务在切机时的 generation/session 校验。
- 旧任务面板、对话框和地图回调的全面重绑定测试。
- 校准、日志、固件和参数操作的完整跨车辆竞态测试。
- 波形历史缓冲和飞行统计的语义边界。

因此当前可以支持 UI 切机，但不能把 MV4 标记为完整交付。

### 5.3 MV5 多机地图

已完成第一版：

- `mg_vehicle_get_session_status` 增加 WGS-84 经纬度、海拔、相对高度和位置有效性。
- Fleet 快照携带每架飞机的位置、航向和 position validity。
- 地图为非 active 飞机绘制弱化颜色标记，支持点击切机。
- active 飞机仍使用 20Hz 主遥测位置，非 active 飞机使用 Fleet 低频快照。

仍未完成：

- 非 active 飞机不显示任务、Home 和完整链路/电池/模式摘要。
- 地图没有车辆列表、批量选择和地图点击后的批量操作。

### 5.4 MV6 批量动作

未实现：

- `selectedVehicles` 独立选择集合。
- 批量解锁、返航、开始任务。
- 逐机结果、拒绝、超时和失败汇总。

---

## 六、需要审核确认的事项

建议重点审核以下产品/架构边界：

1. **UDP 行为变化**
   - `mg_connection_open("udp://...")` 现在返回物理 link handle，心跳到达后才创建 Vehicle。
   - 连接面板已按 waiting link / discovered route 区分显示；外部旧调用方需要改用 `mg_connection_list()` 读取链路和车辆映射。

2. **一机多链路身份规则**
   - 当前 managed endpoint 按 `sysid/compid` 合并到同一 Vehicle，可覆盖 USB 与遥测电台同时出现的基础场景。
   - 是否进一步使用系统 UID 作为身份主键、以及 UID 冲突时如何处理，仍需产品确认。

3. **手动关闭后的重发现策略**
   - 用户主动关闭一架飞机后，同一 sysid 继续发心跳时是否应自动重新加入？
   - QGC 使用忽略列表避免立即复活，当前 M-GCS 只做了关闭过程中的短暂抑制。

4. **串口/TCP 的共享 LinkSession 边界**
   - 当前实现已统一使用通用 `LinkSession`，没有 UDP peer 的路由会退化为同一发送通道。
   - TCP loopback 和 POSIX PTY 的多 sysid 发现/关闭回归已通过；真实飞控总线和
     串口设备仍需现场验收。

5. **提交和发布边界**
   - 本轮改动提交到 `main` 并推送；是否立即升版本号/打标签由发布节奏决定。
   - 自动探测、MV4 异步隔离、MV5 完整地图和 MV6 批量控制仍保留为后续切片。

---

## 七、建议下一阶段顺序

1. 完成 MAVLink 自动密钥探测。
2. 完成全部异步操作的 route/vehicle 取消、超时和跨车辆竞态收口。
3. 继续 MV5 多机地图的任务图层、模式与健康信息。
4. 最后完成 MV6 多选、批量确认和逐机结果汇总。

---

## 八、主要代码与测试索引

- `core/include/core/mavlink/mavlink_router.h`
- `core/src/mavlink/mavlink_router.cc`
- `core/include/core/transport/link_session.h`
- `core/src/transport/link_session.cc`
- `core/include/core/transport/link_interface.h`
- `core/src/transport/udp_link.cc`
- `core/src/transport/connection_manager.cc`
- `core/src/vehicle/multi_vehicle_manager.cc`
- `core/src/vehicle/vehicle.cc`
- `core/include/core/vehicle/vehicle_link_manager.h`
- `core/src/vehicle/vehicle_link_manager.cc`
- `bridge/ffi/ffi_vehicle_manager.cc`
- `bridge/ffi/ffi_flight_commands.cc`
- `flutter_shell/lib/fleet/`
- `flutter_shell/lib/connection/connection_controller.dart`
- `flutter_shell/lib/connection_panel.dart`
- `flutter_shell/lib/widgets/mg_top_status_bar.dart`
- `core/tests/test_mavlink_router.cc`
- `core/tests/test_bridge_vehicle_session.cc`
- `flutter_shell/test/fleet_controller_test.dart`
- `flutter_shell/test/fleet_ffi_smoke_test.dart`
- `flutter_shell/test/connection_panel_test.dart`

关联文档：

- [多机连接实施计划](MULTI_VEHICLE_PLAN.md)
- [多机连接研究](../research/MULTIVEHICLE_CONNECTION_RESEARCH.md)
- [当前状态审计](STATUS_AUDIT.md)
- [架构说明](../architecture/ARCHITECTURE.md)
- [FFI API 清单](../../bridge/ffi/API_CATALOG.md)
