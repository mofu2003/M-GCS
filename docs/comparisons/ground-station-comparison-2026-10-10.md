# M-GCS、QGroundControl 与 Mission Planner 多维比较

核对日期：2026-10-10  
比较对象：当前 M-GCS 工作树、QGroundControl 本地源码、Mission Planner 本地源码。  
目的：评估代码所表达的产品能力、飞行安全闭环、工程成熟度和 M-GCS 的改进顺序，不比较营销描述或某次预编译发行版。

## 1. 比较基准与读法

| 项目 | 本地路径 | Git 基准 | 工作树状态 | 主要实现栈 |
| --- | --- | --- | --- | --- |
| M-GCS | `/Users/fansicheng/Projects/M-GCS` | `5476b0c727eada9913e8826d4b4caff44f7cecf3`，2026-10-10 | 有未提交清理、安全与接口门禁修改 | C++17 core + C FFI + Flutter/Dart |
| QGroundControl | `/Users/fansicheng/Projects/qgroundcontrol` | `bf0096d974dc2792556c61ca783779083d9de41c`，2026-10-05 | 干净 | C++20 + Qt 6/QML |
| Mission Planner | `/Users/fansicheng/Projects/MissionPlanner` | `a2fcd74d64513d0d8fb43fbdd72020fa75aaa974`，2026-07-11 | 有未跟踪 `.DS_Store`，未纳入比较 | C#/.NET Framework 4.7.2 + WinForms |

M-GCS 的当前工作树不是纯 `HEAD`：它包含用户正在进行的收尾改动。本文按该工作树代码判断，并将新改动尚未进入提交的事实保留在证据边界中。QGC/MP 的结论来自本地源码、项目构建配置和测试布局；没有在本轮构建这两个上游工程，也没有接飞控做硬件验证。源码中存在 API、类或按钮只证明有实现入口，不自动等于功能完整、默认启用或所有固件都支持。

### 成熟度标记

- **深**：前端工作流、后端状态、错误/超时路径和对应验证基本闭环。
- **中**：主要流程存在，但某些固件、失败路径、持久化、平台或测试仍有限。
- **浅**：仅有模型、FFI 声明、占位代码或零散 UI，不能按完整用户功能计。
- **未核实**：本地代码扫描无法证明行为；需要目标平台构建、上游硬件或人工操作验证。

## 2. 总体判断

| 维度 | M-GCS | QGC | Mission Planner | 结论 |
| --- | --- | --- | --- | --- |
| 可交付功能广度 | 任务、参数、校准、遥测和连接已形成主路径；大量 FFI 能力尚未进入 UI | 广覆盖、多固件、多平台，包含规划、飞行、设置、视频、日志和扩展 | ArduPilot 工作流最深，配置、规划、日志、固件和飞行工具非常广 | M-GCS 还未达到任一上游的整体产品覆盖 |
| 飞行安全设计 | 安全规则最明确，覆盖 ACK、确认、过期状态、任务文件和取消；规则到实现仍有落差 | 安全 UI/固件插件/多链路成熟，但特定输入设备或旧状态路径也有例外 | 操作工具丰富且常直接面向 ArduPilot；确认、错误展示和状态建模不一致 | M-GCS 的规范质量突出；须把规范全面变成代码级门禁与端到端验证 |
| 架构边界 | 清晰的 Flutter → FFI → core 分层，C FFI 有意稳定 | 最完整的成熟抽象：Vehicle、Fact、FirmwarePlugin、LinkManager、MissionManager、QML 元对象系统 | 功能积累深，但历史全局状态、界面、协议和固件逻辑耦合较多 | M-GCS 在小规模模块边界有优势；QGC 在边界的覆盖面、扩展经验和验证深度领先 |
| 多机 | 有独立 Vehicle/session、共享链路和批量工作流 | 一等产品能力，车辆、链路、焦点与选择模型已有长期演进 | 可管理多连接及多种 ArduPilot 设备，但 UI/状态存在历史全局化 | M-GCS 与 QGC 架构目标接近，仍要继续验证重复 sysid、重连、断链、并行写入等边界 |
| 工程治理 | 有清晰约束、37 个后端门禁和 Flutter 测试；本次新增 FFI 符号对齐与真实 smoke | C++20、强 lint、跨平台 CI、测试框架、sanitizer/coverage/hardening 选项 | Windows MSBuild 主路径和多平台旁支，构建/测试一致性较弱 | QGC 的自动化治理最强；M-GCS 比较容易加强但仍欠跨平台全矩阵 |
| 平台适配 | 源码覆盖 macOS、Windows、Linux、Android；实际验证按主机分布 | 官方代码/CI 目标 Windows、macOS、Linux、Android、iOS | 主路径是 Windows 桌面；Android 可用，Linux 经 Mono 有限制，macOS/iOS 实验性 | M-GCS 的跨端目标接近 QGC，但本地通过 macOS 门禁不代表其他平台已交付 |

**最准确的定位**：M-GCS 是“功能逐步补齐中的新架构地面站”，不是已经达到 QGC/MP 覆盖率的替代品。它当前最有价值的差异是把 ACK 结果、过期状态、危险动作确认、任务文档和可取消操作写成明确的安全契约；最大差距是功能闭环、跨平台实测和安全规则全覆盖。

## 3. 多维评分矩阵

分数是源码审计下的相对成熟度，不代表安全认证或飞控兼容保证。5 为该维度证据最完整；M-GCS 的“小而清晰”不因代码量较少而自动拿高分。

