# M-GCS 多机控制、编队与蜂群参考

研究日期为 2026-11-06。本文件区分外部项目已描述的做法与针对 M-GCS 的建议。外部能力依据官方文档与源码，本次未进行第三方软件飞行实测。

## 2026-11-10 GitHub 源码补充

本轮按当时 GitHub `master`/`main` 的固定提交重新核对源码，重点是“地面站替操作者做了什么”，不讨论机载编队、避碰或轨迹算法。这里的“未发现”只代表所查源码路径没有该流程，不代表项目绝对不具备。

| 项目与快照 | 地面站实际职责 | 边界与限制 |
|---|---|---|
| QGroundControl `6eb58f0` | 区分全部车辆、活动车辆和临时选中集合；多机面板显示逐车状态，并对选中集合逐机执行 Arm、Disarm、Start Mission、Pause | 所查 MultiVehicle 路径未发现持久命名小队、槽位规划或群组任务生成。批量分支在执行时读取当前选择，不能据此宣称确认后冻结目标或同步动作。任务编辑器通常跟随活动车，不能把“已多选”理解为“批量改任务/上传”的同一目标。[多机与选择](https://github.com/mavlink/qgroundcontrol/blob/6eb58f084992665d9d6d96d8d58824c2df7cad56/src/Vehicle/MultiVehicleManager.cc) [批量动作](https://github.com/mavlink/qgroundcontrol/blob/6eb58f084992665d9d6d96d8d58824c2df7cad56/src/FlyView/GuidedActionsController.qml) [任务作用域](https://github.com/mavlink/qgroundcontrol/blob/6eb58f084992665d9d6d96d8d58824c2df7cad56/src/MissionManager/PlanMasterController.cc) |
| Mission Planner `a64d768` | Formation 窗口选择物理领机、编辑成员相对偏移，运行时在地面读取领机状态并持续向成员发送目标；窗口还显示逐机模式、解锁状态、GPS、目标与实际位置 | 这是 GCS 在线目标分发原型，不是“每机任务模板→逐机航线上传”。群动作通常遍历当前连接车辆，并带有包含或排除领机的规则，不是冻结确认的命名小队。源码窗口标为 Beta，官方也将其限定为实验性能力。停止按钮停止地面发送循环，不等于已让飞机暂停或返航。[Formation 窗口](https://github.com/ArduPilot/MissionPlanner/blob/a64d768676decd213c94ac52580edfa3bbfe0678/Swarm/FormationControl.cs) [目标发送](https://github.com/ArduPilot/MissionPlanner/blob/a64d768676decd213c94ac52580edfa3bbfe0678/Swarm/Formation.cs) [群动作范围](https://github.com/ArduPilot/MissionPlanner/blob/a64d768676decd213c94ac52580edfa3bbfe0678/Swarm/Swarm.cs) |
| MAVProxy `130908b` | Swarm 插件从飞控参数 `FOLL_SYSID` 推导领机、跟随机和未归组车辆；按组成员显示心跳、模式、电池、预解锁和告警，并提供带 SYSID/COMPID 的定向按钮 | 组关系来自飞控 Follow 配置，不是在 GCS 里创建任意命名编队。不同按钮的成员范围不同，例如 Auto 面向领机、Follow All 面向跟随机。所查插件没有统一的逐命令 pending/ACK/部分失败结果账本，“All”也不能视为原子操作。[Swarm 模块](https://github.com/ArduPilot/MAVProxy/blob/130908b97a25186ee6649d1aae5efb1cdefbc752/MAVProxy/modules/mavproxy_swarm.py) |
| Skybrush Live `62c3446` | 以无人机表演为专门场景，把逻辑任务槽位和实际 UAV 分开；上传前检查空槽、缺失无人机、起飞位置与朝向，逐机生成包含槽位参数的场次数据并单独上传；上传结果按 UAV 聚合，失败项可重试；启动前还建模现地环境、围栏、机载/人工预检、启动时间和操作者授权 | 这是配合专门演出执行栈的流程，不能据此宣称普通 ArduPilot 机队已支持相同能力。`stages.ts` 主要定义阶段状态和依赖，个别阶段仍明确是 TODO；因此应借鉴“流程与可观测状态”，不要把阶段列表写成所有按钮均被强制锁死。逐机上传成功也不等于机群同步起飞。[槽位到实机映射](https://github.com/skybrush-io/live/blob/62c344699695975bab4f8c92aeb5607d1e9ad0dd/src/features/show/upload.js) [起飞区检查](https://github.com/skybrush-io/live/blob/62c344699695975bab4f8c92aeb5607d1e9ad0dd/src/views/show-control/TakeoffAreaSetupDialog.tsx) [阶段与依赖](https://github.com/skybrush-io/live/blob/62c344699695975bab4f8c92aeb5607d1e9ad0dd/src/features/show/stages.ts) [逐机上传结果](https://github.com/skybrush-io/live/blob/62c344699695975bab4f8c92aeb5607d1e9ad0dd/src/features/upload/selectors.ts) |

固定提交只用于固定本次源码证据，不代表对应能力已经进入稳定发行版：

- QGroundControl：`6eb58f084992665d9d6d96d8d58824c2df7cad56`
- Mission Planner：`a64d768676decd213c94ac52580edfa3bbfe0678`
- MAVProxy：`130908b97a25186ee6649d1aae5efb1cdefbc752`
- Skybrush Live：`62c344699695975bab4f8c92aeb5607d1e9ad0dd`

### 地面站通常承担的八类工作

1. **身份与集合**：把物理飞机、逻辑组成员、活动焦点和一次操作的目标集合分开。QGC 明确区活动车与临时选择；MAVProxy 的组由飞控跟随关系派生；M-GCS 的持久小队保存稳定成员身份，三者语义不同。
2. **角色与槽位**：为成员分配领机、跟随位置或演出槽位。Mission Planner 直接把偏移绑定到成员；Skybrush 把逻辑槽位映射到实机，并拒绝不在任务中的 UAV 上传。
3. **计划落地**：把“编队意图”变成每架飞机可执行的任务、轨迹或目标，并提供整体预览。QGC 的基础多机入口没有完成这套槽位到逐机任务的产品链；Skybrush 是专门演出实现；MAVProxy 面板主要管理已有 Follow 关系。
4. **上传与准备**：逐机生成产物、显示排队/进行中/失败/部分成功，支持失败项重试。Skybrush 的逐机上传状态与任务映射是清楚的参考。
5. **起飞前检查**：检查成员在线、位置、朝向、任务、围栏与人工检查结果。检查项应能定位到具体 UAV，而不是只给一个“机群就绪”总灯。
6. **执行范围确认**：确认页必须写清实际参与的车辆、被跳过的离线/冲突成员、动作和固定成员集合。QGC 的批量 UI 可学习信息组织，但其执行时读取当前选择的做法不满足 M-GCS 已规定的冻结目标语义。
7. **结果与状态监控**：逐机保留发送失败、拒绝、超时、取消和接受结果；接受后的动作完成再按模式、解锁、位置、任务进度等遥测条件判断。全组统一汇总不能抹掉单机失败。
8. **停止与恢复**：区分停止后续派发、暂停任务、停止地面目标流、返航或降落，并记录哪些飞机已被人工接管。没有任何一个已查 GCS 能靠单一“Stop All”按钮自动保证所有飞机进入安全状态。

### 对 M-GCS 分组功能的直接结论

- 当前已有的“持久小队”方向是对的，但还应成为地图执行页的一等操作对象：在机队列表中直接按小队选定成员，同时显示该小队在线、离线、身份冲突和临时身份待确认人数。仅放在设置页，操作者容易忘记本次实际执行的是哪些飞机。
- 小队成员管理需要支持从当前勾选飞机追加成员、移除单个成员；成员变更只改本地分组，不得隐式写 `FOLL_SYSID`、切 Follow 模式或改动飞机状态。
- 执行确认和结果必须继续使用“先冻结明确 `vehicle_id` 与会话，再逐机发送并保留 ACK/失败结果”的现有路线。不能因为界面上选择了一个小队，就把之后新上线或同 SYSID 的飞机自动纳入。
- “批量开始任务”仍是顺序下发，不等于同时起飞。需要同步启动时，应另外设计明确的时间同步/同步启动机制并验证飞控支持，不能用命令循环速度冒充同步。
- 角色、队形模板和逐机任务生成应在真实任务类型出现后再落代码。Skybrush 的关键参考是先建立“逻辑槽位→物理飞机”的显式映射，再生成逐机产物；不是先做一套没有任务使用方的角色字段。
- 本轮据此先实现：地图多机面板按小队选择成员、界面可见实际成员集合，以及小队成员的追加/移除。它补足了主流方案里“集合必须清楚”的基础，同时不引入未经验证的编队控制。

## 建议的总体路线

沿多机可靠控制、协同任务、动态编队、蜂群自治四个阶段推进。初期采用地面集中任务编排与飞控本地执行，后续根据动态规划和感知需求增加机载协同能力。

首轮建议使用 4 架同型 ArduCopter SITL，随后扩展至 8–12 架验证编队，再单独进行更大数量的通信和界面压力测试。这些数量是研究建议的验证档位。

## 外部方案的可借鉴做法

| 方案 | 已核实做法 | 对 M-GCS 的参考价值 |
|---|---|---|
| QGroundControl master | 活动车辆与多选集合分开，批量动作逐机调用 | 查看焦点、选中集合、操作目标分别管理。[车辆管理](https://github.com/mavlink/qgroundcontrol/blob/master/src/Vehicle/MultiVehicleManager.cc) [批量动作](https://github.com/mavlink/qgroundcontrol/blob/master/src/FlyView/GuidedActionsController.qml) |
| Mission Planner | 普通多机接入与 Formation 功能分别存在，后者在地面计算并持续下发偏移目标 | 用于学习地面领航跟随原型。官方 Swarming 文档标为 Beta，其旧文档细节与 master 的机型分支需要区分。[文档](https://ardupilot.org/planner/docs/swarming.html) [源码](https://github.com/ArduPilot/MissionPlanner/blob/master/Swarm/Formation.cs) |
| MAVProxy | 活动车辆、alllinks 逐机操作、基于 FOLL_SYSID 的群组 GUI | 适合构建可脚本化复现路径，参考组与成员的控制入口。[多机指南](https://ardupilot.org/mavproxy/docs/getting_started/multi.html) [Swarm 模块](https://ardupilot.org/mavproxy/docs/modules/swarm.html) |
| Skybrush | 设计、验证、轨迹映射、检查与同步执行的完整流程 | 参考机队作业流程及任务准备度展示。[官方架构](https://docs.skybrush.io/public/skybrush-server-doc/latest/features.html) |
| PX4 ROS 2 | 每机 namespace、client key 与目标编号，飞控与伴随计算机桥接 | 参考多机身份隔离与机载计算接入边界。[多机指南](https://docs.px4.io/main/en/ros2/multi_vehicle) |
| Aerostack2 | 虚拟质心、逐机相对位姿与 FollowReference | 参考编队表示及可修改的队形任务接口。[官方编队文档](https://aerostack2.github.io/_04_robot_behaviors/swarm_flocking/index.html) |
| EGO-Swarm | 原始论文描述机载感知、去中心化异步规划和轨迹共享 | 参考后续机载分布式自治的职责范围。[原始论文](https://arxiv.org/abs/2011.04183) |

QGC 引用范围为 master/Daily，PX4 为 main 开发版。参数、namespace 和能力适配需要对齐实际固件与软件版本。Aerostack2 编队接口的参考价值在于任务和参考帧表达，其部署拓扑需要单独核验。

Aerostack2 原始论文的室外双机巡检展示地面 GUI 创建并上传任务，任务加载后由各机 Jetson 计算执行。这为地面规划监督、机载执行提供了具体案例。[原始论文 IV-B](https://arxiv.org/html/2303.18237v2#S4.SS2)

## 基础协议的依据

- MAVLink 命令通过匹配的 COMMAND_ACK 返回结果。ACCEPTED 表示飞控接受并尝试执行，动作完成需要依据具体命令和遥测另行判断。[命令协议](https://mavlink.io/en/services/command.html)
- 任务、围栏与集结点传输使用任务协议，MISSION_ACK 确认传输结果。上传成功与任务执行完成分别记录。[任务协议](https://mavlink.io/en/services/mission.html)
- 外部控制使用位置、速度、加速度等设定值消息，支持范围依飞控和机型而异。[Offboard 接口](https://mavlink.io/en/services/offboard_control.html)
- 冗余链路可能使同一命令多次抵达。MAVLink 包序号不能直接用作跨链路去重依据，各链路有自己的计数器。[冗余与去重](https://mavlink.io/en/guide/redundancy_deduplication.html)
- TIMESYNC 通过往返通信估计时钟偏差，需要过滤异常测量。部署时验证飞控版本、实际支持和偏差精度。[时间同步](https://mavlink.io/en/services/timesync.html)
- ArduPilot 支持按消息请求发送间隔。参数和航点传输可能降低遥测频率，容量验证需要同时覆盖这些流量。[请求遥测](https://ardupilot.org/dev/docs/mavlink-requesting-data.html)

## M-GCS 当前可复用的代码

- 前端 captureBatchTarget 冻结车辆集合，runBatch 逐机 await 并保存每机结果，一架失败后继续处理其他成员。[fleet_controller.dart](../flutter_shell/lib/fleet/fleet_controller.dart#L217)
- bridge 提供显式 vehicle_id 的异步提交，捕获提交时的车辆和会话，返回 ticket 并支持 poll。[ffi_flight_commands.cc](../bridge/ffi/ffi_flight_commands.cc#L611)
- core 的 MultiVehicleManager 提供多车辆和共享链路入口，Vehicle 提供会话、多路由与命令连接对象。[multi_vehicle_manager.h](../core/include/core/vehicle/multi_vehicle_manager.h#L58) [vehicle.h](../core/include/core/vehicle/vehicle.h#L62)

以上为局部只读代码定位。本次没有运行构建或测试，不能据此宣称整套多机系统已通过验证。

> 更新（2026-11-08，v1.2.0）：本文件以下内容是研究建议。已落地的是持久小队的最小闭环——
> 硬件身份（`uid2`/`uid`）经 `mg_vehicle_list` 的 `hardware_uid` 暴露，小队按身份认回成员，
> 就绪检查与整队批量执行在“设置 → 小队”的地图浮层；多机面板只做当前会话的勾选与批量执行。
> 编队模板、槽位、角色与机载协同仍未实现，见 `docs/ROADMAP.md` 的“当前不承诺”。

## 总体架构选择

地面集中控制便于初期调试，但需要持续下发目标的动作依赖地面链路。机载分布式协同适合要求失去地面通信仍可协作的任务，但增加机载计算、通信与一致性工作。

建议采用地面集中任务编排与机载局部执行相结合的方式。初期复用 ArduPilot Auto、Guided、Follow 等能力，在真实需求出现时加入机载任务执行器、局部规划或邻机协作。

软件依赖继续保持 flutter_shell → bridge/ffi → core → third_party。Flutter 负责交互和显示，bridge 负责数据转换及调用边界，core 负责群体任务状态、逐机调度与结果聚合。机载端根据任务需要接收单机计划或轨迹，负责本机执行。core 直接使用自身车辆和连接能力，bridge 的目标作用域类型保留在桥接边界。

按任务需求分三种控制模式。单机 Auto 任务适合可提前上传的区域作业，Follow 适合领机加空间偏移，机载轨迹执行器适合需要本地规划或更严格时间语义的动态协同。初期在同一 core 中维护责任边界，规模和部署需求明确后再决定独立后台进程。

Skybrush 值得借鉴的部分是从任务准备到执行的完整流程。其官方架构使用独立 Server 与前端，并有专用机载表演模式。轨迹与实机映射、起飞位置和朝向检查、统一启动时刻都有明确流程。[架构](https://docs.skybrush.io/public/skybrush-server-doc/latest/features.html) [轨迹分配](https://docs.skybrush.io/public/skybrush-live-doc/latest/tutorials/show-in-10-steps/assign_drones_to_show.html) [启动时间](https://docs.skybrush.io/public/skybrush-live-doc/latest/tutorials/show-in-10-steps/choose_start_time.html)

## 任务与执行模型

每次群体任务保存固定的车辆与会话集合、任务版本、成员角色、逐机计划、允许参与的条件、失败处理方式和执行记录。编组修改形成新版本，已运行任务保留自己的成员快照。

逐机执行覆盖预检查、排队、发送、等待协议结果、已接受、执行中、目标达成，以及拒绝、发送失败、超时和取消等分支。超时表示缺少足够确认，日志和界面保留实际可见的遥测结果。

本地 OperationId 用于关联业务记录。标准 COMMAND_ACK 没有任意业务事务 ID，匹配必须结合车辆、会话、组件、命令及协议上下文。对同一车辆同一种命令的并发操作进行必要串行化，并验证迟到 ACK 场景。

群体阶段的放行条件由任务显式定义。例如固定编队要求指定成员全部就绪，分区巡检允许在预设最少成员数以上重分配区域。对于未通过检查的成员，界面展示原因并让操作者看到本次实际参与集合。

本系统内的人工控制、任务执行和编队执行明确控制归属。人工接管后暂停对应成员的自动目标派发，再按实际模式和状态确认后续动作。外部遥控或其他地面站的接管需要通过飞控状态变化与具体协议支持识别。

停止后续派发、请求飞控取消、确认飞控已经取消、发起保持或返航，分别使用明确的状态和动作语义。取消本地任务后，持续观察飞机实际状态。

## 编队的演进

第一步使用物理领机与固定偏移，验证成员角色、偏移坐标系、领机数据链路和单机退出。ArduPilot Follow 支持领机身份与空间偏移，具体参数及行为以部署固件为准。[Follow 模式](https://ardupilot.org/copter/docs/follow-mode.html)

第二步引入虚拟参考轨迹与编队槽位。公共轨迹负责整体运动，各成员的槽位定义相对位置。形成队形、队形变换和退出路径均进行速度、加速度及间距约束检查。

特别覆盖领机转弯时外侧成员的速度要求、槽位交换路径交叉、不同 Home 高度带来的相对高度差，以及旧位置数据导致的跟踪滞后。坐标和高度基准在计划与状态中显式保存。

队形切换先求成员到新槽位的映射，再规划满足间距和动力学限制的过渡路径。记录全过程的最小间距、相对位置误差及目标数据年龄。机型能力决定动作，固定翼和 VTOL 阶段需要各自定义保持、转弯与退出方式。

## 蜂群任务的演进

从区域划分和逐机任务分配开始，再加入成员掉队后的剩余任务再分配。记录任务片区、当前承担者、进度和分配版本。

当任务确实要求脱离地面链路继续协作时，将相应任务执行、局部规划和邻机交互放到机载端。通信恢复后先对齐成员、任务版本和完成情况，再恢复协同。

例如区域巡检可以先按面积与续航分配片区，再加入掉队后的剩余区域重分配。目标函数逐步加入飞行时间、航程、剩余电量与载荷能力。分布式分配和邻机避碰在对应机载能力具备后单独验收。

## 验证路线

先验证单机操作在多机环境中保持车辆与会话绑定，再验证群体任务逐机结果，然后验证多机协作和动态编队。

测试覆盖切换焦点、断线重连、拒绝命令、ACK 丢失与延迟、重复链路、遥测陈旧、定位退化、领机退出、人工接管、群体任务部分成功和返航落点分配。每项保存可复现脚本、期望行为和结果。

ArduPilot SITL 官方提供多机启动、自动系统 ID 和初始位置偏移，适合构建这些测试。[SITL 多机说明](https://ardupilot.org/dev/docs/using-sitl-for-ardupilot-testing.html#swarming-with-sitl)

| 阶段 | 交付能力 | 验收重点 |
|---|---|---|
| 1 | 4 机可靠控制、固定目标批量操作、逐机结果与回放 | 焦点变化与重连场景的目标绑定，协议结果正确关联 |
| 2 | 分组任务、逐机计划、阶段屏障和部分失败策略 | 一机拒绝或掉线后的行为可复现，单机可退出与接管 |
| 3 | 物理领机跟随、静态队形、领机退出 | 相对误差、消息年龄、最小间距与退出行为 |
| 4 | 虚拟参考轨迹、动态队形与路径过渡 | 速度和加速度限制、转弯、槽位交换、任务时间误差 |
| 5 | 区域分配、掉队重分配与机载局部协同 | 任务完成率、重复覆盖、重分配耗时及失联后的既定行为 |

群体返航与降落规划逐机等待点、进场顺序、路径和落点。测试记录共享 Home、接近路径和同时下降等具体场景。

## 通信、界面与回放

按链路统计吞吐、往返时间、消息年龄、队列等待和丢包。为控制与关键状态预留带宽，限制参数下载、日志下载与视频对共享资源的占用。多机初次上线时分批同步参数和任务。遥测处理与 UI 重绘解耦。

任务记录同时保留采样时间、接收时间与时钟偏差估计。控制使用经过新鲜度检查的数据，地图展示可做视觉插值。

界面划分为机队准备、任务规划、执行监控、复盘四个工作区。执行页同时呈现地图、异常、群体阶段与逐机结果，控制区持续显示实际作用的车辆集合和数量。

回放记录群体任务版本、固定成员集合、单机动作、协议结果、关键状态变化和人工接管。软件重启后先恢复可见状态并核对飞机实际状态，再处理任务续接。

统计指标包括命令 ACK 延迟分布、超时率、群体任务准备与完成时间、UI 刷新耗时、CPU 与内存、链路负载、编队误差和最小间距。每个数量档分别测量，支持规模由测量结果确定。
