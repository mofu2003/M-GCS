# Changelog

## Unreleased

### Added

- 多机共享链路：UDP、TCP loopback 和 POSIX PTY 均完成多 `sysid/compid` 解复用验证。
- 显式 link handle、link → route → vehicle 映射、同机多 route 归并、per-route heartbeat/
  primary health 和 route 级精确关闭。
- 多机地图第一版：Fleet 位置快照、非 active 车辆标记和点击切换。
- MAVLink signing：每 `MavlinkAdapter` 独立 TX sequence、密钥、时间戳和 stream；
  入站签名校验、出站签名、命名密钥库、连接后自动应用和 SETUP_SIGNING 下发。

### Changed

- 连接会话拆分为物理 link 与 Vehicle 焦点，命令始终绑定明确 Vehicle。
- Flutter 连接面板和地面站配置页纳入多机链路与 signing 管理。
- 更新多机状态审核、实施计划、架构、功能差距和 FFI Catalog。

### Verification

- 后端 offline/simulation：37/37 通过。
- Flutter 常规测试：111 通过。
- `test_mavlink_protocol`：133/133 通过。
- 真实 Fleet FFI UDP 测试：3/3 通过。
- `flutter analyze`、UI token 检查、macOS debug 构建：通过。

## 0.0.79 - 2026-10-08

### Added

- 配置工作区完成模块化：原 4,099 行单文件拆成 35 个独立 Dart 文件，
  按系统状态、校准、端口、电机、飞行模式、PID、电池、解锁和全部参数分层。
- 保留 `lib/configuration_workspace.dart` 作为兼容导出，不改变既有调用方和页面行为。
- 新增配置路由、配置组件和参数编辑回归测试，覆盖九页路由、标签控制器生命周期、
  罗盘进度、数值回滚/去重以及 PID 参数命名空间。

### Changed

- 配置页间距统一迁入 `MgMetrics.config*`，不新增第二套尺寸系统。
- Flutter 结构整理为 `components/`、`configuration/` 和 `demo/`，组件画廊仅作为开发工具。
- 当前文档、架构图、状态审计和 API Catalog 统一到 `0.0.79`。

### Removed

- 删除已完成使命的配置工作区拆分施工指南。
- 删除废弃的 `CONSTRAINTS`、`UPSTREAM_CODE_SELECTION`、`QGC_MP_BACKPORT_MAP`
  重定向文档，以及重复的历史整理报告。

### Verification

- `ctest --test-dir build_check ...`：35/35 通过。
- `flutter analyze --no-pub lib test`：无问题。
- `flutter test --no-pub`：79/79 通过。
- `flutter build macos --debug --no-pub`：通过。
- 全仓 Markdown 链接检查：0 个失效链接。

## 0.0.78 - 2026-10-08

### Added

- 顶部状态栏统一的悬浮长按确认条：0.5 秒白底到蓝色进度填充，填满后出现对勾并
  下发命令；“移动到此处”、返航、开始任务、解锁/上锁共用该流程。解锁成功不弹窗，
  失败才显示后端错误。
- 右下角 MacGlass 复合姿态/指南针仪表：固定在圆形玻璃盘内，中央为姿态球，外圈
  为航向刻度，顶部固定航向箭头，底部胶囊显示中文方向和三位航向。仪表使用旧版
  MacGlass 配色、毛玻璃和专用玻璃不透明度。
- 飞控消息中文翻译：覆盖常见 ArduPilot 预解锁、EKF/GPS、电池、模式切换等状态
  文本；未知消息保留原文，不做猜测翻译。
- Home 同步语义：`mg_mission_doc_verify` 新增 `home_sync_count` / `home_sync`，
  飞控动态 Home 漂移不再污染任务差异；任务页提供“采用飞控 Home”和“保留本地 Home”。

### Changed

- 飞行模式切换和顶部危险命令统一为 0.5 秒长按确认，移除重复的确认对话框。
- 所有弹窗改为纯淡入，移除 macos_ui 默认的缩放回弹跳动。
- 位置、姿态、VFR HUD 遥测流和前端轮询提高到 20 Hz，地图和仪表刷新更平滑；
  波形采集保持原有 10 Hz。
- 飞控消息时间戳固定 72 px 宽度并右对齐，正文独立列对齐。
- 固定翼 Go to location 继续使用 `WP_LOITER_RAD` 半径和符号方向，盘旋圆箭头位于
  圆周切线方向。
- 右侧连接、飞控消息和任务规划面板共享右上槽位，同一时间只显示一个；底部遥测
  数据条在地图内容区水平居中。

### Fixed

- QGC/ArduPilot 任务坐标帧兼容：`MAV_FRAME_MISSION` 的 `x/y` 按 `param5/param6`
  语义处理，不再把 DO_/CONDITION_ 等非定位命令回读成 `(0,0)` 假航点；固定翼
  `NAV_TAKEOFF` 的位置仍保留。
- QGC `.plan` 和飞控回读共用同一套“命令是否承载位置”判断；非定位命令不再进入
  假航线，Home 自动更新与任务项差异分开显示。
- 修复任务文档在 Home 缺失时从飞控 HOME_POSITION 读取后仍重复要求手工设置 Home
  的前端体验。

### Documentation

- 全量同步 AGENTS、README、Flutter README、API Catalog、架构图、开发指南、状态审计
  和任务规范到 `0.0.78`。
- 新增 QGC/MP 功能差距分析和 ADR 0010，明确 M-GCS 是独立实现，不是 QGC/MP 源码
  分支或完整复刻。
- 修复全仓 Markdown 内部链接，历史整理报告标记为历史快照。

### Verification

- `ctest --test-dir build_check ...`：35/35 通过。
- `flutter analyze --no-pub lib test`：无问题。
- `flutter test --no-pub`：69/69 通过。
- `flutter build macos --debug --no-pub`：通过。
- `git diff --check`：通过。
- QGC `.plan`、飞控任务回读和 `MAV_FRAME_MISSION` 兼容回归测试通过。

## 0.0.77 - 2026-10-07

### Changed

- **文档结构重组**：从扁平结构改为分类结构（architecture/guides/planning/specs/research），
  消除约 3,000 行重复内容
- **指导文档统一**：AGENTS.md 成为唯一权威源，DEVELOPMENT.md 改为简化版，
  CONSTRAINTS.md 标记为已废弃
- **上游策略合并**：UPSTREAM_CODE_SELECTION.md 与 QGC_MP_BACKPORT_MAP.md 
  合并为 specs/upstream/SELECTION_STRATEGY.md
- **版本号统一**：8 个文档版本号更新到 0.0.77
- **交叉引用修复**：所有内部链接更新到新路径

### Removed

- 系统垃圾文件：3 个 .DS_Store

## 0.0.76 - 2026-10-07

### Added

- 航向罗盘（右下角）：表盘固定、单头箭头指向当前航向，中文方位字（北/东/南/西），
  三级刻度；数据与地图上飞机标记同源（`GLOBAL_POSITION_INT.hdg` / `VFR_HUD.heading`）。
- 姿态仪（罗盘左侧）：天地线随横滚反向旋转、随俯仰平移，机翼符号固定在中央，
  带 ±10/20/30 俯仰刻度。数据走 `ATTITUDE`（新增 Dart 侧轮询，后端本就提供
  `mg_vehicle_get_attitude`）。新增仪表专用色 `MgColors.attitudeSky/attitudeGround`。
- 地面站配置页（`设置 → 地面站配置`）：与飞控配置同构（左侧栏目 + 右侧内容），
  第一页为「地图配置」，把原先散在地图上的底图选择与数据来源署名收进来。
- 固定翼 go to location 的盘旋圆：半径取 `|WP_LOITER_RAD|`、方向取其符号
  （正=顺时针/负=逆时针），圆周上下各一个切线箭头指示绕行方向。
- 飞行模式列表显示中文名（`flight_mode_labels.dart`，覆盖 ArduPilot Plane/Copter/
  Rover/Sub/Tracker 与 PX4 的模式名；未收录的回退成英文标识）。

### Changed

- 飞行模式切换改为 QGC 式**按住确认**：按住条目进度填满（1 秒）才下发切换，
  中途松手取消；取消短按直接切换与确认弹窗。
- 当前飞行模式改用**底色高亮 + 强调色文字**表示，不再用对勾。
- 状态栏 VTOL 状态后缀改中文：`转固定翼` / `转多旋翼` / `多旋翼` / `固定翼`。
- 地图右下角的底图下拉与数据来源「?」按钮移除，改由地面站配置页承载。

### Fixed

- 引导目标（移动到此处）原先在地图上没有盘旋范围提示，固定翼按固定翼语义补上盘旋圆。

### Verification

- `flutter analyze lib test` 通过，Flutter `56/56` 测试通过，macOS debug 构建通过。
- 后端 offline/simulation 门禁 `35/35` 通过。
- 本地 QuadPlane SITL 实测：模式列表按住切换、盘旋圆与方向箭头、罗盘/姿态仪实时显示、
  地面站配置页切换底图来源即时生效。

## 0.0.75 - 2026-10-07

### Added

- 底部悬浮遥测仪表盘：与 QGC 默认 instrument panel 一致的八项读数（相对高度、
  距 Home、垂直速度、地速、空速、油门、飞行时间、飞行里程），指标名中文。
- 地图点击动作菜单（对照 QGC `FlyViewMap.qml` 的 `mapClickDropPanel`）：移动到此处、
  环绕飞行、在此外设置兴趣点、在此设置 Home、设置估计原点、设置航向；点击处显示
  准星图标，点击菜单外任何位置即可关闭。
- 左侧飞行动作条新增「高度」（独立于引导菜单），对应 QGC
  `guidedModeChangeAltitude`：`SET_POSITION_TARGET_LOCAL_NED` + `LOCAL_OFFSET_NED`。
- 统一悬浮面板容器 `MgFloatingPanel`：65% 半透明 + 背景模糊 + 圆角 + 描边 + 投影，
  顶部状态栏、地图工具列、飞行动作条、遥测条、设置菜单、连接/消息/航线规划浮层共用。
- 新增 FFI：`mg_command_change_heading`（`MAV_CMD_CONDITION_YAW`）、
  `mg_command_set_global_origin`（`SET_GPS_GLOBAL_ORIGIN`）、
  `mg_command_change_altitude`。
- VTOL 任务语义：`NAV_VTOL_TAKEOFF` 可作首项、`NAV_VTOL_LAND` 可作末项。

### Fixed

- **Dart FFI 把 C 的 `float` 参数错误绑定为 `Double`**：`mg_command_guided_point`
  的高度/航向/速度、`mg_command_set_home` 与 `mg_command_takeoff` 的高度在传参时
  被按位错读。表现是"移动到此处"时目标高度变成垃圾值——落在合法范围就飞向错误
  高度（掉高度），落在范围外则被飞控以 `MAV_RESULT_DENIED` 拒绝；起飞高度也可能
  静默失效。现所有 `float` 参数一律绑 `Float`。
- 移动到此处对齐 QGC `APMFirmwarePlugin::guidedModeGotoLocation`：不再提供高度输入，
  目标高度固定取飞机当前 AMSL；拿不到有效海拔时直接拒发，不再退化成相对高度或 0。
