# M-GCS 多机框架实现评估报告

> 评估日期：2026-01-20
> 基线版本：`0.0.79`
> 评估人员：Kiro (Claude Code)
> 参考实现：QGroundControl 4.x (Apache-2.0/GPLv3) / Mission Planner 1.3.x (GPLv3)

---

## 一、执行摘要

M-GCS 当前多机框架已完成**核心解复用架构**和**基础生命周期管理**，达到 QGC 的共享链路分发能力和 MP 的单链路单机稳定性。与上游对比：

| 能力维度 | QGC 4.x | MP 1.3.x | M-GCS 0.0.79 | 差距评级 |
|---------|---------|----------|--------------|---------|
| **链路架构** | ||||
| 共享 UDP 多机解复用 | ✓ | ✗ | ✓ | ★★★★★ 已对齐 QGC |
| 串口/TCP 共享链路 | ✓ | ✗ | ✓（本地回归） | ★★★★★ 已对齐结构，现场验收待补 |
| 同机多链路合并 | ✓ VehicleLinkManager | ✗ | ✗ | ★★☆☆☆ 核心缺失 |
| **生命周期** | ||||
| 心跳自动发现 | ✓ | ✓ | ✓ | ★★★★★ 已对齐 |
| 链路质量跟踪 | ✓ heartbeatElapsedTimer | 基础 | ✗ | ★★★☆☆ 需补充 |
| 通信丢失检测 | ✓ communicationLost 分级 | ✓ | 部分 | ★★★☆☆ 需细化 |
| 忽略列表 | ✓ _ignoreVehicleIds | ✗ | 短暂抑制 | ★★★★☆ 基本覆盖 |
| **状态隔离** | ||||
| 按车辆状态管理 | ✓ | ✓ | ✓ | ★★★★★ 已对齐 |
| 异步操作取消 | ✓ | 部分 | 部分 | ★★★☆☆ 需加固 |
| 任务文档隔离 | ✓ | ✓ | ✓ | ★★★★★ 已对齐 |
| **UI 多机支持** | ||||
| 地图显示多机 | ✓ | ✓ | ✗ | ★☆☆☆☆ 未开始 |
| 批量选择 | ✓ selectedVehicles | ✗ | ✗ | ★☆☆☆☆ 未开始 |
| 批量命令 | ✓ | ✗ | ✗ | ★☆☆☆☆ 未开始 |

**总体评级：3.5/5 星**
已完成协议层和后端基础架构，缺前端多机地图和批量操作。

---

## 二、架构对比

### 2.1 QGC 架构（参考为主）

```cpp
// QGC: 链路与车辆完全解耦，多对多关系
MAVLinkProtocol::vehicleHeartbeatInfo(link, vehicleId, compId, fwType, vehType)
  -> MultiVehicleManager::_vehicleHeartbeatInfo()
    -> new Vehicle(link, vehicleId, ...)
      -> VehicleLinkManager::_addLink(link)

// 同一车辆可通过多个链路通信
Vehicle {
  VehicleLinkManager {
    QList<LinkInfo_t> _rgLinkInfo;  // 所有链路
    WeakLinkInterfacePtr _primaryLink;  // 主链路
    bool _communicationLost;  // 全链路失联
    int _heartbeatMaxElpasedMSecs = 3500;
  }
}

// 关键特性
1. 链路独立于车辆：Link 先存在，Vehicle 后绑定
2. 一车多链路：同一 sysid 可被多个 Link 报告，合并到一个 Vehicle
3. 主链路切换：primary/secondary 失效时自动切换
4. 分级通信丢失：单链路丢失 vs 全链路丢失
5. 忽略列表：手动关闭后短时间内屏蔽重发现
```

### 2.2 MP 架构（单链路单机为主）

```csharp
// MP: 一个 MAVLinkInterface 对应一个 sysid
MAVLinkInterface {
  ICommsSerial BaseStream;  // 单个传输层
  byte sysid;  // 目标系统
  byte compid;  // 目标组件
  MAVState MAV;  // 当前车辆状态
}

// 多机通过多个 MAVLinkInterface 实例
MainV2.Comports[0] -> Vehicle A
MainV2.Comports[1] -> Vehicle B

// 关键特性
1. 链路与车辆强绑定：一个 port 对应一个 Vehicle
2. 无共享链路：UDP 多机需要多个 socket（例如 14550/14551）
3. 切换通过 MainV2.comPort 全局引用
4. 专用 ArduPilot 语义：MAV_CMD 执行、参数格式、校准流程
```