| 维度 | M-GCS | QGC | MP | 为什么 |
| --- | ---: | ---: | ---: | --- |
| 核心飞行/连接闭环 | 2 | 5 | 5 | M-GCS 主闭环有实现，但固件、输入、外围工具覆盖仍少 |
| ArduPilot 深度 | 2 | 4 | 5 | MP 的参数、校准、日志、规划和专用设备最深 |
| PX4 深度 | 2 | 5 | 2 | QGC 的 PX4 插件/UI/测试体系更完整；MP 仓库的主定位是 ArduPilot |
| 任务规划/数据格式 | 2 | 5 | 4 | M-GCS CAD 文档模型良好，但复杂任务项和前端覆盖较少 |
| 多机/共享链路 | 3 | 5 | 3 | M-GCS 已实现真实共享链路；QGC 的多机使用历史和 UI 覆盖更成熟 |
| 参数语义/元数据 | 2 | 5 | 5 | M-GCS 读取和编辑能力在发展中，验证写入仍有缺口 |
| 校准流程 | 3 | 4 | 4 | M-GCS 有明确取消/超时和 PX4 分支，但多实例、跨源过滤仍需闭环 |
| 日志/分析/回放 | 2 | 4 | 5 | M-GCS 有飞行日志/MAVLink FTP 能力；完整用户链路较少 |
| 视频/载荷 | 2 | 5 | 4 | QGC 有跨平台 GStreamer 管线；MP 视频和载荷工具较实用；M-GCS 已在 macOS 开发构建闭环本地播放/录制/截图，但仍只有一端、且不在默认发布构建内 |
| 地图/地形/离线 | 2 | 5 | 5 | M-GCS 地图源固定、离线数据弱；两者拥有更成熟缓存和地理工具 |
| 安全契约清晰度 | 5 | 4 | 3 | M-GCS 文件化规则最清楚，但偏差清单显示仍未全部实现 |
| 安全行为自动验证 | 3 | 4 | 2 | M-GCS 有仿真与规则门禁；上游测试量/平台矩阵更大，MP 测试集中度较低 |
| 架构可读性与边界 | 4 | 5 | 2 | M-GCS 分层清楚；QGC 抽象成熟但规模巨大；MP 历史耦合更强 |
| 构建/CI/lint 完整度 | 3 | 5 | 2 | QGC 有强 CI/静态检查/安全选项；MP CI 主轴偏 Windows build；M-GCS 持续加强 |
| 跨平台产品成熟度 | 2 | 5 | 2 | M-GCS 多平台代码存在，但平台矩阵实测仍少 |
| 扩展与设备生态 | 1 | 5 | 5 | QGC plugin、Vehicle Component 和 MP 插件/自定义工具生态占优 |
| 许可/再分发清晰度 | 4 | 3 | 2 | M-GCS MIT 简洁；QGC dual license 需选路径；MP GPLv3 衍生与再分发义务较重 |

## 4. 功能覆盖：按用户工作流比较

