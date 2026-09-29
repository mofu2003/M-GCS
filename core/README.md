# 后端核心模块地图

`core/` 保存地面站后端引擎，负责连接传输层、飞控对象和 MAVLink 协议状态。这里不依赖任何前端框架，也不承担界面展示职责。

## 目录说明

- `include/core/`：后端和桥接层内部使用的公共 C++ 头文件。
- `src/mavlink/`：MAVLink 适配器、发送器、命令队列、参数缓存和任务传输。
- `src/transport/`：串口、TCP、UDP、内存链路、端口发现和连接管理器。
- `src/vehicle/`：飞控模型和多飞控管理器。
- `src/mission/`：MAVLink 任务编辑、航点传输和测绘辅助能力；`mission/cad`
  已包含前端无关的规划文档、revision、undo/redo、命令目录、校验、编译、估算和持久化。
- `src/geofence/`：围栏数据管理和校验。
- `src/calibration/`：传感器校准状态机和各传感器策略。
- `src/firmware/`：ArduPilot/PX4 固件 profile、模式映射和参数重映射。
- `src/settings/` 与 `src/log/`：飞行日志、MAVLink 日志和日志回放。
- `src/sitl/`：SITL 启动和依赖探测。
- `src/video/`：视频流元数据和解码相关骨架。
- `tests/`：后端离线测试和模拟飞控测试。

## 高风险路径

以下路径涉及连接、协议状态机或飞控命令，修改时必须配套聚焦验证：

- `src/mavlink/mavlink_adapter.cc`
- `src/mavlink/mavlink_senders.cc`
- `src/mavlink/command_queue.cc`
- `src/mavlink/mission_transfer.cc`
- `src/mavlink/param_cache.cc`
- `src/transport/connection_manager.cc`
- `src/transport/serial_link.cc`
- `src/transport/tcp_link.cc`
- `src/transport/udp_link.cc`
- `src/vehicle/vehicle.cc`
- `src/vehicle/multi_vehicle_manager.cc`
- `src/calibration/calibration_service.cc`

## 当前平台状态

- Windows：保留 Win32 串口、Winsock 和硬件测试脚本入口。
- Linux：使用 POSIX filesystem、BSD sockets 和 POSIX 串口路径。
- macOS：串口使用 POSIX `termios`；较早环境通过过完整构建和默认测试，当前 checkout 需先生成 MAVLink C 头再重新验证。
- Android：filesystem 当前仍是占位实现，需要 JNI/AssetManager 上下文。

## 开发原则

1. 原始 MAVLink 细节留在后端适配器内。
2. 后端只暴露稳定模型和桥接契约，不向 UI 泄漏协议状态机。
3. 命令、任务、参数和校准链路必须通过测试证明行为。
4. 真实硬件测试工具放在 `tests/manual/` 或顶层 `tests/`，不得进入默认 CI。
5. 跨平台实现必须显式隔离平台 API，不能让 Windows、Linux、macOS、Android 的专有头文件泄漏到其他平台编译路径。
6. 修改 transport 或 connection manager 后，至少运行 `test_transport_links` 和默认 offline/simulation 门禁。
7. Mission CAD 的文档、几何、约束和编译属于 core，不依赖任何具体前端；`MissionPlan` 是其编译输出，而不是 CAD 的源模型。