- ROI 高度改用相对 Home 坐标系（与 QGC 的 `MAV_FRAME_GLOBAL_RELATIVE_ALT` 一致）。
- 环绕飞行改用 QGC 的 COMMAND_INT 形式：`MAV_FRAME_GLOBAL`、`NaN` 速度/圈数、
  `ORBIT_YAW_BEHAVIOUR_UNCHANGED`，高度取 `home AMSL + 相对高度`。
- 起飞高度取 `max(飞控最小起飞高度, 用户值)`，并新增飞机位置有效性检查。
- 顶部状态栏窄窗口（640/720 px）溢出。
- 飞行模式菜单 `ListTile` 因 `Material` 祖先位置错误触发的断言。
- 地图点击菜单不选项目无法关闭。

### Changed

- 遥测数据流速率：`GLOBAL_POSITION_INT` 2 Hz → 10 Hz、`GPS_RAW_INT` 2 Hz → 5 Hz，
  飞机在地图上的移动明显更顺滑。
- 顶部状态栏移除 `M-GCS` 品牌与飞行模式前的飞机图标。
- 飞控消息面板移入右上角，与连接面板、航线规划互斥（同一角落只显示一个）。
- 左侧飞行动作条的起飞/降落/返航改为竖排。

### Removed

- 移除旧的 `mapControlSurface` 色值，并入统一的 `floatingSurface`。

### Verification

- `flutter analyze lib test` 通过，Flutter `51/51` 测试通过，macOS debug 构建通过。
- 后端 offline/simulation 门禁 `35/35` 通过（含新增的 change_altitude 断言）。
- 用本地 QuadPlane SITL 实测：引导地图菜单、遥测仪表盘、模式切换、返航/降落。

## 0.0.74 - 2026-10-06

### Added

- 新增 QGC 风格主页面顶部状态栏：集中显示连接/待命/解锁/飞行状态、飞行模式、
  VTOL 模式、GPS 星数与定位类型、电池百分比/电压和链路类型。
- macOS 标题栏改为真正的原生 `NSWindow` 标题栏，标题文字显示为 `M-GCS`。

### Changed

- macOS 窗口恢复原生不透明标题栏，内容区从标题栏下方开始；移除配置页为透明
  full-size 标题栏预留的额外高度。

### Removed

- 删除地图上的旧矩形飞行仪表盘，避免与顶部状态栏重复显示同类状态。

### Verification

- `flutter analyze lib test` 通过，Flutter `26/26` 测试通过，macOS debug 构建通过。
- 后端 offline/simulation 门禁通过。

## 0.0.73 - 2026-10-05

### Added

- 地图飞行仪表盘第一版：紧凑状态条显示链路、模式 + VTOL 状态、解锁、GPS、
  地速、相对高度和电量；展开后显示 3×3 飞行/导航/任务数据网格。
- 引导目标显示：成功发送“移动到此处”后，地图保留目标图标；仪表盘同时显示目标
  坐标和距目标距离，断开连接时清除。
- VTOL 转换文案明确区分进行中状态：`正在转换为固定翼`、
  `正在转换为多旋翼`、`MC`、`FW`。

### Verification

- `flutter analyze lib test` 通过，Flutter `24/24` 测试通过，UI token 检查通过，
  macOS debug 构建通过。用本地 QuadPlane SITL 实测仪表盘紧凑/展开形态。

## 0.0.72 - 2026-10-03

### Changed

- 文档重新分层：当前规范保留在 `docs/` 根目录；QGC/MP 上游对照移入
  `docs/upstream/`；字体和多机连接调研移入 `docs/research/`；重构计划和质量审计移入
  `docs/audits/`，并在 `docs/README.md` 重新整理推荐阅读顺序和唯一权威出处。
- 清理工作区运行产物：移除 `.DS_Store`、SITL `eeprom.bin`、`logs/`、`terrain/` 和
  临时 `.workbuddy-ai/`；这些内容不再出现在 git 状态中。
- 更新所有当前文档的版本基线到 `0.0.72`，历史 CHANGELOG 和冻结审计保持原始版本号。

### Verification

- Markdown 本地链接检查：缺失 0。
- `tools/preflight.sh` 全部通过；后端 offline/simulation `35/35`，Flutter analyze、
  `24/24` 测试和 macOS debug 构建通过。

## 0.0.71 - 2026-10-03

### Fixed

- 修复 VTOL 地面启动任务流程：QGC 的 `Vehicle::fixedWing()` 不包含 VTOL，M-GCS 现在同样
  只把真正固定翼放入 AUTO+解锁流程；VTOL 改用 `GUIDED -> 解锁 -> MISSION_START`，
  不再停在已解锁但没有启动任务的状态。
- `MAV_CMD_DO_REPOSITION` 改为 COMMAND_INT + `MAV_FRAME_GLOBAL`，与 QGC APM 引导逻辑
  一致；此前 COMMAND_LONG 缺少 frame，ArduPilot 会返回 `MAV_RESULT_UNSUPPORTED`。
- 引导操作按具体状态给出不可用原因；起飞按 `GUIDED -> 解锁 -> NAV_TAKEOFF` 执行；
  暂停改为 ArduPilot 的真实暂停模式（Copter Brake、Plane/VTOL Loiter、Rover Hold）。
- 任务文档、回读核对结果和 dirty 状态绑定当前连接 session；切换飞控后必须先重新读取
  或创建任务，避免把飞控 A 的任务上传给飞控 B。
- 任务传输增加单事务门控、mission type 精确路由、来源 sys/comp 和序号校验；重传/下载
  会清空旧执行进度，命令 ACK 超时会立即唤醒等待者。
- 参数写入确认同时校验 sysid/compid、类型、请求代次和值；参数缓存、下载完成和 refresh
  按当前目标组件隔离，旧回包不再污染新会话。
- TCP/UDP 读线程改为共享状态持有，Windows 超时后不再 CloseHandle 后继续析构导致
  use-after-free；连接 reset 的阻塞关闭移到锁外。
- 心跳丢失会回收幽灵 Vehicle/session；关闭 active vehicle 后自动选择剩余车辆。
- 本地创建的 Survey/Corridor 可导出为 QGC Survey/CorridorScan ComplexItem；无法完整表达
  时退回 SimpleItem 并返回 warning，不再直接失败。
- VTOL 飞行模式台接入 `EXTENDED_SYS_STATE.vtol_state`，显示 `MC`、`FW` 和转换状态；
  单次遥测读取失败保留最后有效状态，不再短暂显示已上锁/未知机型。

### Verification

- 后端 offline/simulation `35/35` 通过，包含新增的任务并发/乱序/来源校验、参数旧回包、
  多机重连、TCP/UDP 关闭、QGC ComplexItem 往返测试。
- Flutter analyze、`24/24` 测试和 macOS debug 构建通过；`tools/preflight.sh` 全部通过。
- ArduPlane QuadPlane SITL 实测：AUTO 地面状态执行引导起飞会切到 GUIDED、解锁并起飞；
  飞行中“移动到此处”通过 COMMAND_INT 被飞控接受。

## 0.0.70 - 2026-10-03

### Added

- 命令目录补上 `MAV_CMD_NAV_VTOL_TAKEOFF`（84）与 `MAV_CMD_NAV_VTOL_LAND`（85），
  此前目录里只有 22/21，VTOL 机型根本找不到垂直起降命令。字段按官方
  `common.xml` 定义：84 的 `param2` 是过渡航向（5 个枚举值）、`param4` 偏航；
  85 的 `param1` 是降落方式（3 个枚举值）、`param3` 进场高度、`param4` 偏航。
- 新增实体类型 `vtol_takeoff` / `vtol_land`，并接入校验（类型↔命令一致性、
  位置/高度必填）与编译；前端命令下拉由目录驱动，无需改动即可看到这两条。

### Verification

- `test_mission_document` 新增用例：目录含 84/85 且带对应枚举选项；
  「垂直起飞 → 航点 → 垂直降落」通过校验、不被判 unsupported、编译出 84/85。
- 用本地 quadplane SITL（`MAV_TYPE=20`）实测：`mg_mission_get_command_catalog`
  含两条命令；构造的 VTOL 任务上传成功，回读计划里同时出现 `command:84` 与 `command:85`。
- 后端 offline/simulation `35/35`；Flutter analyze 无问题、测试 `23/23`；
  UI token 与 macOS debug 构建通过；完整 `tools/preflight.sh` 通过。

## 0.0.69 - 2026-10-03

### Fixed

- 修复「解锁按钮点了没反应」。确认弹窗的 `primaryButton` 之前 `pop(singleAction)`，
  而 `singleAction` 默认 `false`——于是点弹窗里的「解锁」被当成"没确认"，`_toggleArmed`
  直接 return，解锁命令从未发出。现在确认键固定返回 `true`，`singleAction` 只用来决定
  是否显示取消按钮。
- 该 bug 同时解释了"消息栏没有输出"：命令没发出去，飞控自然不会回 `Throttle armed`
  之类的 STATUSTEXT，消息框没内容可显示。修好后解锁成功，消息也随出现了。

### Verification

- 用界面自动化走完整流程验证：点击「解锁」→ 弹窗出现 → 点弹窗「解锁」→ 飞控解锁，
  面板出现「已解锁」徽标、按钮变为「上锁」。
- 同一流程下打开消息框，标题旁显示 `6` 条消息并列出时间戳（含解锁产生的
  `Throttle armed`），确认消息链路正常。
- Flutter analyze 无问题、测试 `23/23`；UI token 与 macOS debug 构建通过；
  完整 `tools/preflight.sh` 通过。

## 0.0.68 - 2026-10-02

### Added

- 新增**飞控消息框**：地图工具栏 `💬 飞控消息` 打开一个浮动面板，按时间列出飞控
  STATUSTEXT，按严重级别着色（error 及以上危险色、warning 警告色、debug 灰），
  自动滚到最新，可一键清空。等价于 MP 的 Messages 页 / QGC 的 Messages 面板。
- 后端 `FlightMessageLog` 保留最近 200 条消息（seq 单调递增、带接收时间与严重级别），
  新增 FFI `mg_telemetry_get_messages(core, since_seq)` 供前端**增量**拉取；断开连接时
  前端清空本地列表并重置 seq。

### Verification

- 用本地 SITL 实测：解锁/上锁后拿到
  `{"seq":1,"severity_name":"info","text":"Throttle armed"}` 与 `"Throttle disarmed"`。
- 消息历史放在独立 `flight_message_log.h` 并由 `Vehicle` 持有，**没有动
  `telemetry_cache.h`**（该头有 <17000 字节的体积守卫，接进去会直接撑破）；
  `test_bridge_architecture_guards` 50/50 通过。
- 后端 offline/simulation `35/35`；Flutter analyze 无问题、测试 `23/23`；
  UI token 检查与 macOS debug 构建通过；完整 `tools/preflight.sh` 通过。

## 0.0.67 - 2026-10-02

### Added

- 飞行模式操作台新增**解锁 / 上锁**按钮（对标 MP 把 ARM 按钮放在飞行模式旁边）。此前
  前端只能通过「启动任务」隐式解锁，没有独立的解锁入口。