| 工作流 | M-GCS | QGC | MP | M-GCS 的实际差距 |
| --- | --- | --- | --- | --- |
| 连接与发现 | 串口/TCP/UDP；Vehicle 会话和共享链路；Windows/macOS/Linux/Android 适配源码存在 | 串口、UDP、TCP、蓝牙、日志回放、模拟链路；自动连接、链路健康和多链路模型 | 串口/TCP/UDP/WebSocket/BLE/WinUSB/文件回放及自动连接，外设集成广 | 缺稳定的链路恢复 UX、连接配置完整性、Android/Windows 实机验证；旧同步连接入口与新多 Vehicle 路径并存 |
| 飞行状态与指令 | arm/disarm、模式、RTL、takeoff、land、goto、orbit、ROI 等 C++/FFI 主路径，主要 UI 动作用 submit/poll 等 ACK | Guided Actions、工具栏、手柄、任务控制、firmware plugin 语义转换 | 丰富的 FlightData、Actions、模式快捷操作、脚本化控制 | 仍需逐命令对照：哪些能力只有 FFI、是否有 ACK、状态回读、模式/机型约束、批量路径是否同样做 freshness/confirmation |
| 任务编辑/上传/执行 | MissionDocument 有导入/导出/验证/编译/撤销重做/任务和围栏集结点；计划失败可回滚 | 专用 Plan View、复杂任务项、survey/structure scan、terrain altitude、resume、battery/time estimate | FlightPlanner 长期支持航点、任务编辑、航线工具和 ArduPilot 专属动作 | M-GCS 命令目录和复杂任务类型、相机触发、勘测覆盖率、固件差异校验、现场工作流仍远少于上游 |
| 文件互操作 | 通用内部文档与格式接口，当前重点在 QGC plan/任务数据路线 | 原生 `.plan`/地理任务与完整复杂项工作流 | 多种导入导出、WP/WPL/KML/其他任务文件长期兼容 | 要逐格式检查 round-trip、未知字段保存、单位/高度基准、frame/command 不丢失；仅有 import/export API 不足以认定互操作成熟 |
| 参数调试 | 参数列表、元数据、描述、分组、参数文件与配置页面已有；部分 FFI 尚未连接 UI | Fact System 统一模型：类型/枚举/位掩码/元数据/组件/默认值/持久化，PX4/APM 配置页面集成 | 参数树、搜索、元数据缓存、批量写入、参数文件和 ArduPilot 配置页丰富 | M-GCS 当前主要短板是写入 ACK/回读、参数刷新代际隔离、元数据覆盖率、firmware-specific presets 及参数大规模 UI 性能 |
| 校准与机体设置 | 陀螺、加速度、罗盘、气压、遥控器策略；取消和 30 s 停滞超时；PX4 的能力差异提示 | Sensors/Radio/Power 等 setup wizard，与 Vehicle/firmware plugin 深度衔接 | ArduPilot 校准、RC、ESC、电机、机体特有配置工具很多 | M-GCS 需要验证多罗盘混合成功/失败、消息来源、取消 ACK/失败、断链中断、PX4 与各 AP 机型行为 |
| 飞行前检查 | health/prearm/firmware/传感器数据有 core/FFI 和配置页面 | Health and Arming Checks、firmware setup 和多种传感器健康模型 | Pre-arm 消息、状态与大量 ArduPilot 专属 setup 页面 | M-GCS 从数据字段到可操作错误说明、修复建议、机型覆盖仍需实测，不宜把存在 prearm API 等同于完整 preflight UX |
| 地图与地理数据 | flutter_map；五个预设瓦片源、坐标系转换、机体/航线显示和地图动作 | Qt Location provider、离线/缓存/地形/geo fence/rally/多种复杂任务地图工具 | GMap.NET provider 丰富、用户选择源、地形/高程、WMS/WMTS、离线地图缓存与 GIS 插件 | 缺用户自定义源、缓存/离线包、地形覆盖可视化、地图数据管理和大规模要素交互 |
| 遥测、仪表、告警 | C++ telemetry cache/freshness、波形环形缓冲、ADSB、Flutter 仪表；过期态门控正在完善 | 大量 Live Fly View 仪表、MAVLink Inspector、vehicle messages、link/health 模型 | FlightData HUD、数据状态、地图 overlay 和可配置 telemetry | M-GCS 当前仪表和告警种类少；需测端到端频率、延迟、丢包、UI rebuild、后台计时器和 stale 状态边界 |
| 日志与事后分析 | 本地飞行事件日志、MAVLink log、FTP API 有一定实现；用户界面/导出/分析闭环较浅 | `.tlog`、MAVLink Inspector、log replay 与日志功能 | `.tlog`/`.rlog`、机载 DataFlash 下载和 LogAnalyzer 深入 | M-GCS 需补完整 DataFlash 列表/下载/取消/断点/校验/大文件管理和可视化分析 |
| 视频、相机、云台 | camera/gimbal/video FFI 与控制页（S2）；本地 FFmpeg(LGPL) 解码 + 录制/截图（S3/S4），已在 macOS 开发构建用真实 TCP/MPEG-TS 码流端到端跑通（取帧、录 `.mkv`、截 `.png`）；默认构建仍 FFmpeg OFF | GStreamer 视频流显示/录制、camera/gimbal 组件和流信息 | GStreamer、多画面/浮动视频、相机和云台入口 | M-GCS 本地播放/录制/截图已在 macOS 一端闭环，但还不是可与上游相比的完整视频产品：默认构建未打包解码后端、硬解、多画面、断流重连、多平台编解码与真实相机/云台硬件回读仍缺 |
| 固件、更新、SITL | 固件识别和 SITL 启停有实现；firmware flasher 当前源码明确为占位失败 | PX4/APM firmware plugin、固件设置和平台工具支持，但刷写受固件/平台限制 | ArduPilot firmware installer、板卡/固件下载和 SITL 工具是核心优势 | M-GCS 没有可交付的刷写闭环；占位 FFI 需标为 unsupported，避免产品显示成可用功能 |
| 遥控器、手柄、操纵 | RC 遥测/校准 API 有；完整 joystick 输入和动作绑定尚未形成与 QGC/MP 相当路径 | Joystick 配置、按钮映射、flight mode/command 映射和校准 | Joystick、游戏手柄和动作配置较完整 | 对安全操作要支持 deadman、失联处理、模式/按钮冲突和输入监控；这不是一个“后续 UI 小功能” |
| 外部设备与插件 | 固件插件模型、sensor decoder、摄像头/载荷 API 基础已存在 | FirmwarePlugin、AutoPilotPlugin、Fact metadata、QML plugin 和 Qt 生态 | ArduPilot 专用扩展、脚本、插件、地面站周边工具 | M-GCS 领域模型比 MP 清晰，但设备协议覆盖、插件 ABI/生命周期、版本兼容及用户扩展生态尚浅 |

## 5. 飞行安全、数据安全与故障闭环