### 2.3 M-GCS 当前架构（QGC 结构 + MP 单机稳定性）

```cpp
// M-GCS: 链路层解复用 + 车辆层隔离
LinkSession {
  ILink* link_;  // 物理传输
  MavlinkRouter router_;  // 帧解析与分发
  std::map<int, LinkPeer> peers_;  // UDP peer 映射
}

MavlinkRouter {
  std::map<int, Target> by_id;  // route_id -> adapter
  std::map<uint16_t, int> by_identity;  // (sysid, compid) -> route_id
  DiscoveryCallback discovery;  // 未知心跳回调
}

MultiVehicleManager {
  open_vehicle(endpoint) -> Vehicle A  // 独立 endpoint
  open_shared_vehicle(endpoint, 0, 0) -> Vehicle placeholder  // 共享 endpoint
    -> router.route_bytes()
      -> 发现 sysid=1 -> on_shared_heartbeat() -> 绑定或创建
}

// 对齐点
✓ UDP 共享链路解复用：与 QGC 同级
✓ 心跳自动发现：与 QGC/MP 同级
✓ 按车辆状态隔离：与 QGC/MP 同级
✓ 精确关闭（不影响共享 socket）：与 QGC 同级

// 缺失点
✗ 同机多链路合并：无 VehicleLinkManager 等效物
✗ 链路质量跟踪：无心跳超时计时器
✗ 主链路切换：当前只有单链路或共享链路，无多链路归属
✓ 串口/TCP 共享：使用通用 LinkSession；TCP loopback 和 POSIX PTY 多 sysid 回归已通过
```

---

## 三、关键能力逐项评估

### 3.1 链路架构 (4/5 星)

#### QGC 参考实现
```cpp
// src/Vehicle/VehicleLinkManager.cc:35
void VehicleLinkManager::mavlinkMessageReceived(LinkInterface *link,
                                                 const mavlink_message_t &message)
{
    // 忽略 RADIO_STATUS（不代表飞控存活）
    if (message.msgid == MAVLINK_MSG_ID_RADIO_STATUS) return;

    int linkIndex = _containsLinkIndex(link);
    if (linkIndex == -1) {
        _addLink(link);  // 自动添加新链路到当前车辆
        return;
    }

    LinkInfo_t &linkInfo = _rgLinkInfo[linkIndex];
    linkInfo.heartbeatElapsedTimer.restart();  // 更新心跳计时
    if (_rgLinkInfo[linkIndex].commLost) {
        _commRegainedOnLink(link);  // 链路恢复
    }
}

// src/Vehicle/MultiVehicleManager.cc:68
void MultiVehicleManager::_vehicleHeartbeatInfo(LinkInterface* link,
                                                 int vehicleId, ...)
{
    // 只接受 MAV_COMP_ID_AUTOPILOT1
    if (componentId != MAV_COMP_ID_AUTOPILOT1) return;

    // 同一 vehicleId 的心跳自动归属到已有 Vehicle
    // 不同链路的心跳被 VehicleLinkManager 合并
    if (getVehicleById(vehicleId)) return;  // 已存在，让 VehicleLinkManager 处理

    Vehicle *vehicle = new Vehicle(link, vehicleId, ...);
    // 后续从其他链路来的相同 vehicleId 心跳，Vehicle 会自动添加链路
}
```

**M-GCS 当前实现**
```cpp
// core/src/transport/link_session.cc
// ✓ 有 MavlinkRouter 统一解析
// ✓ 有 UDP peer 记录
// ✓ 支持同机多链路归并（同一 sysid 跨 managed endpoint）
// ✗ 无链路心跳计时器

std::vector<int> MavlinkRouter::route_bytes(const uint8_t* data, size_t size,
                                             const MavlinkPeer* sender)
{
    // 一次解析，按 sysid/compid 路由到已注册 adapter
    // 未注册时触发 DiscoveryCallback
    // ✓ 支持跨 chunk 粘连帧
    // ✓ 重复身份保护（同一 sysid/compid 不能绑定两个 route）
}
```

**差距**
1. ✗ 同一 sysid 无法同时在 USB (LinkSession A) 和遥测 (LinkSession B) 出现
2. ✗ 无链路级心跳超时检测（依赖 Vehicle 层的全局心跳超时）
3. ✓ UDP 共享链路完整实现，与 QGC 同级

**建议**
- [ ] 引入 `VehicleRouteManager`，管理一个 Vehicle 的多个 route_id
- [ ] LinkSession 内添加 per-route 心跳计时器
- [x] 扩展共享链路到串口/TCP（通用 LinkSession + 本地回归）