- 两个动作都先确认再执行：解锁提示"电机可以转动"，上锁提示"飞行中上锁会掉飞机、地面
  测试先拆桨"。按钮按当前 `armed` 状态切换，执行中显示进度圈，失败弹窗给出原因。
- 前端补齐 `mg_command_disarm` 绑定（后端早已实现，Dart 侧一直没接），控制器新增
  `armVehicle()` / `disarmVehicle()`：发命令后都在 1.5 s 窗口内等心跳确认状态翻转，
  没翻转就报错，而不是发完就当成功。
- 修复解锁按钮一点就崩溃的问题：确认弹窗里的按钮传了 `ControlSize.regular`，而
  `MacosAlertDialog` 断言要求 `ControlSize.large`，导致点击后直接出现 Flutter 错误红屏。
  已按 `MacosAlertDialog` 的约束改正，对话框外观同时对齐航线规划面板已有的确认弹窗。

### Verification

- Flutter analyze 无问题、测试 `23/23` 通过；UI token 检查与 macOS debug 构建通过；
  完整 `tools/preflight.sh` 通过。
- 解锁路径复用「启动任务」已在用的同一个 `_armVehicleAndValidate`；上锁是它的镜像实现。

## 0.0.66 - 2026-10-02

### Changed

- 本机标记剪影按需求收敛为两种：**多旋翼**与**固定翼飞机**。删掉 VTOL（飞机 + 四个桨盘）、
  地面车辆和"未知"三种画法：VTOL 和其它机型统一按固定翼画——它本身就是带升力桨的飞机。
  机型判定也相应简化：MAV_TYPE 2/3/4/13/14/15/16（多旋翼族含共轴、三/六/八轴）走多旋翼，
  其余一律固定翼；MAV_TYPE 缺失时回退到控制档位。

### Verification

- 两个剪影的绘制代码未变（上一版已逐帧核对过形状），本次只删分支与调整机型映射。
- Flutter analyze 无问题、测试 `23/23` 通过；UI token 检查与 macOS debug 构建通过；
  完整 `tools/preflight.sh` 通过。

## 0.0.65 - 2026-10-02

### Added

- 地图上新增「飞机 → Home」虚线，并在连线中点显示距离（`距 Home 1.01 km`）。距离用
  WGS-84 计算，与任务航段一致；样式刻意和任务航线区分：更细、点划更密、成功色。
  飞机离 Home 不足 5 m 时不画——此时连线长度为零，标签只会压住飞机和 Home 图标。

### Verification

- 复用地图既有的虚线绘制（`StrokePattern.dashed`）与距离标签（`_DistanceLabel`）路径，
  与用户已确认可用的任务航段距离标注同源。
- Flutter analyze 无问题、测试 `23/23` 通过；UI token 检查与 macOS debug 构建通过；
  完整 `tools/preflight.sh` 通过。

## 0.0.64 - 2026-10-02

### Added

- 任务中止判定：`mg_mission_get_execution_progress` 新增 `aborted` —— 解锁飞行中离开过
  任务模式、且最后一项未到达即判中止。带上"最后一项未到达"是为了区分正常收尾：飞控跑完
  任务后自己也会切 RTL/降落，那时 `reached_end` 已是 true。任务面板据此显示
  「任务已中止」（危险色 + 警示图标），不再把中断的任务显示成"进行中"。
- 补上「清除飞控任务」入口：此前后端有 `mg_mission_clear`，但前端完全没有接。现在任务
  面板动作行有该按钮，按 MP/QGC 的习惯先确认再执行，只清飞控上的任务、不动本地文档。

### Verification

- `test_sim_mission` 新增中止用例：任务模式下解锁后切到 LOITER 且未到终点 => `aborted:true`；
  同时校验 `reached_end` / `finished` / `aborted` 三者互不干扰。
- Flutter 新增控制器用例：清除飞控任务后本地文档仍在。测试 `23/23` 通过。
- 后端 offline/simulation `35/35`；analyze 无问题；UI token 与 macOS debug 构建通过；
  完整 `tools/preflight.sh` 通过。

## 0.0.63 - 2026-10-02

### Changed

- 任务完成判定改为 QGC 的语义（`FlyViewMissionCompleteDialog.qml` 的等价实现）：
  **曾在任务模式（ArduPilot Auto / PX4 Auto Mission）下解锁飞行，之后上锁**才算
  `finished`。此前"最后一项到达即完成"会提前报完成——固定翼到达最后一项后还要返航
  降落；那套判断现在叫 `reached_end`（航段飞完）。
- `mg_mission_get_execution_progress` 同时返回 `reached_end` 与 `finished`，前者用于
  "等待返航/降落"的中间态。新一轮解锁会清掉上一轮的完成标记。
- 任务面板进度条分三档：`任务进度 第 N/M 项 · 已到达 X 项` → `航段已完成 · 共 N 项，
  等待返航/降落`（警示色）→ `任务完成 · 共 N 项`（成功色）。

### Verification

- `test_sim_mission` 新增 QGC 规则用例：未解锁不完成、GUIDED 解锁飞行后上锁不算完成、
  AUTO 解锁后上锁判完成、`reached_end` 与 `finished` 相互独立、再次解锁清标记。
- 后端 offline/simulation `35/35` 通过；Flutter analyze 无问题、测试 `22/22` 通过；
  UI token 检查与 macOS debug 构建通过；完整 `tools/preflight.sh` 通过。

## 0.0.62 - 2026-10-02

### Changed

- 地图上的本机标记不再统一用「圆形徽标 + 纸飞机图标」，改为按机型绘制机体剪影：
  多旋翼画交叉机臂 + 四个桨盘 + 机身，固定翼画机身 + 后掠翼 + 尾翼，VTOL 在固定翼
  剪影上叠四个升力桨盘，地面车辆画车身 + 四轮；剪影默认朝正北，按航向旋转。
- 机型判定用官方 MAV_TYPE（1 固定翼，2/3/4/13/14/15/16 多旋翼族，10/11 地面车辆，
  19–25 VTOL），缺失时回退到控制档位。绘制用 `CustomPaint`，不引入新的图片资源或依赖。
- 顺手删掉 `_MapMarker` 里已无用的航向旋转参数（只有 Home 还在用它）。
- 修掉飞行模式面板在 `initState` 里同步刷新导致 `setState() called during build`
  的异常：首次拉取模式表改到首帧之后执行。

### Verification

- 用一次性渲染测试把五种剪影导出成 PNG 逐帧核对形状（多旋翼机臂一度被桨盘盖住，
  已调整桨盘半径与机臂线宽），核对完删除该临时测试。
- Flutter analyze 无问题、测试 `22/22` 通过；UI token 检查与 macOS debug 构建通过；
  完整 `tools/preflight.sh` 通过。

## 0.0.61 - 2026-10-02

### Fixed

- 飞行模式操作台改用与航线规划面板一致的外框（`surfaceMuted` + `panelRadius` +
  分隔线 + 投影 + 标题栏 + 关闭按钮），不再是一个裸列表面板。
- 修正摆放位置：原来固定 `top:16, left:70`，正好盖在飞行遥测悬浮窗上。现在放在左侧
  空带（遥测条下方、连接面板上方），并按连接面板是否展开动态让位，三个浮层可同时打开
  且互不遮挡。
- 修正面板内部布局：模式列表所在的 `Expanded` 之前拿到无界高度约束，热重载时抛
  `RenderFlex ... unbounded height`。改为由 `Positioned` 的 top/bottom 给出紧约束。

### Verification

- 热重载后无渲染异常；Flutter analyze 无问题、测试 `22/22` 通过。
- UI token 检查与 macOS debug 构建通过；完整 `tools/preflight.sh` 通过。

## 0.0.60 - 2026-10-02

### Added

- 新增 FFI `mg_flight_get_mode_mapping`：对标 MP 的 `ConfigFlightModes` 页面，读取
  `FLTMODE1..6`（PX4 为 `COM_FLTMODE1..6`）、模式通道 `FLTMODE_CH` / `MODE_CH`，
  以及 Copter/Rover 的 `SIMPLE` / `SUPER_SIMPLE` 位掩码；返回值同时给出该机型的模式
  选项表。写回复用既有 `mg_config_write_params`，不新增写接口。
- `mg_config_read_param` 对模式档位参数按**当前机型**注入模式名选项：`FLTMODE1..6`
  现在返回 `{"value":10,"label":"Auto"}` 这类选项，配置页的 6 个档位因此自动渲染成
  模式名下拉，而不是让用户填数字——这正是 MP 从参数元数据取模式名的做法。

### Verification

- `test_sim_commands` 新增断言：模式档位参数带 `options_from_flight_modes` 并给出
  可读模式名（`Loiter`）。
- 用本地 ArduPlane SITL 实测：`mg_flight_get_mode_mapping` 返回 `FLTMODE_CH=8` 与
  六档实际值（1=Auto、2=RTL、3=Loiter、4=FBWA、5=Stabilize、6=Manual），
  `mg_config_read_param("FLTMODE1")` 返回完整 Plane 模式选项。
- 后端 offline/simulation `35/35` 通过；Flutter analyze 无问题、测试 `22/22` 通过；
  UI token 检查与 macOS debug 构建通过；完整 `tools/preflight.sh` 通过。

## 0.0.59 - 2026-10-02

### Added

- 新增 FFI `mg_flight_get_modes` / `mg_flight_set_mode`：按机型返回完整飞行模式表
  （`id` = custom_mode、`name`、`settable`）与当前模式、解锁状态，并支持按名字切换。
  模式号沿用 MP/QGC 的 ArduPilot 枚举（Copter Auto=3、Plane Auto=10），PX4 走
  main/sub 打包值。
- 新增「飞行模式」操作台：贴左上角悬浮，显示当前模式与解锁状态，列出模式表并高亮当前项，
  不可切换项置灰；未连接时只读。参考 QGC `FlightModeMenu.qml` 的列表交互与 MP
  `FlightData.cs` 的 `CMB_modes` 按机型取表。
- 三个浮层不再互斥：连接、航线规划、飞行模式可以同时打开，各自贴不同边缘，互不遮挡。

### Verification

- `test_sim_commands` 新增模式目录用例：连接后目录可用、含 Copter 的 Stabilize/Loiter、
  按名字切换到 Loiter 得到 custom_mode 5、未知模式被拒。
- 用本地 ArduPlane SITL 实测：`mg_flight_get_modes` 返回 Plane 表与 `current_mode:"Auto"`，
  `mg_flight_set_mode("Loiter")` 成功且回读变为 `Loiter`（custom_mode 12）。
- 后端 offline/simulation `35/35` 通过；Flutter analyze 无问题、测试 `22/22` 通过；
  UI token 检查与 macOS debug 构建通过；完整 `tools/preflight.sh` 通过。

## 0.0.58 - 2026-10-02

### Fixed

- 任务回读核对不再把 ArduPilot 的位置量化误差当成差异：`x`/`y` 以前用 degE7 整数
  精确比较，完全没用 `tolerance`；而飞控内部把经纬度存成 float 度，往返会带来
  约 0.1–0.5 m 误差，导致几乎每次上传都报“回读发现差异”。现在按当地尺度把米制
  容差换算成 degE7 阈值再比较（纬度 111320 m/度，经度按 cos(lat) 折算）。
