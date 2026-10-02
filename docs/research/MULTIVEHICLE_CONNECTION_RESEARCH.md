# 多机管理与单机连接：QGC、Mission Planner 源码对照

研究日期：2026-09-23

> 本文是连接模型的研究快照，源码行号和上游 commit 只对应当时的本地源码；当前产品
> 边界以 [架构说明](../ARCHITECTURE.md)、[开发约束](../CONSTRAINTS.md) 和状态审计为准。

本文只比较连接、飞控身份/焦点、命令路由和后续 fleet/swarm 边界。源码行号对应本地只读源码快照：QGroundControl `a89def242`（2026-07-20，`fix(Vehicle): clear communicationLost as soon as any link regains comms`）；Mission Planner `a2fcd74d6`（2026-07-11，`MAVLinkInterface: null exception fix ...`）；M-GCS `371c594`（2026-09-23，`feat: separate vehicle connection from active focus`）。行号会随上游版本变化。

## 结论

用户层面可以、也应该把普通单机连接与未来的编队/蜂群功能分开。连接页只负责选择端口/地址、打开链路、等待心跳、显示连接状态和断开；不应要求用户进入多机管理、选择 fleet 或设置 active vehicle 才能连接、查看或控制单架飞机。

但“多机功能独立”不等于底层不能有会话注册/飞控生命周期管理。普通单机连接仍然需要一个运行时对象承载链路、MAVLink、心跳、遥测和命令状态。建议后端保留内部 vehicle/session 对象，把 fleet selection、批量命令、编队和蜂群作为后续可选功能，不让它们成为单机连接 API 的前置步骤。

当前 M-GCS 有一个需要收敛的具体问题：`mg_vehicle_open()` 是不改变焦点的通用入口，而 `mg_connection_connect()`、TCP/UDP 便捷接口又会打开相同会话并自动切换 active focus。对第一版单机客户端，公开一套连接入口即可；旧入口应在兼容期委托给唯一实现，不再赋予不同的焦点语义，待调用方迁移后再移除。

## 两个参考地面站的做法

### QGroundControl：先管理 Link，再按 MAVLink 身份建立 Vehicle

QGC 的 `LinkManager` 管理活跃链路，并把链路收到的字节接到全局 `MAVLinkProtocol`（`src/Comms/LinkManager.cc:165-170`）。`MultiVehicleManager` 监听 heartbeat 身份；对合格的 autopilot heartbeat 按 system ID 查重，不存在时才创建 `Vehicle`，并把发现它的 Link 交给新 Vehicle（`src/Vehicle/MultiVehicleManager.cc:66-119`）。因此 QGC 模型不是“一次连接调用等于一个飞控”，而是“打开一条链路，链路上出现一个或多个 MAVLink system”。

每个 QGC Vehicle 自己持有 `VehicleLinkManager`。收到该 vehicle 的 MAVLink 消息时，若消息来自尚未绑定的链路，就把链路加入该 Vehicle；随后逐链路检测通信丢失，并在主链路不健康时选择备用链路（`src/Vehicle/Vehicle.cc:519-529`；`src/Vehicle/VehicleLinkManager.cc:35-52, 100-130, 171-203, 251-310`）。这支持“一机多链路/链路冗余”，但它不是编队或蜂群控制。

QGC 确实有 `activeVehicle` 和 `selectedVehicles` 属性；注释将 active 描述为“从 UI 视角当前活动的 vehicle”（`src/Vehicle/MultiVehicleManager.h:16-43, 68-73`）。添加第一架 Vehicle 时会将其设为 active（`src/Vehicle/MultiVehicleManager.cc:129-135`）。所以它不是“完全没有多机管理”，而是把 active focus 作为 UI 便捷状态；核心命令接口是 Vehicle 实例的方法，例如 `Vehicle::sendMavCommand`（`src/Vehicle/Vehicle.h:588-615`），命令目标自然是该对象，不必先通过全局焦点再路由。

### Mission Planner：端口列表 + 每端口 MAV 列表，另有独立 Swarm 工具

Mission Planner 保留全局 `MainV2.Comports` 端口列表（`MainV2.cs:417-422`）。每个 `MAVLinkInterface` 持有 `MAVList`，并用 `sysidcurrent`/`compidcurrent` 暴露便捷的当前 MAV（`ExtLibs/ArduPilot/Mavlink/MAVLinkInterface.cs:289-323`）。连接时会统计 heartbeat 的 `(sysid, compid)`，选择一个当前系统/组件（同文件 `:870-895`）。因此 MP 同样保留了单机 UI 常用的 current MAV 便利状态，也能表达一个端口上看到多个 MAV。