| 安全议题 | M-GCS | QGC | MP | 评估 |
| --- | --- | --- | --- | --- |
| 危险命令确认 | S3 明确要求 UI 入口确认；顶栏与地图用长按/确认组件，控制器层应阻止漏接 callback 时静默执行 | Guided 操作和模式/arm 有确认路径；手柄动作存在有意的直接操作例外 | 常规快捷操作较直接；不同按钮和 Actions 菜单确认不完全一致 | M-GCS 规则最明确；仍要枚举 arm、disarm、mission start、fleet batch、map、keyboard/gamepad 的全部入口 |
| command ACK 作为最终结果 | 主路径 submit/poll 等 ACK；拒绝结果传给调用方；发送失败回调本轮刚补 | MavCommandQueue 对 ACK 追踪成熟，按 vehicle/command 管理，具体匹配规则以本地版本为准 | wait/retry/ACK 在 MAVLinkInterface，很多错误向 UI 报告仍取决于调用流程 | M-GCS 原则严格但旧 FFI/高阶入口还需清理；不要把“发包成功”误显示成执行成功 |
| ACK 来源隔离 | M-GCS command queue 已带 sysid/compid；mission ACK 校验 type、target 和活跃事务 | 每个 vehicle/链路有上下文，不能只用抽象类存在推断所有相同 sysid 场景；本地源需针对行为逐条测 | 主要 command ACK 匹配 sysid/compid/command，mission transfer 按目标 sysid 过滤 | M-GCS 在共享链路 mission 防串扰上有明确专门处理；相同命令、同一 sysid 多 endpoint 与 ACK generation/ticket 仍是难点 |
| 断链与过期状态 | 用户工作树新增 `vehicleStateFresh` 和 UI/backend 门控；native armed 与旧 FFI snapshot 尚有残留（KD-16） | link health/communicationLost 显式通知；并不会简单把真实状态改写成 disarmed | 通信中断警告机制较薄，保留最后状态属于可观察行为 | M-GCS 命令门控方向更保守；要把 freshness 作为域模型字段贯穿 core→FFI→fleet summary，而不只在 Dart |
| 重试与超时 | safety 规则要求所有长操作限时；任务 ACK 约 1.5 s 状态机，校准停滞约 30 s；命令按类别重试 | 命令队列、mission manager、link watchdog、重连 backoff 等不同层拥有专门时限 | command 默认重试与专用 timeout 广泛存在，但部分操作同步等待且结果只日志化 | 不能简单用超时更短代表更安全；按命令副作用、ACK 语义和重复执行安全性定义策略 |
| 取消语义 | S8 要求取消下发飞控命令且确认前不能显示停止；校准类型区分固件支持 | APM 校准取消有专门流程；PX4 部分校准明确不支持立即取消 | 许多校准/任务传输的取消能力不统一 | M-GCS 的契约方向优于假装取消；仍要测发送失败、ACK NACK、断链及设备不支持分支 |
| 任务文件完整性 | MissionDocument 为内部状态，导出/保存/上传分离；上传/清除失败回滚 | 本地待写任务与已接受任务分槽处理，成熟文件/任务转换 | 广泛支持任务格式；当前 checkout 的 `setWP`/`setWPint` 在返回拒绝结果前会写本地缓存，存在 rejected item cache coherence 缺陷 | M-GCS 有强数据安全设计；要补复杂项未知字段 round-trip 与 schema migration；MP 该缺陷见第 13 节 |
| 参数写入安全 | 约束希望以 PARAM_VALUE/PARAM_ERROR 确认和回读；当前偏差登记仍显示写入确认有缺口 | Fact 写入等待、回读/拒绝语义和 UI 错误链更成熟 | 参数设置有成熟专属 metadata/workflow，但各路径验证一致性不同 | M-GCS 应在扩展参数 UI 前先建立 write transaction/generation、目标隔离、回读差异和重试防重 |
| 终止/急停 | API 有不可逆 `DO_FLIGHTTERMINATION`，UI 决定不暴露；forced disarm 是另一个概念 | 全仓有意不提供 flight termination 面向普通操作入口，提供确认后的常规 guided/arm 操作 | 有 terminate flight 的 Action 入口并确认 | M-GCS 的不暴露决策保守，但需确保“急停”术语只指可恢复 forced disarm，避免操作者误解 |
| 外部资源/数据 | MAVLink signing key 本地保存、按 vehicle 绑定；还应持续审查 secret 日志、备份和错误输出 | signing、settings、日志及离线资源有成熟产品面 | 支持大量外部下载/数据源；README 明确列出遥测/更新外部服务及禁用选项 | M-GCS 可通过少外部服务降低隐私面；QGC/MP 用户需要明确在线依赖、日志文件、地图请求和签名密钥存储策略 |

### 安全总体判断

M-GCS 有一套独立于 UI 的明确安全规则，涵盖拒绝传播、ACK 成功定义、危险动作确认、超时、freshness、任务文件和取消。三者中，**规则表达与代码审查清晰度** M-GCS 有优势；但规则文件里仍有多项待修复偏差，而且“单测通过”未覆盖所有入口。QGC 的优势是很多安全逻辑已被长期放在 Vehicle/FirmwarePlugin/Link/Mission manager 内，并有专门的健康与 arm-check 呈现。MP 的优势是 ArduPilot 专业用户可直接触达深层功能，代价是错误语义、线程阻塞、全局上下文和确认一致性需要逐功能审查。

安全结论不应汇总成一个“谁绝对安全”的排名：错误的模式值、固件差异、双链路时状态选取、取消是否真的到达飞控、地图高度基准、状态 stale、命令重复重试都是具体的风险域。当前建议把 M-GCS 的 KD-01/02/04/05/06/16/20/21/22/23/24/25/28/29/31/32/33/35 等记录逐个核对，本报告对受本次有限源码抽查之外的条目不作关闭判断。

## 6. 架构、扩展与代码质量

### M-GCS

- 依赖方向清楚：Flutter → C ABI → C++ core；C ABI 是可替换前端的稳定边界。C++17、Flutter/Dart 在 [CMakeLists.txt](/Users/fansicheng/Projects/M-GCS/CMakeLists.txt:1) 和 [pubspec.yaml](/Users/fansicheng/Projects/M-GCS/flutter_shell/pubspec.yaml:1) 中声明。
- 在产品规模较小时，Vehicle/session、firmware adapter、MissionDocument、calibration strategy、telemetry cache 分区可读性较好；最近补充的 FFI directory/symbol/version checker 有助于减少 Dart/Native 漂移。
- 主要结构风险是 `ConnectionController` 仍承载连接、轮询、多机、任务调度、遥测派发、参数和生命周期；C++ core 的模块依赖仍有已登记循环和锁/回调债务。FFI 导出数量大而前端绑定覆盖有限，命名别名和旧 ABI 增加兼容维护面。
- Video、firmware flashing、部分 payload/PX4 configuration FFI 有声明但不是完整面向用户的产品路径。判断“有接口”不能计入满功能。