- 非位置命令（DO_/CONDITION_ 等）的坐标帧差异归入 `normalizations`：飞控对这类
  命令统一按 frame 0 存，我们文档会带默认帧 3，属等价表示，不再报成差异。
- Flutter 侧默认核对容差从 0.1 m 调整为 1.0 m，与 ArduPilot 的存储精度匹配。

### Verification

- 用本地 ArduPlane SITL（UDP）实测：同一份 4 项任务，0.1 m 容差报 4 处差异，
  1.0 m 容差 `matches:true`、0 处差异。
- 新增 `test_sim_mission_document` 用例，模拟飞控侧 float 量化：1 m 容差判定一致、
  0.05 m 容差仍能报出量化差异。
- 后端 offline/simulation `35/35` 通过；Flutter 测试 `22/22` 通过；
  UI token 检查和 macOS debug 构建通过；完整 `tools/preflight.sh` 通过。

## 0.0.57 - 2026-10-01

### Fixed

- 地图首次拿到有效定位时把视角拉回飞机：地图中心是跨会话保存的，上次可能停在默认的
  上海，而飞机在几千公里外，连上了却看不到飞机。现在首次定位与当前中心距离超过 20 km
  时自动移动到飞机（低于 20 km 不动用户视角，每个会话只做一次）。

### Verification

- 用本地 ArduPlane SITL（UDP 14550）实测：GCS 上报 `lat -35.363261 / lon 149.16523`、
  Home 同为 `-35.363261 / 149.16523`，与 SITL 设定完全一致，航向 353°、空速 0.6–0.9 m/s，
  坐标链路无偏移。
- Flutter analyze 无问题，Flutter 测试 `22/22` 通过。
- UI token 检查和 macOS debug 构建通过；完整 `tools/preflight.sh` 通过。

## 0.0.56 - 2026-10-01

### Changed

- 加大航线规划面板的按钮：任务动作行（从飞控读取 / 上传到飞控 / 启动任务 / 取消）、
  围栏与 Rally 操作组、Home 卡片、核对面板的按钮从 `ControlSize.small` 提升到
  `ControlSize.regular`，与连接窗口保持一致。
- 任务动作行由 `Row` 改为 `Wrap`，按钮变大后在窄面板里自动换行，避免溢出。

### Verification

- Flutter analyze 无问题，Flutter 测试 `22/22` 通过。
- UI token 检查和 macOS debug 构建通过；完整 `tools/preflight.sh` 通过。

## 0.0.55 - 2026-10-01

### Added

- 新增 FFI `mg_mission_get_execution_progress`：报告任务执行进度
  （`valid` / `current_index` / `current_seq` / `total` / `reached_count` /
  `last_reached_seq` / `finished`）。`current_index` 已去掉 ArduPilot home 偏移。
- 适配器开始统计 `MISSION_ITEM_REACHED`：按 seq 去重累计已到达项，最后一项到达即
  `finished`；任务下载/上传会重置统计。此前 `MISSION_ITEM_REACHED` 只透传回调，
  前端无法判断任务是否跑完。
- Flutter 侧任务面板新增执行进度条（当前第 N/共 M 项、已到达数、完成状态），
  由 `MissionDocumentController.refreshExecutionProgress` 每秒轮询；
  面板只在显示时挂载，隐藏即停止轮询。

### Verification

- 后端 offline/simulation `35/35` 通过（`test_sim_mission` 新增 5 条进度断言）。
- Flutter analyze 无问题，Flutter 测试 `22/22` 通过。
- UI token 检查通过；完整 `tools/preflight.sh` 通过。

## 0.0.54 - 2026-10-01

### Added

- `mg_operations_get_snapshot` 新增 `fence` 和 `rally` 区段（`type` 分别为
  `fence_transfer` / `rally_transfer`），字段与 `mission` 同构
  （`state`/`progress_pct`/`received`/`sent`/`total`/`result`/`error`）。此前围栏和
  Rally 传输没有进度来源，前端无法等待完成。
- Flutter 侧完成对接：`MissionDocumentBackend` 新增围栏/Rally 的
  compile/upload/download/import/verify/clear/进度查询接口，
  `MissionDocumentFfiBackend` 绑定对应 FFI，`MissionDocumentController` 新增
  `uploadFence`/`readFenceFromVehicle`/`verifyFence`/`clearFence` 及 Rally 同名流程，
  任务面板“设置”标签页新增围栏/Rally 操作组（读取 / 下发 / 清除，下发自带回读核对）。
- `MissionVerificationDifference` 新增 `area` 字段，承载围栏/Rally 核对返回的区段名。

### Verification

- 后端 offline/simulation `35/35` 通过。
- Flutter analyze 无问题，Flutter 测试 `21/21` 通过（新增围栏/Rally 流程用例）。
- UI token 检查和 macOS debug 构建通过；bundle 内 `libmgcore.dylib` 含全部 300 个
  `mg_*` 导出符号（含新增围栏/Rally 接口）。
- 完整 `tools/preflight.sh` 通过。

## 0.0.53 - 2026-10-01

### Changed

- QGC ComplexItem 现在可以编译：复用 QGC 保存在 `TransectStyleComplexItem.Items`
  里的已算好任务项，而不是在 M-GCS 重新推导 Survey/Corridor 几何（参照 QGC
  `TransectStyleComplexItem::_save`）。新增 `expand_complex_item_entities` 和
  `expand_document_entities`，编译与估算都会按 `entity_insert_index` 把复杂项展开到
  正确位置。
- 校验层不再对复杂项一刀切报错：`TransectStyleComplexItem.Items` 缺失的复杂项继续以
  `complex_item.unsupported` 阻断上传；可展开的复杂项改为 `complex_item.expanded`
  警告（几何来自 QGC、在 M-GCS 中只读），展开出的未知 MAV_CMD 报
  `complex_item.unsupported_command`。含可展开复杂项的任务现在可以正常校验、编译和上传。

### Verification

- 后端 offline/simulation `35/35` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.52 - 2026-10-01

### Added

- 新增 FFI `mg_mission_doc_verify_fence` / `mg_mission_doc_verify_rally`，与
  `mg_mission_doc_verify` 对称：在一次已完成的对应下载后比较文档与飞控的围栏/Rally，
  返回 `matches`、`difference_count` 和结构化 `differences`
  （`area` / `index` / `field` / `expected` / `actual`），支持容差。
- 围栏比较覆盖多边形数量、顶点数、inclusion、每个顶点经纬度、圆形数量/圆心/半径以及
  breach-return 点与高度；Rally 覆盖点数、经纬度和高度。
- `test_sim_fence_rally` 扩展为上传 → 下载 → 回读 → verify 的完整闭环，并验证清空飞控
  围栏后 verify 会报告差异（29 项断言）。

### Verification

- 后端 offline/simulation `35/35` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.51 - 2026-10-01

### Fixed

- 修复围栏/Rally 下载时 mission type 被写死为 0 的适配器 bug：`send_mission_request_int`
  和 `send_mission_ack` 现在携带实际 mission type，围栏/Rally 下载不再错走普通任务状态机。
  该问题由新增的端到端模拟测试发现。

### Added

- 新增 `geo_fence_from_plan` / `rally_points_from_plan`：`compile_geo_fence_plan` 和
  `compile_rally_plan` 的逆过程。围栏分组规则与 QGC `GeoFenceManager` 一致
  （相邻且 `param1` 顶点数和命令相同的顶点属于同一多边形），非法计划会被拒绝。
- 新增 FFI `mg_mission_doc_import_fence_from_vehicle` / `mg_mission_doc_import_rally_from_vehicle`：
  在一次已完成下载后把飞控围栏/Rally 读回文档，语义与 `mg_mission_doc_import_from_vehicle`
  一致（未完成下载返回 `invalid_state`）。适配器新增 `fence_get_plan_snapshot` /
  `rally_get_plan_snapshot` 提供 provenance。
- 端到端模拟测试扩展为围栏/Rally 上传 + 下载 + 回读文档的完整闭环（25 项断言）。

### Verification

- 后端 offline/simulation `35/35` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.50 - 2026-10-01

### Added

- 新增模拟测试 `test_sim_fence_rally`：mock 飞控现在按 mission type 分库存放任务项
  （`MAV_MISSION_TYPE_FENCE` / `MAV_MISSION_TYPE_RALLY`），并在 mission count、
  request、ack、clear 中回显请求的 mission type，使围栏/Rally 上传可以端到端验证。
- 测试覆盖：文档围栏经 `mg_mission_doc_upload_fence` 下发为 5 个 fence 任务项
  （多边形顶点带 `param1` 顶点数、圆形带半径、breach-return 带高度），
  `mg_mission_doc_upload_rally` 下发 2 个 `NAV_RALLY_POINT`，
  `mg_mission_doc_clear_fence` 清空飞控围栏。

### Verification

- 后端 offline/simulation `35/35` 通过（新增 1 个 simulation 测试）。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.49 - 2026-10-01

### Added

- 新增 `compile_rally_plan`：把文档 `rally_points` 编译成 MAVLink rally mission type
  任务项，编码与 QGC `RallyPointManager` 对齐（`MAV_CMD_NAV_RALLY_POINT` 5100、
  `MAV_FRAME_GLOBAL_RELATIVE_ALT`、`param5/6/7` 为纬度/经度/高度、`autocontinue=0`）。
- 新增 FFI `mg_mission_doc_compile_rally`、`mg_mission_doc_upload_rally` 和
  `mg_mission_doc_clear_rally`，把文档 Rally 点接到既有 rally mission 传输上（Rally 是
  独立 MAVLink mission type，不随任务上传）。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.48 - 2026-10-01

### Added

- 新增 `compile_geo_fence_plan`：把文档 `geo_fence` 编译成 MAVLink fence mission
  type 的任务项，编码与 QGC `GeoFenceManager` 对齐（多边形用 5001/5002 且 `param1`
  为顶点总数，圆形用 5003/5004 且 `param1` 为半径，breach-return 用 5000 +
  `MAV_FRAME_GLOBAL_RELATIVE_ALT`），顺序为多边形 → 圆形 → breach-return。
- 新增 FFI `mg_mission_doc_compile_fence`、`mg_mission_doc_upload_fence` 和
  `mg_mission_doc_clear_fence`，把文档围栏接到既有 fence mission 传输上（围栏是独立
  MAVLink mission type，不随任务上传）。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.47 - 2026-10-01

### Added

- 任务校验层补齐 GeoFence 与 Rally 几何校验：Rally 点经纬度和高度、围栏多边形顶点
  数量和经纬度范围、圆形围栏圆心和半径、breach-return 点及高度都会检查，分别返回
  `rally.*` / `geo_fence.*` 错误码。
- 围栏与 Rally 属于独立 MAVLink mission type，仍作为合法文档保留（不阻断任务上传），
  与阻断上传的 `complex_item.unsupported` 区分开。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.46 - 2026-10-01

### Fixed