关键区别在于 MP 也提供显式目标命令，例如 `doARM(sysid, compid, ...)`（`ExtLibs/ArduPilot/Mavlink/MAVLinkInterface.cs:2621-2645`）。Swarm 被放在独立 `Swarm/` 功能中，并遍历端口和 MAV 列表后按每个 MAV 的 sysid/compid 发命令（`Swarm/Swarm.cs:21-73`）。这说明“普通单机连接”与“集群批量/编队功能”可以是不同的用户工作流；但 MP 的全局 `MainV2` 和 current MAV 遗留状态也提醒我们不要照搬其全局耦合。

## 对照 M-GCS 当前实现

M-GCS 当前的 `mg_vehicle_open(endpoint)` 会先通过 `MultiVehicleManager::open_vehicle()` 创建一个 `Vehicle`，再让该 Vehicle 自己打开对应 serial/TCP/UDP 链路，函数成功时返回内部 `vehicle_id`（`bridge/ffi/ffi_vehicle_manager.cc:11-29`；`core/src/vehicle/multi_vehicle_manager.cc:21-87`）。`Vehicle` 持有自己的 `ConnectionManager`；链路打开后状态先是 `WaitingHeartbeat`，心跳到达后才转为 `Connected`（`core/include/core/transport/connection_manager.h:35-43, 64-85`；`core/src/vehicle/vehicle.cc:48-112, 173-205`）。所以 API 返回成功表示“链路已打开/会话已创建”，不等于“已识别并准备好控制的飞控”。连接 UI 应继续等状态/心跳，而不能把 open 成功直接当作飞控已就绪。

最近一次改动已使 `mg_vehicle_open()` 不改变 active focus，并在关闭当前焦点后清空焦点（`core/include/core/vehicle/multi_vehicle_manager.h:38-59`；`core/src/vehicle/multi_vehicle_manager.cc:90-128`）。不过 legacy `mg_connection_connect()` 与 TCP/UDP 入口仍调用 `mg_vehicle_open()` 后显式设置 active vehicle，而 `mg_connection_disconnect()` 关闭 active vehicle（`bridge/ffi/ffi_connection.cc:86-145`）。这形成两个公开连接习惯，单机使用者容易误解。

与 QGC/MP 相比，M-GCS 当前更接近“一次 endpoint open 对应一个 Vehicle runtime”。`MultiVehicleManager::on_heartbeat()` 收到的 `link_context` 被忽略，并按 sysid 更新/创建 Vehicle（`core/src/vehicle/multi_vehicle_manager.cc:189-220`）；但当前连接路径中，heartbeat 由每个 Vehicle 自己的 `MavlinkAdapter` callback 处理并更新自身身份（`core/src/vehicle/vehicle.cc:173-205`）。这还不是 QGC 那种共享 LinkManager、按 sysid 聚合多条链路的完整 topology，也不足以据此宣称已支持可靠的“一链路多飞控”或“一机多链路”。第一版单机产品不必立即补齐这类 fleet topology，应先明确并验证“一次连接对应一架目标飞控”的约束。

## 优劣判断

| 维度 | M-GCS 当前 | QGC | Mission Planner |
|---|---|---|---|
| 单机连接路径 | endpoint 创建 Vehicle+ConnectionManager；连接和 focus 已分开，但还有 legacy 双入口 | 通用 Link 打开，heartbeat 后发现 Vehicle；首机默认成为 UI active | 端口连接并选 current MAV，UI 便利状态较强 |
| 命令目标 | 新旧 API 多处仍经 active vehicle，尚非完整显式 vehicle target | Vehicle 对象方法天然携带目标 | 既支持 current MAV，也有 sysid/compid 显式命令 |
| 一链路多飞控 | 尚未证明/未形成完整聚合模型 | heartbeat 按 system ID 创建多个 Vehicle | 每个端口有 MAVList，可遍历多个 MAV |
| 一机多链路/冗余 | 当前基本一 Vehicle 对一 ConnectionManager | VehicleLinkManager 多 link、primary link 与健康切换 | 多端口与每端口 MAV 状态可用，但模型更分散 |
| 编队/蜂群 | 应作为后续独立领域功能 | 不是其普通 MultiVehicleManager 的职责 | 有独立 Swarm 功能，提供可参考的功能分区 |
| 主要优势 | 对象边界简单，C++ 后端/FFI 与 UI 分离，便于先把单机链路做扎实 | Link/Vehicle 关系和冗余模型成熟，命令目标明确 | ArduPilot 功能深，显式 sysid/compid 与 swarm 工具现成 |
| 主要代价/风险 | 双入口语义、active 路由残留、连接对象偏重；暂不支持成熟链路拓扑 | Qt/QML 对象体系较重，生命周期有异步两阶段处理 | 全局状态和 legacy current MAV 耦合较多，适合借功能不宜照搬架构 |