### QGroundControl

- 大型 Qt 产品通过 Vehicle、LinkManager、FirmwarePlugin、AutoPilotPlugin、FactSystem、MissionManager、QML 控制层形成成熟域边界；QML properties/registration 让界面状态进入 Qt 的响应式对象系统。
- 多固件差异由插件和参数/Fact 元数据共同承担，且高层任务编辑不是简单的 MAVLink waypoint 列表；复杂任务、地形、估算、resume 与工具栏状态有专门模型。
- 代码量巨大、Qt/C++/QML/CMake/GStreamer 等栈复杂，构建环境昂贵，加入新流程需理解大量模型与信号生命周期；但本地版本具备 C++20、强 CI、静态检查、sanitizer/coverage/hardening 选项和广泛测试目录。
- 本地 QGC 指南明确规定 Fact、Multi-Vehicle、Firmware Plugin 和 QML Integration 模式，并有 clang-format/tidy、ruff、pyright、qmllint、clazy 等门禁；这是 M-GCS 可借鉴的治理范例，而不是机械复制 Qt 结构的理由。

### Mission Planner

- 以 ArduPilot 操作能力为优先：MissionPlanner 主工程是 .NET Framework 4.7.2 WinForms，协议、设备配置、飞行显示、日志、地图和插件历史较长；有大量专门面向 ArduPilot 的参数、校准、DataFlash、SITL、板卡和地面站工具。
- 具体优势来自积累和直接可调性，不能用源码结构更旧就推断它缺少某项能力。它含大型 C# 工程、众多子项目/第三方库、全局或共享状态、平台 UI 与固件专用代码，新增通用抽象和系统性静态门禁不如 QGC 一致。
- README 的主构建路径为 Windows Visual Studio/MSBuild；Linux 通过 Mono 有功能限制，macOS/iOS 被描述为实验性。因此它的产品覆盖宽，但平台一致性不是 QGC 的水平。
- 用户可通过大量配置页、脚本/扩展及地图/日志工具做深度 ArduPilot 运维；与 QGC 的通用 Vehicle/FirmwarePlugin 体系相比，跨固件抽象不是 MP 的设计中心。

### 代码质量不是代码量排名

| 观察点 | M-GCS | QGC | MP |
| --- | --- | --- | --- |
| 域模型/依赖 | 层次清楚，规模仍小；核心规则与几处依赖约束尚未完全兑现 | 抽象广、长期演进、多固件约束明确；模块和异步信号复杂度高 | 功能成熟、历史积累和全局状态较多，局部改动可能跨层 |
| 测试 | 当前完整门禁 37 个 CTest offline/simulation 加 Flutter 测试；更多硬件/跨平台验证仍需 CI/设备 | 独立测试框架覆盖 Vehicle、Fact、Mission、Comms、Firmware、QML、Fuzz、Portable 等多个域 | 仓库有若干测试/CI，但核心 WinForms 功能的自动化覆盖密度和统一度偏低；不能把 `test` 文件夹存在等同全面验证 |
| 静态检查 | clang warning、UI token、FFI 声明/绑定检查；部分禁区/架构规则仍是文本审查 | 明确的强制 lint/pre-commit、warnings-as-errors 和安全/覆盖开关 | 主要构建和编译验证集中于 Windows；当前项目文件关闭常规 analyzer 项 |
| 可重现构建 | CMake/Flutter lockfile 清晰；Python MAVLink 生成和平台 SDK 需准备 | CMake preset + Qt/GStreamer/Android SDK 多工具链；依赖精确且重型 | Visual Studio、.NET Framework、NuGet、子模块和 Windows 工具链为主 |
| 接口演进 | C ABI 替换前端方便，但导出 surface 已大，必须用 ABI 契约约束 | C++ QObject/Fact/QML 接口规模大，通过模块内部 contract 演进 | C# 项目内部调用直接、插件方便，但跨模块 API 边界与兼容规则分布较散 |

## 7. 平台、部署、许可证和外部依赖

### 平台

- **QGC**：本地 README 和 `.github/CONTRIBUTING.md` 声明 Windows/macOS/Linux/Android/iOS；构建依赖 Qt 6.11 系、GStreamer 和各平台 toolchain。CI/预设涵盖桌面、移动和 ARM 路径，但不代表每个 feature 在每个 target 完全相同。
- **M-GCS**：当前代码和 CI 包括 Windows、macOS、Linux、Android；iOS 未列为目标。CMake 能构建 core，不等于 UI 的地图、串口 USB、窗口和 native assets 在所有桌面一致。当前本机验证为 macOS，Linux/Windows/Android 实际包体仍由 CI 提供证据。
- **MP**：项目主 TargetFramework 是 `net472`、WinForms；官方 README 的主编译方式是 Visual Studio。Android 可用，但 Linux/Mono 有限制，macOS/iOS 属实验性支路。

### 许可证

- M-GCS 项目主许可证为 MIT，适合保持轻量可再分发；字体有单独条款，第三方依赖仍需各自 notice。
- QGC 本地 `.github/COPYING.md` 说明其代码 dual-license 为 Apache-2.0 或 GPL-3.0，Apache 路径有 Qt 商业许可条件，选取源码时必须逐文件核对归属/notice，不应直接移植 GPL 片段到 MIT 工程。
- MP 根 `COPYING.txt` 为 GPLv3。研究行为和协议可行；复制其实现代码到 M-GCS 会形成许可冲突，须避免。