- 修复含 QGC ComplexItem 的任务文档在上传时静默丢弃复杂项的安全隐患：校验层新增
  `complex_item.unsupported` 错误，文档带复杂项时 `validate`/`compile`/上传/回读
  都会失败，并提示先在 QGroundControl 处理或移除复杂项。此前编译只展开普通实体，
  上传后飞控任务会与导入的 `.plan` 不一致且没有任何提示。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.45 - 2026-10-01

### Changed

- `MissionComplexItem` 新增 `entity_insert_index`，记录复杂项在任务序列中相对普通航点的
  插入位置；任务文档 JSON 一并保存和恢复（缺失时按“末尾”兼容旧文档）。
- QGC `.plan` 导出不再把复杂项固定追加到末尾，而是按插入位置与 SimpleItem 交错输出，
  保持 Survey/Corridor 与航点的原始先后顺序。
- `doJumpId` 作为派生字段在导出时对简单项和复杂项统一重编号，避免混排后出现重复序号。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.44 - 2026-10-01

### Changed

- QGC ComplexItem（Survey、CorridorScan 等）从 opaque `extensions` 提升为正式
  `MissionComplexItem` 文档模型，保存 `id`、`item_type` 和原始 JSON 载荷。
- QGC `.plan` 导入会把 `type != SimpleItem` 的条目按 `complexItemType`（回退
  `type`）识别为 complex item；导出时重新生成完整的 QGC ComplexItem JSON，
  确保 Survey/Corridor 等复杂任务往返无损。
- 复杂任务的导入告警改为“只读保留、暂不可语义化编辑”，明确当前能力边界。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.43 - 2026-10-01

### Changed

- GeoFence 从 opaque 区段提升为正式文档模型，支持 polygon、circle、inclusion 和
  breach-return。
- QGC `.plan` GeoFence version 2 会映射为正式模型，导出时重新生成
  `polygons`、`circles` 和 `breachReturn`；version 1 继续忽略并告警。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.42 - 2026-10-01

### Changed

- Rally 不再只保存在 opaque `extensions` 中：任务文档新增正式 `rally_points` 模型，
  支持经纬度和高度，并随任务文档 JSON 一起保存和恢复。
- QGC `.plan` 的 `rallyPoints` version 2 会映射到正式 Rally 模型，导出时重新生成
  QGC 点数组；旧 version 1 继续忽略并给出 warning。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.41 - 2026-10-01

### Changed

- `MissionDocument` 新增 `extensions` 区，用于保存任务文档本身不理解的格式专属 JSON。
- QGC `.plan` 导入不再拒绝 ComplexItem、非空 GeoFence 或 Rally；这些区段会原样保留，
  并在再次导出 `.plan` 时放回，同时通过 warnings 明确说明它们仍是 opaque 数据。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.40 - 2026-10-01

### Changed

- 参数 profile 识别补齐 Rover、Submarine 和 AntennaTracker，分别映射到
  `rover`、`sub`、`tracker`，不再返回空 profile。
- 这些 profile 当前回退到通用描述库；后续可独立补充机型专属参数元数据，覆盖报告
  和分组接口会使用正确的 profile 名称。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.39 - 2026-10-01

### Changed

- 根据覆盖率报告补齐高频 ArduPilot 参数基础元数据：旧版 `WPNAV_SPEED`、
  `WPNAV_SPEED_UP`、`WPNAV_RADIUS`、`PILOT_SPEED_DN`，以及 `ARMING_CHECK`、
  `THR_MID`、`TELEM1_BAUD`。
- 模拟参数集覆盖率从 77% 提升到 97%，唯一 fallback 保留为测试用未知参数
  `MGC_UNKNOWN`；不伪造缺失参数的完整范围或枚举。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.38 - 2026-10-01

### Changed

- 参数元数据覆盖报告现在按固件版本的 canonical 参数名统计，旧固件中的
  `WPNAV_*` 等名称会映射到当前描述库名称，不再被误报为缺失。
- 补充 QGC ArduPilot 4.7 常见重映射：`PILOT_SPD_DN`、`Q_PILOT_SPD_DN` 和
  `ARMING_SKIPCHK`。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.37 - 2026-10-01

### Changed

- 新增 `mg_config_get_metadata_coverage`：按当前固件/机型 profile 统计已加载参数中
  described、fallback 和覆盖率，并列出缺少本地描述的参数名。
- 覆盖报告只读，不修改参数缓存、不发送 MAVLink 请求，也不伪造缺失元数据；用于指导
  后续按真实 ArduPilot 参数页缺口补齐描述库。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.36 - 2026-10-01

### Changed

- 参数前缀刷新现在支持版本重映射反查：在旧 ArduPilot 固件上，用户请求 `WP_*` 时，
  参数缓存中的 `WPNAV_*` 会通过 canonical 名称匹配并参与刷新。
- firmware plugin 增加 `canonical_param_name`，ArduPilot 实现反向遍历版本表，PX4
  保持原名；精确名称重映射、重试和状态查询行为保持不变。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.35 - 2026-10-01

### Changed

- 参数页现在会把已从飞控加载、但描述库未覆盖的参数归入稳定的 `other` 分组，不再让
  这些参数从分组接口中消失。
- `other` 分组会从 MAVLink 参数类型推断基础 `int/float` 类型，保留原始参数名和实时
  值，并用 `found:false` 明确标记缺少本地描述；不伪造范围或枚举信息。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.34 - 2026-10-01

### Changed

- 参数写入和精确刷新现在会经过 ArduPilot 固件版本参数重映射：例如旧固件收到
  `WP_SPD` 时会转换为 `WPNAV_SPEED`，新固件保持原名。
- `mg_params_set`、`mg_config_write_param`、`mg_config_write_params` 和
  `mg_params_refresh` 使用重映射后的参数名查缓存并发送；未知版本、PX4 和未知参数
  保持原样。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.33 - 2026-10-01

### Changed

- 为批量参数刷新增加异步任务状态和重试：每个名称记录发送时间、重试次数和目标
  sysid/compid，1 秒无回复时最多重发 2 次，重试耗尽进入 failed。
- 新增 `mg_params_get_refresh_status`，返回 active/total/completed/pending/round/failed；
  `PARAM_VALUE` 完成对应刷新，`PARAM_ERROR` 立即失败，原 `mg_params_refresh` 响应保持兼容。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.32 - 2026-10-01

### Changed

- 新增 `mg_params_refresh`：接受 JSON 名称数组，支持精确参数名和 `PREFIX*` 前缀，
  基于当前参数缓存去重解析，并通过现有 `PARAM_REQUEST_READ` 发送刷新请求。
- 刷新响应返回 `requested`、`resolved`、`sent`、`failed`、`duplicates` 和 `unknown`；
  不自动下载整表，也不改变现有 `mg_params_request_all` 的语义。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.31 - 2026-10-01

### Changed

- 参数描述库校验现在会拒绝 `read_only` 参数；单参数写入和批量写入使用同一套
  描述库规则，覆盖 read-only、枚举、范围、bool 和缺失描述。
- `mg_config_write_params` 不再绕过元数据校验，并在原有 `written` / `failed`
  字段外增加 `errors` 数组，逐项返回参数名和拒绝原因；合法项仍会正常发送。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.30 - 2026-10-01

### Changed

- 补上阶段 D 的参数明确拒绝路径：识别标准 MAVLink `PARAM_ERROR`，在收到
  `does not exist`、`out of range`、`permission denied`、`read-only`、类型不匹配等
  错误时立即把对应 pending 写入标记为 `failed`，保留映射后的原因并移除重试队列。
- 增加协议层和参数状态机测试，覆盖 PARAM_ERROR 路由、失败终态和失败回调；
  不修改公开 FFI 字段和参数写入调用方式。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.29 - 2026-10-01

### Changed

- 完成阶段 D 的第一个参数写入增强：`PendingWrite` 记录目标 sysid/compid；
  PARAM_SET 超时后不再只增加重试计数，而是通过 `process_param_timeouts()` 真正重发，
  重试耗尽仍进入 `failed` 并保留错误原因。
- 发送 PARAM_SET 失败时立即清理 pending 状态，避免后续 PARAM_VALUE 或超时误更新同一
  写入记录；参数下载、写入历史、FFI 和 `pending / confirmed / failed` 字段保持不变。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.28 - 2026-10-01

### Changed

- 完成第二阶段 C 的收口拆分：新增 `mavlink_adapter_command.cc`、
  `mavlink_adapter_params.cc`、`mavlink_adapter_log.cc` 和
  `mavlink_adapter_dispatch.cc`，迁出命令队列发送、参数请求/超时、日志接收、
  MAG_CAL_PROGRESS 兼容解析和 `process()` 消息分发。
- `mavlink_adapter_internal.h` 从 553 行降到 290 行，现在只保留 `Impl` 状态、
  方法声明和底层 `send_msg()`；`process()` 的锁范围、消息顺序、超时检查和锁外回调
  执行语义保持不变。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.27 - 2026-10-01

### Changed

- 完成第二阶段 C 的第三项 MAVLink 内部拆分：新增
  `core/src/mavlink/mavlink_adapter_mission.cc`，将任务计数、任务项、任务请求、任务
  ACK、当前航点和到达通知，以及任务发送辅助函数共 15 个方法移出头文件。
- `mavlink_adapter_internal.h` 从 769 行降到 553 行；`process()` 的锁范围、消息顺序、
  任务超时检查和延迟回调执行位置未改变，任务上传/下载和 FFI 行为保持不变。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.26 - 2026-10-01

### Changed

- 完成第二阶段 C 的第二项 MAVLink 内部拆分：新增
  `core/src/mavlink/mavlink_adapter_handlers.cc`，将心跳、系统状态、GPS、IMU、姿态、
  VFR HUD、参数值、命令 ACK、校准进度、RC、Servo、位置、电池、振动、ESC、ADS-B、
  视频流以及相机/云台共 30 个消息处理器移出头文件。
- `mavlink_adapter_internal.h` 从 1269 行降到 769 行；`process()` 的锁范围、消息顺序、
  超时检查和延迟回调执行位置未改变，公开 FFI 和 MAVLink 行为保持不变。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.25 - 2026-10-01

### Changed

- 完成第二阶段 C 的第一项 MAVLink 内部拆分：新增
  `core/src/mavlink/mavlink_adapter_state.h`，将遥测最新值、链路质量、日志下载和
  Command ACK 等待状态从 `mavlink_adapter_internal.h` 迁出。
- `mavlink_adapter_internal.h` 现在只保留 `Impl` 组合、协议辅助方法和消息分发入口，
  文件从 1434 行降到 1269 行；公开 FFI、MAVLink 行为、JSON 契约和 Flutter 绑定不变。

### Verification

- `flutter analyze lib test` 通过。
- Flutter 测试 `20/20` 通过。
- 后端 offline/simulation `34/34` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.24 - 2026-10-01

### Changed

- 完成第二阶段 B 的第五个内部重构：新增 `FlightCommandFfiBackend`，将 RTL、降落、
  起飞、解锁、模式切换、任务启动、引导点、暂停、环绕和设置 Home 的动态绑定与 JSON
  解包从 `ConnectionController` 迁出。