---

### 3.2 生命周期管理 (3.5/5 星)

#### QGC 参考实现
```cpp
// src/Vehicle/VehicleLinkManager.cc:100
void VehicleLinkManager::_commLostCheck()  // 每 1 秒调用
{
    const int heartbeatTimeout = 3500;  // 3.5 秒无心跳视为失联

    bool linkStatusChange = false;
    for (LinkInfo_t &linkInfo: _rgLinkInfo) {
        if (!linkInfo.commLost &&
            linkInfo.heartbeatElapsedTimer.elapsed() > heartbeatTimeout) {
            linkInfo.commLost = true;  // 单链路丢失
            linkStatusChange = true;

            // 通知用户："primary/secondary link communication lost"
        }
    }

    if (linkStatusChange) {
        emit linkStatusesChanged();  // UI 更新链路状态
    }

    _updatePrimaryLink();  // 尝试切换到其他活跃链路

    // 检查全链路丢失
    bool totalCommunicationLoss = std::all_of(_rgLinkInfo,
        [](const LinkInfo_t &info) { return info.commLost; });

    if (totalCommunicationLoss) {
        if (_autoDisconnect) {
            closeVehicle();  // 自动断开
            return;
        }
        _communicationLost = true;
        emit communicationLostChanged(true);  // 触发 UI "Communication Lost" 警告
    }
}

// src/Vehicle/MultiVehicleManager.cc:149
void MultiVehicleManager::_deleteVehiclePhase1(Vehicle *vehicle)
{
    // 由 VehicleLinkManager::allLinksRemoved 信号触发
    // 两阶段删除：先从列表移除，延迟 20ms 后销毁（等 Qml 释放引用）
    _vehicles->removeAt(i);
    QTimer::singleShot(20, [this, vehicle]() { _deleteVehiclePhase2(vehicle); });
}

// src/Vehicle/MultiVehicleManager.h:74
QList<int> _ignoreVehicleIds;  // 手动关闭后短时间屏蔽

// 关键设计
1. 分级通信丢失：单链路 vs 全链路
2. 主链路自动切换：primary 失效时选 best active
3. 延迟删除：避免 Qml 持有引用时崩溃
4. 忽略列表：防止立即重发现
```

**M-GCS 当前实现**
```cpp
// core/src/vehicle/multi_vehicle_manager.cc
void MultiVehicleManager::close_vehicle(int session_id, bool allow_reuse)
{
    // ✓ 有关闭抑制期（100ms 内屏蔽同 sysid 重发现）
    // ✓ 精确关闭（共享 LinkSession 内只移除一个 route）
    // ✗ 无长期忽略列表
    // ✗ 无主链路切换（当前只支持单链路或共享链路）
}

// ✗ 无 per-link 心跳超时检测
// ✓ 有 Vehicle 层全局心跳超时（未细化到链路）
```

**差距**
1. ✗ 无链路级心跳计时和 `commLost` 状态
2. ✗ 无主链路切换逻辑
3. ✗ 忽略列表只有 100ms 抑制，无用户手动关闭后的持久屏蔽
4. ✓ 共享链路精确关闭已实现

**建议**
- [ ] 添加 `ignored_vehicles_` 持久列表（与 QGC 对齐）
- [ ] 实现同机多链路后，补充主链路切换
- [ ] LinkSession 内添加 per-route 心跳计时

---

### 3.3 状态隔离 (4.5/5 星)

#### QGC 参考实现
```cpp
// QGC: Vehicle 对象即状态容器
Vehicle {
  ParameterManager* parameterManager;
  MissionManager* missionManager;
  QGCCameraManager* cameraManager;
  MAVLinkLogManager* mavlinkLogManager;
  // ... 30+ 管理器
}

// 切换 active vehicle 只改 UI 焦点，不移动状态
MultiVehicleManager::setActiveVehicle(Vehicle *vehicle) {
  _activeVehicle = vehicle;
  emit activeVehicleChanged(vehicle);
  // Qml 绑定到 vehicle 的各个管理器
}
```

**M-GCS 当前实现**
```dart
// flutter_shell/lib/connection/connection_controller.dart
class ConnectionController {
  Map<int, VehicleRuntimeSnapshot> _vehicleSnapshots = {};

  void switchActiveSession(int newSessionId) {
    _saveCurrentSnapshot();  // 保存当前车辆状态
    _applySnapshot(newSessionId);  // 恢复目标车辆状态
  }
}

// ✓ 已隔离：遥测、参数、消息、校准、Home、GoTo、飞行统计
// ✓ 任务文档按 session 独立保存
// ✗ 部分异步操作的 session 校验需加固
```