### 外部依赖和运维

MP README 详细列出固件站点、更新 CDN、参数/日志元数据、地图、高程与遥测统计等外部服务，并描述缓存/离线资源位置。QGC 的依赖更集中在 Qt/GStreamer/平台 SDK 和所选地图/视频源。M-GCS 目前组件较少、服务面较小，但地图 tile URL 仍写在代码中；要走向独立使用，需要用户可管理 provider、归因/条款、安全存储和离线缓存策略。

## 8. M-GCS 在代码层已经有、但不应误算为已交付的能力

当前 API 面覆盖比实际 UI 更宽。静态扫描显示 314 个公开导出中仍有大量函数未被 Flutter 使用或测试触达（本节数字为 2026-10-10 快照；0.2.1 已删除 34 个历史遗留导出，公开导出降为 280，见 `CHANGELOG.md`）。以下面向用户的闭环需要在 roadmap 单独列项：

| 领域 | 代码已有的迹象 | 仍需证明的部分 |
| --- | --- | --- |
| Firmware flash | `mg_firmware_flash`、状态 API、FirmwareFlasher 类型 | core 实现当前直接返回 unsupported；没有文件选择、固件来源/签名校验、板型约束、进度/取消和真实刷写验收 |
| PX4 专属 setup | PX4 FFI 读写声明和若干传感器支持 | 与 QGC PX4 setup 页对应的参数/安全/调参面、每个字段语义和真实 PX4 测试 |
| Camera/Gimbal/Video | camera/gimbal/video FFI；macOS 开发构建已端到端跑通本地解码 + 录制 + 截图（LGPL FFmpeg，含容量上限清理） | 仍在 macOS 之外的一端复现；硬解、跨平台编解码、多画面、断流重连、真实相机/云台硬件自动发现与设备回读、发布包内打包 FFmpeg（R6） |
| DataFlash/log download | MAVLink FTP、log list/download/record API | 完整可见的用户流程、磁盘/空间/续传、巨大日志、文件校验、解析/图表和错误恢复 |
| Survey/terrain | survey generator、terrain query、任务估算符号 | 规划表单、相机模型、航线参数、地形源/缓存、实时校验、文件 round-trip 和与上游结果对照 |
| Map providers/offline | 多个 tile URL enum 和 path provider | 自定义 URL/WMS/WMTS、令牌安全、缓存上限、离线包、版权 attribution、不同坐标体系一致性 |
| 参数与配置 | 参数 FFI/元数据/配置页面 | 对所有导出能力提供前端入口或正式标为 internal/deprecated；写入 ACK/readback、下载中止、参数版本冲突 |
| Joystick | MAVLink command/RC models | OS joystick 枚举、校准、按钮动作、deadman、丢失输入、抢占/多机焦点保护 |

## 9. 按场景选型，而非绝对排名

| 使用情境 | 当前更合适 | 原因/条件 |
| --- | --- | --- |
| PX4 与 ArduPilot 混合、桌面到手机、视频/规划一体 | QGC | 平台/固件插件成熟、主飞行和任务/设置完整；具体机型仍应核对版本与插件能力 |
| ArduPilot 深度调参、板卡配置、DataFlash 分析、现场工程工具 | Mission Planner | ArduPilot 专用工具和长期工作流最完整；偏 Windows，扩展能力强 |
| 需要可替换 Flutter UI、明确 C ABI、MIT 派生约束，并愿意承担成熟度建设 | M-GCS | 分层和安全契约适合作为产品基线；现阶段不应把它当成 QGC/MP 全功能等价替代 |
| 对高风险飞控操作要求“失败不得显示成功、断链状态必须未知、任务文件不得静默覆盖” | M-GCS 的规则模型可借鉴，运行任务优先用已完成实机验证的软件 | M-GCS 的规范写得严格，但仍有未关闭偏差，不能以文档代替飞行验证 |

## 10. M-GCS 改进优先级

### P0：继续关闭飞行安全与协议闭环

1. 将 armed/mode/landed/position freshness 从 core 状态模型一路带到 FFI 和每架飞机摘要；不能只在 Flutter UI 做 stale 门控。
2. 命令以 `(vehicle/session, sysid, compid, command, generation/ticket)` 关联；禁止同目标相同命令歧义，拒绝码、超时、发送失败、IN_PROGRESS 和取消结果均有独立状态。
3. 校准取消维持 `cancelling`，必须看到可证明的 ACK/结果或明确失败；按 sysid/compid、校准类型、attempt generation、多罗盘实例过滤消息。
4. 参数写事务按目标、参数名、类型、发送值与 request generation 匹配回读；连接切换/刷新/旧响应不允许污染新会话。
5. 枚举所有命令入口（顶栏、地图、任务、批量、键盘、手柄、旧 FFI），把 confirmation 和 stale gate 纳入自动化测试。

### P1：把“API 覆盖”转换成可靠的用户功能

1. 建立产品能力矩阵：每个 FFI 标为 `UI-backed / internal / unsupported / deprecated`，拥有者、固件矩阵、错误路径和测试路径。
2. 首先补日志/DataFlash、离线地图/自定义源、terrain-aware planning、firmware flashing、安全与能力检测、joystick；按用户真实需求选优先级，不应同时铺开。
3. MissionDocument 做格式 round-trip、未知字段兼容和 schema version；把 QGC `.plan`、WP/WPL、KML/GeoJSON 与 MP 实际工作流按样本对测。
4. 视频明确区分“收到 MAVLink 视频流信息”“可以解码并显示”“可以控制 camera/gimbal”“可以录制”；按平台做 capability matrix。

