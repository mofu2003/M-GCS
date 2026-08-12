# 项目状态

更新日期：2026-07-28。

## 当前结论

M-GCS 已具备可继续开发的 C++ 后端、C FFI bridge 和 Flutter 桌面前端。后端能力覆盖较广，但正式产品链路仍集中在 macOS 串口与 ArduPilot 只读验证，不能把接口存在等同于生产验收。

Flutter 正式入口会加载 MgCore native library。加载成功后，串口连接、遥测、参数同步、参数浏览和参数写入走真实 FFI；加载失败时应用显示明确错误。UDP、TCP 和蓝牙尚未接入 Dart 应用层。

## 已实现

后端：

- 串口、TCP、UDP、内存链路、端口发现和连接生命周期；链路失效后由应用层显式重连。
- MAVLink adapter、命令队列、参数缓存、任务传输和遥测缓存。
- ArduPilot/PX4 profile、飞控模型、多机管理和活动飞控选择。
- 参数、任务、围栏、测绘、校准、日志、ADSB、地形和诊断接口。
- 按 offline、simulation、hardware、manual 分类的 CTest 测试。

前端：

- macOS 和 Windows 桌面工程。
- `ConnectionSessionBackend`、`ConnectionController` 和 Dart MgCore FFI 层。
- 串口连接、参数启动同步、遥测轮询、参数树和参数写入入口。
- 地图、系统定位、连接抽屉、状态栏、姿态仪和后台参数页面。
- 项目自有皮肤契约和默认 `MacGlassSkin`。

## 已知验证

- 2026-07-21：使用 AppleClang、CMake 4.4.0 和 Ninja 1.13.2 配置、编译后端成功；`offline` / `simulation` 门禁为 29/29 通过（18 个 offline、11 个 simulation）。
- 2026-07-21：使用 Flutter 3.44.7 / Dart 3.12.2，`flutter analyze` 无问题，`flutter test` 为 52/52 通过；Dart FFI 测试成功加载本地 `build/mgcore.dylib`。
- 2026-07-21：以 Xcode 26.6 的 `DEVELOPER_DIR` 执行 `flutter build macos` 成功，产物为通用二进制 `app/build/macos/Build/Products/Release/mgcs.app`（arm64、x86_64）。系统默认仍选择 Command Line Tools，日常开发仍建议切换到完整 Xcode。
- ArduPilot Copter 曾完成只读连接、心跳、姿态、固件、参数和部分 servo output 验证。
- CI 定义了 Ubuntu 和 Windows 后端的 offline/simulation 门禁；当前还需要补 MAVLink 自动生成步骤后才能在全新 checkout 上运行。

真实硬件验证未覆盖参数写入、解锁、切模式、任务执行、校准和电机测试。这些能力必须单独设计安全验收。

## 生产缺口

- MAVLink XML 已进入仓库，但 C 生成头不提交；本地生成方式已固定为 `pymavlink 2.4.49`，CI 仍需增加自动生成步骤。
- macOS Xcode phase 与 Windows CMake install 已配置 native library 嵌入；Windows 仍需在真实 Windows 环境验证最终包。
- Windows 前端需要在真实 Windows 环境构建和硬件验证。
- Android/iOS 平台工程、权限、native packaging 和文件系统尚未恢复。
- Android filesystem 当前仍是占位实现。
- 固件下载与刷写当前仍是占位实现。
- 视频接收/解码只有基础结构，没有完整生产验收。
- UDP、TCP、蓝牙、任务、告警和更多飞控配置尚未接入前端工作流。
- CI 未覆盖 macOS、Flutter analyze/test/build 和移动平台。
- 商业地图服务、离线地图、Key 管理和第三方资产授权清单尚未完成。

## 下一阶段

1. 固化 macOS/Windows native library 打包流程。
2. 固定 MAVLink C 头生成方式或提交可复现的生成产物，恢复本地和 CI 构建。
3. 在 Windows 上完成前后端构建、串口连接和硬件只读验证。
4. 把 UDP/TCP 与任务、告警等已存在后端能力接入应用契约。
5. 恢复 Android 平台并完成 filesystem、USB/串口和 native packaging。
6. 将 Flutter 和 macOS 验证纳入 CI。
7. 为任何危险写操作建立确认、能力检查、超时和真实硬件验收流程。
