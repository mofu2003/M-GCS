# 项目状态

更新日期：2026-09-23。当前版本：`0.0.4`。

## 当前结论

M-GCS 已具备可继续开发的 C++ 后端、C FFI bridge 和 Flutter 桌面前端。0.0.4 在不改变三层主架构的前提下，补齐了任务规划、安全配置、飞行日志/飞控文件、多机控制和可替换前端壳层的应用层接缝。后端能力覆盖较广，但正式产品链路仍集中在 macOS 串口与 ArduPilot 只读验证，不能把接口存在等同于生产验收。

Flutter 正式入口会加载 MgCore native library。加载成功后，串口、TCP、UDP 连接、遥测、参数同步、参数浏览和参数写入走真实 FFI；加载失败时应用显示明确错误。蓝牙尚未接入 Dart 应用层。后台已提供 Copter 和 Plane/QuadPlane 机架类型页面，配置值直接通过现有参数写入链路下发。应用层通过 capability contract 发现任务、围栏、Rally、日志、MAVLink FTP 文件、多机和飞行控制能力，前端 shell 可以在不修改 Core 的情况下替换。

## 已实现

后端：

- 串口、TCP、UDP、内存链路、端口发现和连接生命周期；链路失效后由应用层显式重连。
- MAVLink adapter、命令队列、参数缓存、任务传输和遥测缓存。
- ArduPilot/PX4 profile、飞控模型、多机管理和活动飞控选择。
- 参数、任务、围栏、Rally、测绘、校准、日志、MAVLink FTP 文件、ADSB、地形和诊断接口。
- 按 offline、simulation、hardware、manual 分类的 CTest 测试。

前端：

- macOS 和 Windows 桌面工程。
- `ConnectionSessionBackend`、`ConnectionController` 和 Dart MgCore FFI 层。
- 串口、TCP、UDP 连接，参数启动同步、遥测轮询、参数树和参数写入入口。
- 地图、系统定位、连接抽屉、状态栏、姿态仪和后台参数页面。
- 串口、TCP、UDP、蓝牙连接入口使用统一控件规格；蓝牙扫描仍为未启用状态。
- Copter `FRAME_CLASS` / `FRAME_TYPE` 和 Plane/QuadPlane `Q_FRAME_CLASS` / `Q_FRAME_TYPE` 机架配置页面。
- 任务编辑器支持航点添加/编辑/排序、地图坐标插入、校验、JSON/CSV 导出，以及任务上传/下载进度。
- 围栏/Rally 配置支持校验、导入导出和飞控同步；“飞行日志”页面支持浏览飞控内存卡目录、下载和上传文件。
- 多机面板支持发现、切换、选择和批量解锁/上锁/返航/降落/模式切换，并返回逐机结果。
- `SessionCapabilities`、`FleetController`、`SafetyController` 和可替换的 shell/skin contract 让页面不直接依赖 native FFI。
- 中英文 ARB 本地化资源、生成绑定和一致性测试。
- 项目自有皮肤契约和默认 `MacGlassSkin`。

## 已知验证