**差距**
1. ✓ 主要状态已按 session 隔离，与 QGC 同级
2. ✗ 异步取消语义需补全（参数下载、日志、校准）
3. ✗ 旧任务面板回调的重绑定测试覆盖不足

**建议**
- [ ] 所有异步回调添加 `session_id` 校验
- [ ] 补充跨车辆竞态测试（校准、参数、日志）

---

### 3.4 UI 多机支持 (1/5 星)

#### QGC 参考实现
```qml
// QGC: 地图显示所有车辆
MapItemView {
  model: QGroundControl.multiVehicleManager.vehicles
  delegate: VehicleMapItem {
    coordinate: object.coordinate
    heading: object.heading
    vehicleColor: object.id === activeVehicle.id ? "yellow" : "gray"
  }
}

// 批量选择
MultiVehicleManager {
  Q_PROPERTY(QmlObjectListModel* selectedVehicles ...)
  Q_INVOKABLE void selectVehicle(int vehicleId);
  Q_INVOKABLE void deselectVehicle(int vehicleId);
}
```

**M-GCS 当前实现**
```dart
// flutter_shell/lib/map/amap_map_view.dart
// ✗ 只显示 active 车辆的主任务和 Home
// ✗ 无非 active 车辆的位置标记

// flutter_shell/lib/widgets/mg_top_status_bar.dart
// ✓ 有车辆选择器（多机时显示）
// ✗ 无 selectedVehicles 独立选择集
// ✗ 无批量飞行命令
```

**差距**
1. ✗ 地图未显示多机位置/航向/状态
2. ✗ 无多选 UI
3. ✗ 无批量解锁/返航/开始任务

**建议**
- [ ] 地图添加 `for vehicle in fleet.vehicles` 渲染循环
- [ ] 添加 `FleetController.selectedVehicles`
- [ ] 飞行命令扩展 `executeForVehicles(List<int> sessionIds)`

---

## 四、核心差距与实施路径

### 4.1 MV3 生命周期（优先级：高）

**目标：** 同机多链路合并 + 链路质量跟踪

**参考：** QGC `VehicleLinkManager`

**实施步骤：**
1. 引入 `VehicleRouteManager` 管理一个 Vehicle 的多个 route_id
2. 每个 route_id 记录心跳计时器和 `commLost` 状态
3. 实现主链路切换逻辑（选择延迟最低的活跃链路）
4. 补充长期忽略列表（`ignored_vehicles_`）
5. 串口/TCP 真实设备/总线验收（身份键仍按 sysid/compid，UID 策略待产品确认）

**验收：**
- [ ] 同一 sysid 可同时在 USB 和遥测出现，合并为一个 Vehicle
- [ ] 单链路失联时切换到另一链路，UI 显示 "Communication lost on secondary link"
- [ ] 全链路失联时显示 "Communication Lost"，5 秒后可选自动断开
- [ ] 手动关闭后 30 秒内屏蔽重发现

---

### 4.2 MV4 异步操作隔离（优先级：中）

**目标：** 所有异步操作添加 session 校验

**参考：** QGC 的 `Vehicle` 对象生命周期管理

**实施步骤：**
1. 所有 FFI 回调添加 `int session_id` 参数
2. Flutter 层回调入口校验 `session_id == currentSessionId`
3. 补充跨车辆竞态测试（A 开始校准 → 切到 B → A 校准完成 → 不影响 B）
4. 参数下载添加取消语义（切机时终止）

**验收：**
- [ ] A 机参数下载中途切到 B，A 的回调不写入 B 的参数表
- [ ] A 机校准中途切到 B，A 的校准进度不显示在 B 的 UI
- [ ] 所有异步操作的单元测试覆盖切机场景

---

### 4.3 MV5 多机地图（优先级：中）

**目标：** 地图显示所有车辆

**参考：** QGC `VehicleMapItem`

**实施步骤：**
1. `AMapMapView` 添加 `for vehicle in fleetController.vehicles` 渲染
2. 非 active 车辆显示灰色标记 + 姿态箭头
3. active 车辆显示黄色高亮 + 主任务
4. 点击车辆标记切换 active

**验收：**
- [ ] 两架飞机同时显示在地图上
- [ ] 点击飞机标记切换 active
- [ ] active 车辆显示主任务，非 active 不显示

---

### 4.4 MV6 批量操作（优先级：低）