## 建议：先完成单机连接，再把 fleet/swarm 后置

1. **单机 UI 只有一种“连接/断开”流程。** 前端配置串口或网络 endpoint，调用统一连接 API；连接成功后订阅状态，显示“等待心跳 → 已连接/识别 → 断开/超时”。不要求进入多机选择页。
2. **后端可以继续复用 Vehicle/ConnectionManager。** 它们是单个飞控会话的运行时承载，不等于用户必须使用多机管理功能。可在产品语义上将其称为 connection session；避免为了名字把底层重写一遍。
3. **统一新代码使用的连接 API，同时保护旧 ABI。** 新代码统一使用 `mg_connection_open(endpoint)` 返回 session ID，再通过 `mg_connection_get_session_state` 查询、`mg_connection_close` 断开；这些操作不依赖 active focus。旧 `mg_vehicle_open` 和 serial/TCP/UDP convenience 函数只作为兼容入口保留，其中旧连接便捷函数继续选中其创建的 session，以免破坏仍通过 active vehicle 路由的命令 API。单机 UI 不暴露这些兼容差异；待客户端迁移且进入 ABI 主版本升级时再考虑删除旧符号。
4. **单机阶段可以有唯一当前 session，但不要把它扩展成 Fleet 功能。** 单一前端实例仅连接一架时，Controller 可以持有该连接返回的 session ID；数据和命令沿 session 调用。避免以“选中车辆”作为打开连接的步骤。
5. **下一阶段再加 Fleet。** Fleet 是多条已建立 session 的可选编排能力，负责车辆选择/分组、逐机执行、限流、超时和结果汇总。编队/蜂群再建立独立的 swarm domain，明确 leader/follower、同步策略、失联/故障处置和安全条件；不要把这些加进基础连接表单或 ConnectionManager。
6. **无需现在复制 QGC 的冗余链路拓扑。** 若产品后续明确要求同一飞控 USB+遥测电台并行或主备切换，再参考 QGC 的 VehicleLinkManager；若需求是一个 UDP 链路上发现多架飞机，再设计 Link 与 Vehicle 的多对多绑定。两种需求不要混为“批量控制”。

### 建议验收边界

单机第一版至少确认：串口/TCP/UDP 各自仅走一套连接语义；端口打开失败能及时返回；端口已打开但无 heartbeat 时保持明确等待状态并可取消/超时；首次 heartbeat 才标记飞控就绪；断链后状态、遥测和命令结果归属于同一个 session；断开 session 不会影响任何其他连接。当前现有状态机已具备 `WaitingHeartbeat/Connected/HeartbeatLost`，可在此基础上补验收，而无需先引入 fleet scheduler 或 swarm 抽象。

## 本地源码索引

- M-GCS：`core/include/core/vehicle/multi_vehicle_manager.h`；`core/src/vehicle/multi_vehicle_manager.cc`；`core/src/vehicle/vehicle.cc`；`core/include/core/transport/connection_manager.h`；`bridge/ffi/ffi_api_connection.h`；`bridge/ffi/ffi_vehicle_manager.cc`；`bridge/ffi/ffi_connection.cc`。
- QGroundControl：`src/Comms/LinkManager.cc`；`src/Vehicle/MultiVehicleManager.h/.cc`；`src/Vehicle/Vehicle.h/.cc`；`src/Vehicle/VehicleLinkManager.h/.cc`。
- Mission Planner：`MainV2.cs`；`ExtLibs/ArduPilot/Mavlink/MAVLinkInterface.cs`；`Swarm/Swarm.cs`。
