# 架构说明

## 当前产品结构

M-GCS 由 C++ 后端、C FFI 契约和 Flutter 外壳组成。前端不能直接依赖 MAVLink、
飞控状态机、串口实现或任务传输内部类型。

```text
Flutter Shell -> bridge/ffi -> core -> third_party
```

依赖只能向右流动。Core 不知道调用方是谁，Bridge 不拥有业务状态，Flutter 不复制
后端协议和编译逻辑。

## 运行时数据流

```text
ILink
  -> ConnectionManager
  -> MavlinkAdapter
  -> Vehicle / MultiVehicleManager
  -> domain services and telemetry cache
  -> FFI JSON/events
  -> Flutter controller and widgets
```

一次连接创建一个 Vehicle 会话。`mg_connection_open()` 返回 session ID，不改变 UI
焦点；`mg_vehicle_select()` 才改变焦点。旧单机连接入口继续作为兼容包装存在。

## Core 领域

### 连接和协议

`ConnectionManager` 管理链路打开、等待心跳、已连接、断链和清理；
`MavlinkAdapter` 隔离生成的 MAVLink 头；`mavlink_adapter_state.h` 单独承载遥测、
链路质量、日志下载和 ACK 等待状态；`mavlink_adapter_handlers.cc` 承载遥测与命令
消息处理器；`mavlink_adapter_mission.cc`、`mavlink_adapter_params.cc`、
`mavlink_adapter_log.cc`、`mavlink_adapter_command.cc` 承载各自协议辅助；
`mavlink_adapter_dispatch.cc` 承载原始帧兼容解析和分发；`CommandQueue` 负责命令 ACK、
超时和重试；
`ParamCache`、`MissionTransfer` 和 `MavlinkFtpClient` 分别负责参数、任务和文件传输。

`MavlinkAdapter::Impl` 的内部拆分已完成：内部头只保留状态、声明和底层发送函数，
协议实现位于 `core/src/mavlink/` 的独立 `.cc` 文件中。拆分冻结了公共 FFI 和消息
语义，不改变生成代码、JSON、发送顺序或回调时序。

TCP/UDP 读线程由共享 `ReadState` 持有 socket、回调和结束条件，链路对象不会在读线程
尚未退出时被提前析构。ConnectionManager 的 reset 会把阻塞关闭放在锁外，GCS 心跳也
只复制共享引用后在锁外发送。

### Vehicle

`Vehicle` 组合连接、飞行模式、校准、日志、遥测缓存、波形、围栏和载荷能力。
多会话由 `MultiVehicleManager` 管理。安全敏感命令必须明确检查连接和目标，并将
ACK/超时/拒绝传到 FFI。

### 任务规划

任务规划使用 `MissionDocument` 保存可编辑任务实体、revision、Home、默认参数和
命令参数；`MissionPlan` 是编译后的 MAVLink 目标模型，不是编辑状态的唯一来源。

任务编译和校验属于后端，前端只提供编辑和渲染。`survey_grid` / `corridor` 由
`SurveyCalculator` 在后端展开；`.waypoints` 和 QGC `.plan` SimpleItem 互操作也在
后端完成。编辑、校验、保存不写飞控，上传和从飞控读取是显式操作。

`mg_mission_doc_*` 是已经发布的兼容 ABI 名称；它们不改变当前产品术语。当前设计概念
统一称为“任务文档”和“任务规划”。

### 遥测和波形

遥测在 Core 中聚合并记录 freshness。高频波形进入后端环形缓冲，前端通过降采样
接口读取；降采样保留 min/max 包络，不在前端对原始采样做平均或滤波。

### 校准和日志

校准由统一服务和策略驱动，终态必须经过参数读回或飞控报告验证。飞行日志的
STATUSTEXT、连接状态和命令结果在锁外回调；校准历史在锁外写盘。

## FFI 设计

FFI 只暴露 C 兼容类型和稳定 JSON envelope。新增接口必须同时更新头文件、实现、
测试、`API_CATALOG.md`、相关文档和前端绑定。兼容接口保留 ABI，但冻结新行为；
会影响 RTL、起飞、降落、任务和参数的接口必须由 UI 先确认。

## Flutter 设计

Flutter 通过 `ConnectionController` 作为状态门面绑定 FFI。任务文档、遥测、参数、
校准和飞行命令已经分别由 `MissionDocumentFfiBackend`、`TelemetryFfiBackend`、
`ParameterFfiBackend`、`CalibrationFfiBackend` 和 `FlightCommandFfiBackend`
持有动态符号绑定；控制器继续负责生命周期、轮询、状态收敛和 `notifyListeners()`。
任务文档控制器绑定当前 native session；active vehicle 或 session 改变后必须先重新读取
或创建任务，避免跨飞控误上传。
地图坐标按地图源在 GCJ-02 与 WGS-84 之间转换。任务面板使用
`MissionDocumentController`；命令下拉和参数表单来自后端命令目录。UI 统一使用
`MgColors`、`MgTypography`、`MgMetrics` 和 `MgFonts`。

## 平台

- macOS：POSIX 串口，`build/mgcore.dylib`，Flutter debug 构建已验证。
- Windows：Win32 串口，`mgcore.dll`，Windows bundle/preflight 已提供，仍需真机验收。
- Linux：POSIX 串口，`libmgcore.so`。
- Android：Gradle/CMake 构建并打包 `libmgcore.so`，data JSON 走 AssetManager 解压；
  USB Host 串口按 QGC 的 `usb-serial-for-android` + `SerialInputOutputManager` +
  JNI 回调结构接入，权限交互和真机 USB 仍需设备验收。

## 约束

持有 Core 锁时不做阻塞 I/O、不调用外部回调、不等待传输。高风险路径修改后必须
运行聚焦回归和完整 `tools/preflight.sh`。