- 2026-07-21：使用 AppleClang、CMake 4.4.0 和 Ninja 1.13.2 配置、编译后端成功；`offline` / `simulation` 门禁为 29/29 通过（18 个 offline、11 个 simulation）。
- 2026-07-21：使用 Flutter 3.44.7 / Dart 3.12.2，`flutter analyze` 无问题，`flutter test` 为 52/52 通过；Dart FFI 测试成功加载本地 `build/mgcore.dylib`。
- 2026-07-21：以 Xcode 26.6 的 `DEVELOPER_DIR` 执行 `flutter build macos` 成功，产物为通用二进制 `app/build/macos/Build/Products/Release/mgcs.app`（arm64、x86_64）。系统默认仍选择 Command Line Tools，日常开发仍建议切换到完整 Xcode。
- ArduPilot Copter 曾完成只读连接、心跳、姿态、固件、参数和部分 servo output 验证。
- 2026-08-12：CI 已在 Ubuntu/Windows 后端任务中加入 MAVLink 自动生成，并新增 Linux Flutter analyze/test 门禁。
- 2026-08-12：`0.0.2` 发布候选后端 Release 构建成功；离线与模拟测试 29/29 通过（18 个 offline、11 个 simulation）。
- 2026-08-12：Flutter 静态分析无问题，组件与控制器测试 79/79 通过。
- 2026-08-12：Docker Compose 配置解析通过，shell 脚本语法检查通过；镜像拉取阶段因 Docker Hub 匿名认证超时未能完成镜像构建，这不属于项目编译错误，仍需在网络可访问 Docker Hub 的环境补验容器内完整 `check`。
- 2026-09-10：Flutter 静态分析无问题，全部 84 项测试通过；TCP/UDP 连接和统一连接抽屉均有回归覆盖。
- 2026-09-10：后端 Release 构建成功，离线/模拟门禁 29/29 通过；Windows 多配置测试目录已补齐运行时 JSON 元数据。
- 2026-09-10：使用项目本地 Flutter/CMake 工具链完成 Windows Release 构建，生成 `app/build/windows/x64/runner/Release/mgcs.exe` 并安装 `mgcore.dll`。
- 2026-09-23：Flutter 3.44.9 工具链下静态分析无问题，全部 114 项测试通过；新增多机、任务编辑/校验、本地化、坐标转换、参数同步、会话能力和飞控文件回归覆盖。
- 2026-09-23：`flutter build macos --debug` 成功，生成 `app/build/macos/Build/Products/Debug/mgcs.app`；macOS 最低部署版本固定为 12.0。
- 2026-09-23：native 离线/模拟 CTest 29/29 通过；MAVLink FTP 文件浏览、上传、下载和取消路径完成 FFI/应用层接入。

真实硬件验证未覆盖参数写入、解锁、切模式、任务执行、校准和电机测试。这些能力必须单独设计安全验收。

## 生产缺口

- MAVLink XML 已进入仓库但 C 生成头不提交；本地、Docker 和 CI 统一通过 `tools/generate-mavlink.sh` 使用 `pymavlink 2.4.49` 生成。
- macOS Xcode phase 与 Windows CMake install 已配置 native library 嵌入；Windows 仍需在真实 Windows 环境验证最终包。
- Windows 前端需要在真实 Windows 环境构建和硬件验证。
- Android/iOS 平台工程、权限、native packaging 和文件系统尚未恢复。
- Android filesystem 当前仍是占位实现。
- 固件下载与刷写当前仍是占位实现。
- 视频接收/解码只有基础结构，没有完整生产验收。
- 蓝牙、前台完整飞行控制、告警和更多飞控配置尚未接入完整生产工作流；当前飞行控制以应用层命令和基础面板为主。
- 多机批量命令仍通过“切换活动飞控后执行，再恢复活动飞控”的兼容方案路由；native/FFI 尚未提供显式 `vehicle_id` 命令目标。
- 日志文件已经可以浏览和传输，但 DataFlash/TLog 曲线、回放、MAVLink Inspector 和故障报告尚未完成。
- Mission 命令覆盖、测绘相机模型、地形跟随、离线地图包和商业地图授权管理仍不完整。
- CI 已覆盖 Ubuntu/Windows 后端、Linux Flutter analyze/test 和 Windows 桌面 Release 打包；尚未覆盖 macOS 和移动平台。
- 商业地图服务、离线地图、Key 管理和第三方资产授权清单尚未完成。

## 下一阶段

1. 在 Windows/macOS 真机完成串口、TCP、UDP 只读连接和遥测验收，并加入 macOS smoke build CI 门禁。
2. 为多机 native/FFI 增加显式 `vehicle_id` 路由，消除批量命令对 active vehicle 的临时依赖。
3. 完善 Mission CAD/任务命令覆盖、日志分析回放、离线地图和载荷工作流。
4. 恢复 Android 平台并完成 filesystem、USB/串口和 native packaging。
5. 为任何危险写操作建立确认、能力检查、超时、审计和真实硬件验收流程。