- QGC/MP 组合的飞行模式门控、解锁确认、模式心跳验证和安全敏感流程仍由
  `ConnectionController` 管理，现有飞行命令行为保持不变。

### Verification

- `flutter analyze lib test` 通过。
- Flutter 测试 `20/20` 通过。
- 后端 offline/simulation `34/34` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.23 - 2026-10-01

### Changed

- 完成第二阶段 B 的第四个内部重构：新增 `CalibrationFfiBackend`，将校准启动、取消、
  状态读取和加速度计姿态确认的动态绑定与 JSON 解包从 `ConnectionController` 迁出。
- 校准状态机、取消收敛、MAV_CAL 进度映射、六面校准流程和 UI 状态保持在主控制器中，
  校准页面行为和现有 FFI 契约不变。

### Verification

- `flutter analyze lib test` 通过。
- Flutter 测试 `20/20` 通过。
- 后端 offline/simulation `34/34` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.22 - 2026-10-01

### Changed

- 完成第二阶段 B 的第三个内部重构：新增 `ParameterFfiBackend`，将参数请求、参数快照、
  参数进度、参数描述读取和参数写入的动态绑定与 JSON 解包从 `ConnectionController`
  迁出。
- 参数缓存、元数据批处理、写入队列、写入确认和 ArduPilot 参数语义保持在主控制器中，
  参数页面行为和现有 FFI 契约不变。

### Verification

- `flutter analyze lib test` 通过。
- Flutter 测试 `20/20` 通过。
- 后端 offline/simulation `34/34` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.21 - 2026-10-01

### Changed

- 完成第二阶段 B 的第二个内部重构：新增 `TelemetryFfiBackend`，将会话状态、GPS、
  位置、Home、VFR HUD、Servo/RC、系统状态和 telemetry freshness 的动态符号绑定与
  JSON 解包从 `ConnectionController` 迁出。
- `ConnectionController` 继续负责遥测状态缓存、轮询频率和 UI 通知，遥测页面与地图
  行为保持不变。

### Verification

- `flutter analyze lib test` 通过。
- Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 后端 offline/simulation `34/34` 通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.20 - 2026-10-01

### Changed

- 完成第二阶段 B 的第一个内部重构：新增 `MissionDocumentFfiBackend`，将任务文档
  FFI 绑定、JSON envelope、任务传输进度、回读验证和文件互操作从
  `ConnectionController` 迁出。
- `ConnectionController` 不再直接实现 `MissionDocumentBackend`，保留共享 Core、动态库
  生命周期和连接状态职责；任务页面公开契约和用户行为保持不变。
- 任务回读验证增加可接受协议归一化结果的独立记录，并继续显示真正差异的详细字段。

### Verification

- `flutter analyze lib test` 通过。
- Flutter 测试 `20/20` 通过。
- 后端 offline/simulation `34/34` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.19 - 2026-09-30

### Fixed

- 磁力计校准在真实 ArduPilot 飞控上现在会按 QGC/MP 的 MAV_CAL 流程先发送
  `MAV_CMD_DO_CANCEL_MAG_CAL` 清理旧状态，再显式请求 `MAG_CAL_PROGRESS` 10 Hz 和
  `MAG_CAL_REPORT` 1 Hz，最后发送 `MAV_CMD_DO_START_MAG_CAL`。
- 修复 `CalibrationStatus.toJson()` 将 `Map<int,int>` 直接交给 `jsonEncode` 导致的
  “Converting object to an encodable object failed” 异常。此前第一次收到罗盘进度时
  状态更新就中断，所以进度条和取消后的界面一直停留在旧状态。
- 取消磁力计校准现在先立即清除前端运行态，再向飞控发送取消并停止 191/192 消息流；
  即使取消命令发送失败，本地状态也不会再卡在“校准中”。

### Verification

- 真实 ArduPilot 飞控验证：开始校准后收到连续 `MAG_CAL_PROGRESS`，进度从 0% 正常
  增长到 8%，两个罗盘分别显示；点击取消后进度条立即消失，开始按钮恢复，后端记录
  `Calibration cancelled by user`。
- 新增 Flutter 回归测试，覆盖含罗盘进度映射的 JSON 编码。
- 聚焦后端测试通过：`test_sim_calibration`、`test_sim_ui_paths`、`test_ffi_contract`。
- 完整 `tools/preflight.sh` 在提交前执行。

## 0.0.18 - 2026-09-30

### Added

- 新增 `ffi_operations.cc` 和 `ffi_operations_internal.h`，把长任务的统一快照与取消
  从系统诊断文件中分离。

### Changed

- params、mission、calibration、log 的状态快照和取消入口现在集中在一个 operations
  模块；系统诊断通过内部接口复用同一份快照。
- `ffi_diagnostics.cc` 保留连接诊断、遥测 dashboard、参数写入历史和系统诊断聚合，
  不再同时维护长任务状态语义。
- `mg_operations_get_snapshot` / `mg_operations_cancel` 的 C ABI、JSON 字段和取消
  行为保持不变，FFI 导出总数仍为 285。

### Verification

- 聚焦测试通过：`test_ffi_contract`、`test_ffi_comprehensive`、
  `test_state_machines`、`test_sim_full_flow`、`test_sim_ui_paths`。
- `tools/preflight.sh` 通过：Flutter analyze/test、后端 offline/simulation 34/34、
  UI token 检查和 macOS debug 构建。

## 0.0.17 - 2026-09-30

### Added

- 新增 `ffi_mission_interchange.cc` 和 `ffi_mission_shared.cc`，把任务文件
  FFI 入口与公共文档快照/校验错误序列化从任务编辑 FFI 中分离。

### Changed

- `ffi_mission_document.cc` 现在只负责任务文档创建、编辑、校验、编译、上传、
  从飞控导入、回读验证、估算和本地文档保存/读取。
- `mg_mission_doc_export_format` / `mg_mission_doc_import_format` 的实现移至独立
  互操作文件，公开 C ABI、JSON 字段、warnings 和错误结果保持不变。
- FFI 自动审计仍报告 285 个导出；前端使用 61 个，测试触达 198 个。

### Verification

- 聚焦 FFI/任务测试通过：`test_ffi_contract`、`test_ffi_comprehensive`、
  `test_bridge_mission_workflow`、`test_mission_document`、
  `test_sim_mission_document`。
- `tools/preflight.sh` 通过：Flutter analyze/test、后端 offline/simulation 34/34、
  UI token 检查和 macOS debug 构建。

## 0.0.16 - 2026-09-30

### Added

- 新增 `mission_waypoints.cc` 和 `mission_qgc_plan.cc`，把 MP `.waypoints` 与
  QGroundControl `.plan` 的读写实现从通用互操作文件中分离。

### Changed

- `mission_interchange.cc` 现在只负责格式识别和按飞控/任务复杂度推荐交换格式。
- `.waypoints` 的分隔、解析、Home 行和 MAVLink 任务行编码集中在独立模块。
- `.plan` 的 SimpleItem、Home、固件/机型、速度、warnings 和错误边界集中在独立模块。
- 公开任务文件 FFI、JSON 字段、warnings 和错误文本保持不变。

### Verification

- 聚焦互操作测试通过：`test_bridge_mission_workflow`、`test_mission_document`、
  `test_sim_mission_document`。
- `tools/preflight.sh` 通过：Flutter analyze/test、后端 offline/simulation 34/34、
  UI token 检查和 macOS debug 构建。

## 0.0.15 - 2026-09-30

### Added

- 新增 `mission_compile.cc` 和 `mission_estimate.cc`，把任务编译、Survey 展开和
  飞行估算从任务文档服务中分离。

### Changed

- Survey/Corridor 展开统一由 `expand_survey_entity` 提供；编译和估算不再各自持有
  一套调用逻辑。
- `MissionDocumentService::compile_document` 和 `estimate_document` 保留为稳定入口，
  内部委托给独立实现，任务编译结果、错误诊断和估算字段保持不变。
- `mission_document.cc` 在 0.0.14 的基础上继续缩小，当前只保留文档服务、历史、
  序列化、持久化和实体辅助逻辑。

### Verification

- 聚焦任务测试通过：`test_bridge_mission_workflow`、`test_mission_document`、
  `test_sim_mission_document`、`test_sim_survey`。
- `tools/preflight.sh` 通过：Flutter analyze/test、后端 offline/simulation 34/34、
  UI token 检查和 macOS debug 构建。

## 0.0.14 - 2026-09-30

### Added

- 新增分阶段重构计划，记录任务、互操作、FFI、operations、参数、连接和 Flutter
  的后续拆分顺序与验收条件。

### Changed

- 将任务文档校验层从 `mission_document.cc` 拆到独立的 `mission_validation.cc`：
  文档级、任务实体、Survey/Corridor 和 MAV_CMD 参数校验现在集中在单一模块。
- `mission_document.cc` 只保留文档服务、undo/redo、revision、序列化、持久化和实体
  辅助逻辑；内部共享类型通过 `mission_document_internal.h` 暴露。
- 公开任务文档头文件、FFI 名称、JSON 字段和错误语义均未改变。

### Verification

- 任务聚焦测试：`test_bridge_mission_workflow`、`test_mission_document`、
  `test_sim_mission_document` 全部通过。
- `tools/preflight.sh` 通过：Flutter analyze/test、后端 offline/simulation 34/34、
  UI token 检查和 macOS debug 构建。

## 0.0.13 - 2026-09-30

### Added

- 统一 operations 快照和取消入口覆盖参数、任务、校准与日志。前端可通过同一套
  `mg_operations_get_snapshot` / `mg_operations_cancel` 读取进度、失败原因和终态，
  不再为每一类长任务维护互不一致的状态语义。
- 参数写入历史加入 `mg_system_get_diagnostics`，记录 requested/actual 值、pending/
  confirmed/failed 状态、错误和时间戳，便于定位参数写入未确认或失败的问题。
- QGC `.plan` 基础互操作：支持 SimpleItem、Home、巡航速度、固件/机型元数据和旧版
  `coordinate` 格式导入导出；复杂项、非空 GeoFence/Rally 和不可无损字段会明确报错
  或返回 warnings，不再静默丢弃。
- 新增任务文件格式策略和 ADR 0008：`MissionDocument` 是唯一内部主状态，ArduPilot/
  MP 默认使用 `.waypoints`，PX4/QGC 默认使用 `.plan`，复杂规划数据保留 `.plan`。
- Android USB 串口桥接改为 QGC 路线：使用 `usb-serial-for-android`、
  `SerialInputOutputManager` 和 JNI 回调接入 native `ILink`，并补充 Android APK、
  Windows 和 Linux bundle 的 CI/预检脚本。
- 新增 MP/QGC 上游实现择优准则和质量审计文档，明确按模块比较，而不是按项目整体站队。

### Changed

- 任务 ACK 错误增加 QGC 风格的稳定 `result_text`，覆盖 unsupported frame/command、
  invalid parameters、no space、denied 和 cancellation 等结果。
- 命令目录补充中文标签、实体类型和参数元数据，任务编辑界面直接从后端目录生成
  命令和参数表单。
- Flutter 任务规划、地图、连接状态和长任务交互与新的 operations/diagnostics 契约
  对齐；前端测试更新到当前 16 项。