**目标：** 多选与批量飞行命令

**参考：** QGC `selectedVehicles` / MP 无批量（不对标）

**实施步骤：**
1. `FleetController` 添加 `selectedVehicleIds: Set<int>`
2. 顶部状态栏添加多选模式切换
3. `executeForVehicles(List<int> sessionIds, FlightCommand cmd)`
4. 逐机结果汇总（成功/拒绝/超时）

**验收：**
- [ ] 可选中 2 架飞机，批量解锁
- [ ] 一架解锁成功，一架拒绝，UI 分别显示结果
- [ ] 批量返航

---

## 五、测试覆盖评估

### 当前测试矩阵

| 测试类型 | QGC | MP | M-GCS | 备注 |
|---------|-----|-----|-------|------|
| 单元测试 | ✓ | ✗ | ✓ | M-GCS 36 个后端测试通过 |
| UDP 共享链路测试 | ✓ | ✗ | ✓ | M-GCS 3 个 FFI 测试 + Python smoke |
| 多链路切换测试 | ✓ | ✗ | ✗ | 需补充 |
| 通信丢失测试 | ✓ | 部分 | 部分 | M-GCS 缺链路级测试 |
| 跨车辆竞态测试 | ✓ | ✗ | 部分 | M-GCS 缺异步操作测试 |
| 真实硬件测试 | ✓ | ✓ | 部分 | M-GCS 仅测过 UDP |

**建议补充：**
- [ ] 多链路切换测试（USB + 遥测）
- [ ] 链路失效恢复测试
- [ ] 跨车辆参数下载竞态测试
- [ ] 真实串口多机测试

---

## 六、许可证合规性检查

### QGC 引用合规性
- M-GCS 使用 MIT 许可证
- QGC 代码参考限于：
  - ✓ 协议事实（MAVLink 消息处理流程）
  - ✓ 架构模式（VehicleLinkManager 概念）
  - ✗ **未直接复制** QGC 的 C++ 代码
- 当前实现：自研 C++ 解复用器 + Dart UI，无 GPL 污染

### MP 引用合规性
- MP 为 GPLv3，M-GCS 当前未引用 MP 代码
- 参考仅限于：
  - ✓ ArduPilot 专用语义（MAG_CAL_PROGRESS、校准流程）
  - ✓ 单链路单机的稳定性设计
  - ✗ **未复制** MP 的 C# 代码

**结论：** 当前合规，可继续复用协议流程和架构模式。

---

## 七、总体建议

### 短期（1-2 周）
1. **补齐 MV3 生命周期：** 同机多链路合并（参考 QGC VehicleLinkManager）
2. **加固 MV4 异步隔离：** 所有回调添加 session 校验
3. **验收测试：** 补充多链路切换和跨车辆竞态测试

### 中期（2-4 周）
4. **实现 MV5 多机地图：** 地图显示所有车辆
5. **扩展共享链路：** 串口/TCP 共享（可选）

### 长期（可选）
6. **实现 MV6 批量操作：** 多选与批量命令

### 是否提交当前改动？
**建议：** 先提交当前 UDP 共享链路实现，理由：
- ✓ UDP 共享链路已完整实现并测试通过
- ✓ 独立 endpoint 多链路已稳定
- ✓ 状态隔离已基本完成
- ✗ MV3 生命周期可作为独立版本（0.0.80）后续提交

**提交标题：** `feat: multi-vehicle UDP shared link demultiplexing (MV2)`

**下一版本：** `feat: multi-vehicle lifecycle and link health tracking (MV3)`

---

## 八、评分卡

| 维度 | 得分 | 满分 | 说明 |
|-----|------|------|------|
| 链路架构 | 8 | 10 | UDP 共享完整，缺同机多链路 |
| 生命周期 | 7 | 10 | 心跳发现完整，缺链路质量跟踪 |
| 状态隔离 | 9 | 10 | 主要状态已隔离，缺异步取消加固 |
| UI 多机 | 2 | 10 | 仅有选择器，缺地图和批量操作 |
| 测试覆盖 | 7 | 10 | 单元测试充分，缺真实多链路测试 |
| 许可证合规 | 10 | 10 | 无直接复制，参考合规 |
| **总分** | **43** | **60** | **71.7%** |

**结论：** M-GCS 多机框架已完成**核心基础架构**（链路解复用 + 状态隔离），达到可用阶段，但需补齐**生命周期管理**和**UI 多机支持**才能达到 QGC 水平。

---

*评估完成时间：2026-01-20*
*下次审核建议：MV3 实施完成后*