### P2：提高工程成熟度

1. QGC 借鉴强制 clang-format/tidy、模块测试、fuzz、sanitizer 和跨平台 CI；避免把 QGC 全栈依赖搬入 M-GCS。
2. M-GCS 给所有危险状态和协议约束增加从 API 到 Flutter 的集成测试，并建立 replay fixtures，不能只做单元状态机测试。
3. 做真实 Linux、Windows、Android 包和 native symbol 检查；CI 中实际跑更新后的 `preflight` 等价全链路。当前 macOS build 不代表平台 parity。
4. 把用户可见的未实现项从 UI 隐藏或明确为 unsupported，避免声明/菜单/FFI 造成“功能已支持”的错觉。
5. 维护标准 MAVLink/mission fixtures、ArduPilot/PX4 SITL 版本矩阵和可复现的模拟场景，并记录固件版本、工具版本和运行结果。

## 11. 建议的验收矩阵

每个 P0/P1 功能至少回答并留下自动化/人工证据：

| 维度 | 最低验收证据 |
| --- | --- |
| 飞控与机型 | ArduCopter、ArduPlane/QuadPlane、Rover、至少一个 PX4 机型；列明 unsupported 能力 |
| 链路 | UDP/TCP/串口模拟；单链路、多 vehicle、断链、重连、切换焦点、重复 sysid 场景 |
| ACK/拒绝 | ACCEPTED、TEMPORARILY_REJECTED、DENIED、UNSUPPORTED、IN_PROGRESS、无 ACK、发送失败、延迟旧 ACK |
| 数据状态 | 初次未知、有效、过期、恢复、切换 Vehicle、后端/前端重建 |
| UI 行为 | 确认前无命令、取消无命令、确认后仅一条命令、失败不显示成功、危险入口可禁用 |
| 文件 | 保存/加载、未知字段、格式转换、上传成功/拒绝/超时/取消前后文件哈希与 plan revision |
| 平台 | macOS、Windows、Linux、Android 各自的 clean build、bundle/native library load、启动和核心 workflow |
| 性能与资源 | 遥测高频、参数全量、波形多实例、地图长时间运行、关闭/重连后内存和线程退出 |
| 发布/许可 | 版本/CHANGELOG、公开符号、第三方 notice、字体/地图 provider 条款、签名密钥不进入日志 |

## 12. 证据边界与结论

- QGC：本地源码 `bf0096d9`，工作树干净；比较时引用其架构指南、README、CMake preset/config、相关源码与测试目录。
- Mission Planner：本地源码 `a2fcd74d`，有一个未跟踪 `.DS_Store`，忽略该文件；比较其 README、.NET project、CI 和相关 C# 源码。
- M-GCS：用户工作树含未提交清理/安全修复；比较当前文件，不假设这些改动已经提交或已部署。
- 本次未构建 QGC（依赖 Qt 6.11.1、GStreamer、对应平台 SDK）；未在当前 macOS 环境构建 Mission Planner（主工程要求 Windows/.NET Framework/WinForms）；也未连接任何真实飞控。对上游功能的判断限于当前源码路径、测试/构建配置和 first-party 项目文档。
- 各仓库文件数/LOC 不能公平表示产品规模：QGC/MP 带有资源、多语言、第三方依赖、平台产物和插件；所以本报告不以 LOC 推断缺陷率或质量。
- 本地统计：M-GCS 321 个公开 FFI 声明、86 个 Flutter 符号绑定；这说明接口面大于 UI 面，不说明 235 个左右的其余 API 全部不工作。未被 UI 调用的 API 需要按公开/内部/兼容/占位逐一归类。

最终判断：QGC 是整体跨平台与多固件成熟度最高的产品；MP 是 ArduPilot 专业操作和配套工具最深的产品；M-GCS 在安全契约、可替换前端和新架构边界上具有清楚的差异化，但目前尚不具备与两者等价的功能面和跨平台运行证据。M-GCS 的竞争路线应是先把高风险链路做成可证明的端到端闭环，再选少数高价值功能做到可靠和易用，而不是追逐 FFI 导出函数数量。

## 13. 可复核源码入口

下列路径指向本次比较的本地 checkout；不同提交更新后需重新核对。

### QGroundControl