- 文档索引、任务规划契约、架构说明和开发准则同步到当前实现，并记录 MP/QGC 的
  分项质量归属和复用边界。

### Fixed

- 参数、任务、校准和日志的取消入口现在返回真实终态；没有活动操作时不会把已完成
  的操作追认为取消。
- `.plan` 导入不会静默忽略复杂项或非空 GeoFence/Rally；无法无损表达时返回明确错误，
  可继续但会降级的字段通过 warnings 暴露。
- Android 平台数据文件通过 JNI 和 AssetManager 解压到缓存目录，native Core 不再依赖
  自定义文件描述符接管方案。

### Verification

- `tools/preflight.sh` 通过：UI token 检查、`flutter analyze lib test`、Flutter
  test 16/16、后端 offline/simulation 34/34、macOS debug 构建通过。
- `git diff --check` 通过；版本号已同步到 CMake、pubspec、README 和 CHANGELOG。
- Windows、Linux、Android 构建由 CI 路径验证；本机未替代真机串口和权限验收。

## 0.0.12 - 2026-09-30

### Added

- Mission planning UI. The route-planning panel (docked to the map's right edge,
  collapsible into an arrow button) now has two tabs: *任务* and *设置*. The task
  tab shows a Home card plus one card per mission item — numbered badge, command
  dropdown, edit / move up / move down / delete, altitude and position. The
  settings tab edits the mission-wide default altitude and speed.
- The panel has explicit *从飞控读取* and *上传到飞控* buttons, matching Mission
  Planner's Read/Write and QGroundControl's Download/Upload. Reading confirms
  first when local items would be overwritten; neither side auto-downloads.
- Clicking the map with the panel closed opens the guided-actions sheet (move to
  here, RTL, land, takeoff, pause, orbit). Availability follows QGC's
  `GuidedActionsController` gating — move-to-here and orbit require the vehicle to
  be flying, RTL additionally requires armed and not already in RTL, land requires
  armed and excludes fixed wing, takeoff is only offered while not flying, and
  every action asks for confirmation. Clicking the map with the panel open adds a
  waypoint at that position and opens the editor; tapping a waypoint marker edits
  it.
- `mg_command_set_home` (MAV_CMD_DO_SET_HOME), so the Home card can write the
  flight controller's home. Parameters match QGC's `Vehicle::doSetHome`
  (`param1=0`, `param5/6` degrees, `param7` metres); out-of-range coordinates are
  rejected locally. `API_CATALOG.md` notes that this changes the RTL reference.
- `mg_mission_doc_import_from_vehicle` reads the vehicle mission into the
  document. It requires a completed download (`invalid_state` otherwise) and
  replaces the entity list only.
- Command catalog entries now carry a Chinese `label` and an `entity_type`, so
  the UI renders the command dropdown and parameter forms straight from the
  backend instead of hard-coding MAV_CMD semantics.

### Changed

- The vehicle now requests `HOME_POSITION` and `EXTENDED_SYS_STATE` streaming at
  1 Hz once it has identified the flight controller. ArduPilot only sends Home on
  first boot and on arming, and does not send EXTENDED_SYS_STATE unless asked
  (QGC's `APMFirmwarePlugin` documents both), so without the request the map's
  Home marker stayed empty and `landed_state` was never available for guided-action
  gating.
- `MissionDocumentController` is no longer unused: the panel drives it directly.

### Fixed

- `_INT` coordinate frames (5/6/11) are normalised to 0/3/10 when importing a
  mission. Vehicle readback uses the `_INT` frames, and the document validator
  only accepted 0/2/3/10, so a real mission imported from a flight controller
  failed its own validation with `entity.frame_unsupported`.
- Mission upload no longer reports success on the start of the transfer alone;
  the client polls `mg_mission_get_progress` until the transfer finishes.

### Verification

- Backend: `ctest -L "offline|simulation"` 34/34. `test_sim_commands` 64/64
  (includes the SET HOME parameter encoding), `test_sim_ui_paths` 49/49
  (Home plus EXTENDED_SYS_STATE streaming requests), `test_sim_mission_document`
  20/20 (import from vehicle, including the frame normalisation),
  `test_mission_document` 122/122.
- Frontend: `flutter analyze` clean, `flutter test` 12/12,
  `dart tools/check_ui_tokens.dart` pass, `flutter build macos --debug` succeeds.

## 0.0.11 - 2026-09-29

### Added

- `core/tests/test_sim_ui_paths.cc` covers the five FFI entry points that real
  UI paths call but no test touched: `mg_calibration_cancel`,
  `mg_params_get_by_name`, `mg_stream_set_interval`,
  `mg_vehicle_request_firmware_version` and `mg_serial_get_uart_names`.
- `MockFlightController` now answers a minimal MAVLink FTP subset
  (ResetSessions / OpenFileRO / ReadFile / TerminateSession), so the
  `@SYS/uarts.txt` download behind `mg_serial_get_uart_names` runs offline in
  under a second instead of waiting out the 3 s x 4 retry budget.
- `test_sim_commands` pins the encoding of the safety commands
  (`mg_command_set_mode`, `mg_command_rtl`, `mg_command_land`,
  `mg_command_guided_point`) plus the takeoff-altitude and loiter-radius
  setters.
- `test_sim_params` covers the batch parameter change set: duplicate collapse,
  unknown/unchanged accounting, malformed JSON rejection, and `_ENABLE` written
  after its dependents.

### Changed

- `API_CATALOG.md` registers `mg_config_read_param`, `mg_config_write_param` and
  `mg_config_write_params` as the preferred parameter read/write entry points,
  and spells out how they differ from `mg_params_set` (descriptor validation,
  no need for a completed parameter download, REAL32 fallback).
- The flight mode page derives its PWM bands from the vehicle profile: fixed
  wing mode 1 additionally accepts `>= 2050`, and out-of-range RC values no
  longer select a slot.
- `tools/audit_status.sh` scans the test dimension for bare `mg_*` symbols
  instead of only call sites, so entries passed as function pointers are counted.
- `component_gallery_demo.dart` is documented as a development-only target: it
  is not reachable from `main.dart`, does not ship in a release build, and is the
  one intentional token-scan exemption.

### Fixed

- Calibration verification no longer reports success when the flight controller
  parameter readback fails. The previous check only failed on `Poor` quality, so
  a failed readback still surfaced as "calibration successful".
- Radio calibration treated `MAV_RESULT_ACCEPTED` as completion. The ACK only
  confirms the start command; the operator still has to move the sticks, so the
  stage now stays `Calibrating`.
- `mg_calibration_get_history` rejected type 4, so radio calibration history was
  always empty; the bound now matches the 0..4 contract in `API_CATALOG.md`.
- `mg_command_guided_point` carried `lat*1e7` / `lon*1e7` in the param5/param6
  slots of a `COMMAND_LONG`, where `MAV_CMD_DO_REPOSITION` expects degrees. The
  flight controller multiplies by 1e7 itself, so the coordinate overflowed.

### Verification

- Backend: `ctest -L "offline|simulation"` 34/34 pass; `test_sim_ui_paths` 26/26,
  `test_sim_commands` 58/58, `test_sim_params` 39/39, `test_sim_calibration` 75/75.
- Frontend: `flutter analyze lib test` clean, `flutter test` 12/12,
  `dart tools/check_ui_tokens.dart` pass, `flutter build macos --debug` succeeds.
- `tools/audit_status.sh` reports 281 exports / 48 used by frontend / 187 touched
  by tests / 93 untouched.

## 0.0.10 - 2026-09-29

### Added

- Map now draws the flight controller's Home point and the aircraft's current
  position, fed by the existing `mg_vehicle_get_global_position` and
  `mg_vehicle_get_home_position` caches at the regular telemetry rate. The
  aircraft marker rotates with heading; neither marker is drawn while the fix is
  invalid.
- Map view preferences are persisted (tile source, zoom, centre) under the app
  support directory, so the map reopens where it was left. New dependency:
  `path_provider` (BSD-3-Clause).
- Per-compass magnetometer calibration progress. `mg_calibration_get_status` now
  carries `compass_progress: [{compass_id, completion_pct}]` alongside the
  aggregate `progress`, and the calibration card draws one bar per detected
  compass.
- `docs/research/FONT_ALTERNATIVES_RESEARCH.md` and ADR 0007 record how the UI font stack
  was chosen, with per-glyph similarity measurements.

### Changed

- UI font is now a single bundled family, `MiSans`, on every platform, so macOS
  and Windows render identically. The previous stack silently resolved to
  Helvetica Neue / PingFang SC / Menlo on macOS because `SF Pro Text` is not a
  resolvable family name.
- Only titles are bold. Numeric readouts, status labels, badges, sensor models,
  firmware names and sidebar entries all use `w400`; the sidebar selection is
  expressed by its accent background instead of font weight.
- The sidebar is collapsible from a chevron next to the (now removed) search box
  and collapses to icons only.
- Waveform polling runs at 10 Hz instead of 1 Hz, matching the requested stream
  rate; `MgAnimatedNumber` tweens in 100 ms so the readout keeps up.
- Waveform window is 40 s.
- Motor output PWM bars are 18 px tall and use a smaller numeric size; the same
  widget is reused for RC calibration.
- Flight mode cards highlight the slot selected by the live `FLTMODE_CH` PWM, and
  RC5-16 cards highlight only when the channel sits in its high band.
- All-parameters page is a divider-separated table with Chinese display names and
  descriptions where the descriptor database provides them, plus unit labels.
- Empty pages keep their widget skeleton with a status strip instead of showing a
  blank page and a few lines of text.
- Arming checks use `ARMING_SKIPCHK` on current firmware with `ARMING_CHECK` as a
  fallback, rendered as a divided grid.
- Compass orientation and calibration tolerance are bound to real parameters
  (`COMPASS_ORIENT`, `COMPASS_CAL_FIT`) instead of the previous dead controls.
- Magnetometer field strength is refreshed for every compass instance, not only
  the selected one.

### Fixed

- Map coordinates: the flight controller reports WGS-84 while AMap tiles use
  GCJ-02, so markers were offset by 100-700 m. Conversion now follows Mission
  Planner's `MercatorProjectionGCJ`, with a three-step inverse that converges to
  centimetres.
- Calibration failures no longer discard the flight controller's own message.
  Both the STATUSTEXT and COMMAND_ACK paths previously replaced it with generic
  guidance, which is why every failure looked like "move away from magnetic
  interference".
- Accelerometer quality thresholds were an order of magnitude too strict for
  ArduPilot's `INS_ACCOFFS_*` range (+/-3.5 m/s2), so successful calibrations
  were reported as failed.
- Compass quality now trusts ArduPilot's own fitness score when available instead
  of taking the worse of fitness and an offset-magnitude heuristic.
- `DO_START_MAG_CAL` now sends `param2=1` (retry on failure) like Mission
  Planner, instead of disabling the flight controller's own retry.
- Motor output table width follows the panel; it previously stayed at 1040 px, so
  widening the window left blank space and narrowing it overpainted the border.
- `copy_points` with `max_points=1` returns zero points; the "latest sample"
  lookup now uses a 1 ms window instead, with a regression test pinning both
  behaviours.

### Verification

- Backend offline/simulation gate: 33 passed.
- Flutter static analysis: passed.
- Flutter widget tests: 12 passed.
- UI token check: passed.
- macOS debug build: passed.

## 0.0.9 - 2026-09-29

### Added

- Added the mission planning engine with its Flutter-side document controller,
  validator, estimator, and upload/verify bridge.
- Added the arming checks page: 19 checks driven by a built-in bit table, reading
  and writing `ARMING_SKIPCHK` on newer firmware with `ARMING_CHECK` as a
  fallback for older ones.
- Added RC transmitter calibration end to end: a new `CalibrationType::Radio`
  strategy sending `MAV_CMD_PREFLIGHT_CALIBRATION` param4=1, `RCn_MIN/TRIM/MAX`
  read-back verification, and a Flutter page with live channel travel and
  `RCn_REVERSED` toggles.
- Added a staged PID tuning page with basic/extended levels for multirotor,
  fixed-wing, and VTOL. Multirotor and VTOL share one layout and differ only in
  the `ATC_`/`PSC_` versus `Q_A_`/`Q_P_` prefix; fixed-wing uses
  `RLL_RATE_`/`PTCH_RATE_`/`YAW_RATE_` plus `RLL2SRV`/`PTCH2SRV`/`YAW2SRV`
  and TECS.

### Changed

- Reworked the motor output page: `macos_ui` progress bars with centered PWM
  readouts, `MacosSwitch` for servo reverse, and optimistic parameter writes that
  render immediately and roll back when the flight controller rejects the value.
- Raised the regular telemetry poll rate from 2 Hz to 10 Hz and removed the
  separate servo output limiter, so the PWM bars track live output.
- Dropped the "waiting for parameter confirmation" banner; write failures are
  still surfaced.
- Removed unused frontend declarations (`MgTypography.group`, `hasParameters`,
  `isCalibrationActive`, `CalibrationStatus.isSuccess/isFailed`, ...) and merged
  the duplicated parameter lookup and error-formatting helpers.
- Renamed `widgets/mg_range_bar.dart` to `mg_motor_output_bar.dart` so the file
  matches its widget.
- Tightened the arming checks layout into a divided grid with uniform row heights.

### Fixed

- Corrected the position and velocity PID parameter names to the ArduPilot 4.8
  `PSC_NE_*` / `PSC_D_*` spelling; the previous `PSC_POSXY_P`-style names do not
  exist on current firmware, so that block read nothing.
- Hardened the Windows serial runtime and bridge packaging.

### Verification

- Backend offline/simulation gate: 33 passed.
- Flutter static analysis: passed.
- Flutter widget tests: 7 passed.
- UI token check: passed.
- macOS debug build: passed.

## 0.0.8 - 2026-09-28

### Changed

- Connected the Flutter FFI loader to Windows `mgcore.dll`, Linux `libmgcore.so`,
  and Android `libmgcore.so` loading paths.
- Added Windows and Linux Flutter build steps that build and stage MgCore and its
  runtime JSON resources beside the desktop executable.
- Added the Android Flutter project and USB Host capability declaration. Android
  still requires a native USB-serial driver bridge before real USB flight-controller
  connections can be validated.
- Expanded the magnetometer calibration surface with per-instance discovery,
  compass mounting state, orientation controls, enable toggles, and field-strength
  summaries.
- Added a Flutter-rendered configuration workspace shell with responsive sidebar,
  search, navigation, toolbar, and explicit light/dark backgrounds; the page
  keeps its existing visual language without depending on macOS-only window
  plugins for these elements.
- Added shared typography and metrics tokens, animated numeric readouts, and a
  PWM range bar with minimum/midpoint/maximum markers for the motor output page.

### Verification

- Cross-platform workspace widget tests: 3 passed.
- Flutter static analysis: passed.
- macOS debug build: passed.

## 0.0.6 - 2026-09-27

### Added

- Added the minimal macOS Flutter shell for connection setup, telemetry, sensor waveforms and calibration workflows.
- Added IMU and magnetometer instance selectors with per-instance waveform buffers and sensor model labels.
- Added accelerometer calibration controls for six-face and level calibration, including step status, orientation placeholder, confirmation and cancellation.

### Changed

- Removed the `1s` mean readout from waveform value cards so the cards show only the latest sample.
- Aligned waveform instance selectors and chart content, and tightened the spacing between the waveform area and calibration cards.
- Added macOS bundle-resource lookup for `SensorDeviceInfo.json`, enabling sensor model decoding from packaged apps.
- Synchronized MgCore and Flutter shell version metadata to `0.0.6` (build `6`).

### Verification

- Flutter static analysis: passed.
- macOS debug build: passed.
- Native sensor and calibration tests: 3/3 passed.

## Backend branch - 2026-09-23

`New-main` was created from `v0.0.5` as the backend and interface branch. Its current shell and backend changes are released together in `main` as `v0.0.6`.

### Removed on this branch

- The Flutter desktop application and all Dart sources.
- Flutter desktop build scripts and toolchain checks.
- Frontend architecture, component and review documents.
- Frontend-specific release notes and the frontend session contract ADR.

### Changed on this branch

- CI now builds and tests the backend on Ubuntu, Windows and macOS.
- The Docker image and compose file only provision the backend toolchain.
- `README.md` and the maintained docs describe the backend contract instead of the Flutter client.
- `mg_vehicle_open()` now creates a session without changing active focus; `mg_vehicle_select()` is the explicit focus operation, while legacy single-connection helpers retain their compatibility behavior.

## 0.0.5 - 2026-09-23

### Changed

- Preserved the complete Flutter desktop frontend while restructuring the client into `application`, `infrastructure`, and `presentation` layers.
- Moved MgCore FFI and JSON adapters into `app/lib/infrastructure/mgcore/` and Flutter controllers/UI into `app/lib/presentation/`.
- Added dependency-direction guards for application, infrastructure, and presentation code.
- Fixed parameter fallback recovery, calibration callback lock boundaries, truncated magnetometer progress parsing, and hidden backstage polling.
- Synchronized CMake, FFI, and Flutter version metadata to `0.0.5` (build `5`).

### Verification

- Flutter tests: 119 passed.
- Native CTest offline/simulation gate: 29/29 passed.
- macOS Debug build: succeeded.

### Notes

- This release keeps the existing Flutter frontend intact. The backend-only client branch is created from this tag and intentionally removes the Flutter application there.

## 0.0.4 - 2026-09-23

### Added

- Capability-based application contracts for mission planning, geofence/Rally, flight logs, MAVLink FTP files, fleet management, and flight control.
- Mission editor support for local waypoint creation, map-coordinate waypoint insertion, validation, JSON/CSV export, and mission transfer progress.
- Flight Logs page with onboard storage browsing, file selection, download progress, cancellation, and upload progress.
- Fleet controller and frontstage vehicle panel for vehicle discovery, active-vehicle selection, batch arm/disarm, mode changes, RTL, landing, per-vehicle results, and active-vehicle restoration.
- Geofence and Rally workflows with validation plus JSON/KML import and export.
- Chinese/English localization catalogs, generated localization bindings, and localization contract tests.
- MacGlass shell contracts and shared frontstage models so the visual shell can be replaced without rewriting application logic.

### Changed

- Bumped MgCore and Flutter application versions to `0.0.4` (build `4`).
- Improved connection/session handling, parameter-session isolation, telemetry polling, and reconnect/disconnect behavior.
- Extended MAVLink FTP and logging FFI paths for onboard file listing, upload, download, cancellation, and progress reporting.
- Raised the macOS deployment target to 12.0 to match the current Flutter/Xcode toolchain baseline.
- Expanded the regression suite to cover fleet control, mission editing/validation, localization, coordinate conversion, parameter synchronization, vehicle files, and session capabilities.

### Verification

- Flutter static analysis: passed with no issues.
- Flutter tests: 114 passed.
- macOS debug build: `mgcs.app` built successfully.
- Native CTest offline/simulation gate: 29/29 passed.

### Known limitations

- Native multi-vehicle batch commands still route through the active-vehicle selection as a compatibility bridge; a future explicit `vehicle_id` command target would remove this temporary coupling.
- Camera, gimbal, and video FFI primitives are present but are not yet exposed as complete Flutter workflows.
- Mission command coverage, log analysis/replay, offline map packages, firmware flashing, mobile packaging, and real-hardware write-operation acceptance remain future work.

## 0.0.3 - 2026-09-10

### Added

- End-to-end TCP and UDP connection flow from the Flutter application through the MgCore FFI backend, including endpoint validation and focused network tests.
- Airframe configuration page for Copter and Plane/QuadPlane, driven by the flight controller's existing parameter metadata.
- Native platform font fallback for Windows, macOS, and Linux.
- Regression coverage for the unified connection drawer layout and network connection callbacks.

### Changed

- Bumped MgCore and Flutter application versions to `0.0.3`.
- Unified the Serial, TCP, UDP, and Bluetooth connection pages around the same `302 x 48` field geometry, label hierarchy, spacing, and glass control styling.
- Made the connection watchdog wake immediately when a disconnect or reset stops it.
- Kept vehicle shutdown outside the multi-vehicle manager mutex to avoid blocking transport teardown.
- Copied runtime metadata into every multi-config output directory so Windows Release tests resolve `SensorDeviceInfo.json` and related data files.
- Improved the Windows desktop build workflow and local toolchain usage.

### Notes

- Bluetooth remains a UI-only connection entry until a native Bluetooth transport and Dart application contract are added.
- Release verification passed 29 backend offline/simulation tests and 84 Flutter tests; Flutter static analysis reported no issues.
- A Windows Release package was built locally with the project-local toolchain and contains `mgcs.exe` and `mgcore.dll`.

## 0.0.2 - 2026-08-12

### Added

- Docker Compose development environment for Linux-based MgCore builds and Flutter checks.
- Shared MAVLink header generation script pinned to `pymavlink 2.4.49`.
- PowerShell and shell wrappers for the Docker development workflow.
- Flutter analyze/test job and reproducible MAVLink generation in GitHub Actions.
- Updated Chinese architecture and optimization reports.

### Changed

- Bumped MgCore and Flutter application versions to `0.0.2`.
- Isolated generated MAVLink headers in a Docker named volume.
- Made the UDP transport test use an available local port.
- Updated widget tests to assert the current calibration and safety UI contract.
- Fixed MSVC standard-library portability and kept multi-config DLL output at `build/mgcore.dll` for Flutter Windows packaging.
- Built the Linux MgCore shared library before Flutter live FFI tests in CI.
- Added a native PowerShell MAVLink generator and a Windows Flutter Release packaging CI gate.
- Packaged MgCore parameter, translation, sensor, and USB board metadata beside the Windows executable.

### Notes

- Docker validates shared C++ and Flutter code but does not produce native macOS or Windows desktop bundles.
- Hardware and manual tests remain opt-in and are not part of the default container or CI gate.
- Release verification passed 29 backend offline/simulation tests and 79 Flutter tests; Flutter static analysis reported no issues.
- The Docker Compose definition was validated, but the local image build could not reach Docker Hub authentication and should be repeated on an unrestricted network.