- 架构与贡献门禁：[AGENTS.md](/Users/fansicheng/Projects/qgroundcontrol/AGENTS.md:1)、[CMakeLists.txt](/Users/fansicheng/Projects/qgroundcontrol/CMakeLists.txt:61)、[cmake/Toolchain.cmake](/Users/fansicheng/Projects/qgroundcontrol/cmake/Toolchain.cmake:17)、[cmake/CustomOptions.cmake](/Users/fansicheng/Projects/qgroundcontrol/cmake/CustomOptions.cmake:61)。
- 命令队列与重复命令/重试：[MavCommandQueue.cc](/Users/fansicheng/Projects/qgroundcontrol/src/Vehicle/MavCommandQueue.cc:174)、[MavCommandQueue.cc](/Users/fansicheng/Projects/qgroundcontrol/src/Vehicle/MavCommandQueue.cc:232)、[MavCommandQueue.cc](/Users/fansicheng/Projects/qgroundcontrol/src/Vehicle/MavCommandQueue.cc:289)、[MavCommandQueue.cc](/Users/fansicheng/Projects/qgroundcontrol/src/Vehicle/MavCommandQueue.cc:505)。
- 任务 ACK、事务与提交/失败处理：[PlanManager.cc](/Users/fansicheng/Projects/qgroundcontrol/src/MissionManager/PlanManager.cc:560)、[PlanManager.cc](/Users/fansicheng/Projects/qgroundcontrol/src/MissionManager/PlanManager.cc:610)、[PlanManager.cc](/Users/fansicheng/Projects/qgroundcontrol/src/MissionManager/PlanManager.cc:830)。
- 参数 ACK/回读与多链路健康：[ParameterManager.cc](/Users/fansicheng/Projects/qgroundcontrol/src/FactSystem/ParameterManager.cc:306)、[VehicleLinkManager.cc](/Users/fansicheng/Projects/qgroundcontrol/src/Vehicle/VehicleLinkManager.cc:100)。
- 固件/Vehicle/Fact 产品模型：[FirmwarePluginManager.cc](/Users/fansicheng/Projects/qgroundcontrol/src/FirmwarePlugin/FirmwarePluginManager.cc:75)、[Fact.h](/Users/fansicheng/Projects/qgroundcontrol/src/FactSystem/Fact.h:17)、[MultiVehicleManager.h](/Users/fansicheng/Projects/qgroundcontrol/src/Vehicle/MultiVehicleManager.h:14)。
- 项目范围和许可：[README.md](/Users/fansicheng/Projects/qgroundcontrol/README.md:25)、[COPYING.md](/Users/fansicheng/Projects/qgroundcontrol/.github/COPYING.md:3)、测试树 `/Users/fansicheng/Projects/qgroundcontrol/test/`。

### Mission Planner

- Windows/.NET Framework 产品边界：[MissionPlanner.csproj](/Users/fansicheng/Projects/MissionPlanner/MissionPlanner.csproj:1)、[README.md](/Users/fansicheng/Projects/MissionPlanner/README.md:1)、[main.yml](/Users/fansicheng/Projects/MissionPlanner/.github/workflows/main.yml:1)、[COPYING.txt](/Users/fansicheng/Projects/MissionPlanner/COPYING.txt:1)。
- 参数和命令 ACK/超时：[MAVLinkInterface.cs](/Users/fansicheng/Projects/MissionPlanner/ExtLibs/ArduPilot/Mavlink/MAVLinkInterface.cs:1683)、[MAVLinkInterface.cs](/Users/fansicheng/Projects/MissionPlanner/ExtLibs/ArduPilot/Mavlink/MAVLinkInterface.cs:2803)。
- 任务条目与本地缓存：`/Users/fansicheng/Projects/MissionPlanner/ExtLibs/ArduPilot/Mavlink/MAVLinkInterface.cs:4098-4131`（`setWP`）、`:4273-4307`（`setWPint`）；调用失败提示在 [FlightData.cs](/Users/fansicheng/Projects/MissionPlanner/GCSViews/FlightData.cs:1535)。
- 罗盘校准、地图/任务规划、视频、日志： [ConfigHWCompass.cs](/Users/fansicheng/Projects/MissionPlanner/GCSViews/ConfigurationView/ConfigHWCompass.cs:453)、[FlightPlanner.cs](/Users/fansicheng/Projects/MissionPlanner/GCSViews/FlightPlanner.cs:175)、[FlightData.cs](/Users/fansicheng/Projects/MissionPlanner/GCSViews/FlightData.cs:3152)、`/Users/fansicheng/Projects/MissionPlanner/Log/LogDownloadMavLink.cs` 与 `LogAnalyzer/`。

### M-GCS

- 规则与当前偏差：[safety.md](/Users/fansicheng/Projects/M-GCS/docs/constraints/safety.md:12)、[known-deviations.md](/Users/fansicheng/Projects/M-GCS/docs/constraints/known-deviations.md:12)、[parity.md](/Users/fansicheng/Projects/M-GCS/docs/constraints/parity.md:1)。
- 多 Vehicle command submit/poll：[ffi_flight_commands.cc](/Users/fansicheng/Projects/M-GCS/bridge/ffi/ffi_flight_commands.cc) 的 `mg_command_submit_for_vehicle`/`mg_command_poll`、[flight_command_ffi_backend.dart](/Users/fansicheng/Projects/M-GCS/flutter_shell/lib/connection/flight_command_ffi_backend.dart) 的 submit/poll 绑定与 ticket 轮询。
- Mission ACK 目标校验和失败处理：[mavlink_adapter_mission.cc](/Users/fansicheng/Projects/M-GCS/core/src/mavlink/mavlink_adapter_mission.cc:268)、[mission_transfer.h](/Users/fansicheng/Projects/M-GCS/core/src/mavlink/mission_transfer.h:66)。
- Firmware flashing 占位：[firmware_flasher.cc](/Users/fansicheng/Projects/M-GCS/core/src/firmware/firmware_flasher.cc:52)。
- FFI 目录、绑定和本机库版本核对：[API_CATALOG.md](/Users/fansicheng/Projects/M-GCS/bridge/ffi/API_CATALOG.md:1)、[check_ffi_contract.py](/Users/fansicheng/Projects/M-GCS/tools/check_ffi_contract.py:1)。

### 复核发现：Mission Planner rejected MISSION_ITEM 仍更新缓存

这是本次源码审计发现的一个具体问题，影响是 Mission Planner 进程内的 mission/fence/rally cache 可能包含飞控拒绝的 item；不表示飞控接受或执行了该 item。`setWP` 与 `setWPint` 收到匹配 ACK 后，在返回 ACK 类型之前把本地 item 写入 cache。当前找不到覆盖拒绝型 item ACK 的测试。由于本轮未运行 MP 构建/硬件测试，结论限定为代码顺序和缓存行为，不推断飞行器端状态。
