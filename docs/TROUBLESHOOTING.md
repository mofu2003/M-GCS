# 故障记录

本文件记录已经踩过、容易复发、会影响开发判断的问题。每条都写清楚现象、原因、修复和防再犯规则。

## 2026-07-18 连接后参数同步与信号状态

### 将遥控器 RSSI 错当成地面站链路质量

- 错误状态：连接完成标签把 `RC_CHANNELS.rssi` 直接标为“链路质量”，导致遥控器与数传状态不一致时误导操作员。
- 根因：MAVLink 中 RC 输入信号与地面站接收包质量来自不同数据源；`RC_CHANNELS` 只说明飞控接收到的遥控器信号。
- 修复：链路质量按活动飞控 MAVLink 包序号的 3 秒窗口成功率计算，写入 `mg_connection_get_diagnostics().link`；遥控器信号单独读取 `mg_rc_get_channels().rssi`。
- 防再犯：不得用心跳年龄、RC RSSI 或默认值代替真实 MAVLink 链路质量。样本不足显示“链路 测量中”，RC RSSI 为 `0` 或 `255` 显示“遥控器 不可用”。

### 连接建立即请求参数列表

- 错误状态：串口打开成功后立即请求参数，目标 sysid/component 尚未由心跳确认，参数请求可能无目标或发往错误组件。
- 根因：传输层打开与 MAVLink 会话确认是两个阶段；`mg_params_request_all()` 依赖活动飞控和目标 sysid/component。
- 修复：`ConnectionController` 只在会话快照确认 `connected` 后请求一次参数列表。下载中以 `received / total` 进度显示，`total` 未到达时显示不定进度。
- 防再犯：同一会话不能重复发送 `PARAM_REQUEST_LIST`；断开后才允许下一会话重新请求。参数同步只读，不得夹带写参数、解锁或模式命令。

## 2026-07-17 连接面板位置语义

### 误把顶部对齐理解为与导航等高

- 错误状态：用户要求连接页面在仪表盘上方，并说明“高度和左边长条一样”；实现错误地把连接面板放到仪表盘右侧，并让它与左侧导航 dock 上下边缘都对齐。
- 根因：把“高度一样”直接解释成组件高度相同，没有结合用户前后两条明确边界“连接页面在仪表盘上面”和“不允许覆盖也不允许移动仪表盘”；正确含义只是连接面板顶部与左侧长条顶部处于同一高度。
- 修复：仪表盘继续固定在 `left: 106px`、`bottom: 18px`；连接面板改为 `left: 106px`、`top: 102px`，底部固定在仪表盘顶部上方 12px。
- 防再犯：自然语言中的“高度一样”存在“尺寸相同”和“纵向位置相同”两种含义，必须同时核对方向词和已有约束；若两种解释冲突，以用户明确说明的上下关系为准并留痕。
- 回归测试：`app/test/frontstage_connection_drawer_test.dart` 锁定顶部、左边和 12px 垂直间隔；`app/test/widget_test.dart` 锁定打开连接前后仪表盘矩形完全一致。

### 连接面板缩小外框但内部仍显臃肿

- 错误状态：连接面板外框缩小后，内部仍保留 58px 信息行、52px 连接按钮、20px 内边距和冗余字段，视觉尺寸没有同步收紧。
- 根因：前一次调整只把“面板大小”作为验收条件，没有把内部控件高度、字号、内边距和字段范围写入测试与规格。
- 修复：连接面板放在仪表盘上方，顶部与导航 dock 对齐；内边距缩为 14px，信息行缩为 48px，连接按钮缩为 44px，输入框与蓝牙占位项同步收紧；Serial 页删除“传输方式”和“飞控识别”。
- 防再犯：面板尺寸调整必须同时检查容器、控件、文字和内容范围，不能只改最外层 `SizedBox`。
- 回归测试：`app/test/frontstage_connection_drawer_test.dart` 锁定面板矩形、字段不存在、信息行和连接按钮高度。

## 2026-07-17 地图上拉菜单

### 只加宽选择框导致完整地图名称在菜单中溢出

- 错误状态：关闭状态的地图选择框已从 140px 加宽到 248px，但打开菜单后两个完整地图名称分别向右溢出 80px 和 24px，Widget 测试报告 `RenderFlex overflowed`。
- 根因：`PopupMenuButton` 的弹出路由继续使用 Material 默认菜单宽度约束，菜单项中的 `Row` 也让文本按自然宽度布局；只修改按钮子组件宽度不会改变弹出菜单约束。
- 修复：为弹出菜单设置固定 380px 宽度约束，并用 `Expanded` 让名称文本占用图标之后的剩余空间。
- 防再犯：弹出式选择控件改成长文案时，必须分别验证关闭状态按钮和独立 overlay 菜单；两者不是同一个布局约束。
- 回归测试：`app/test/frontstage_layout_test.dart` 打开真实菜单，检查两个完整名称可渲染且测试过程没有 overflow 异常。

### Widget 测试只推进一帧导致菜单不可点击

- 错误状态：`PopupMenuButton` 的选项已经构建且位置在选择框上方，但测试点击被模态遮罩拦截，地图样式回调没有触发。
- 根因：点击后的第一次 `pump(Duration)` 才创建弹出菜单路由，动画从该帧开始；没有第二帧推进时菜单仍处于忽略点击状态。
- 修复：先执行一次无时长 `pump()` 创建路由，再固定推进 250ms 完成菜单动画，然后点击选项。
- 防再犯：测试新建 overlay route 时必须区分“创建路由帧”和“推进动画帧”；项目包含持续姿态动画，仍不能使用 `pumpAndSettle()`。
- 回归测试：`app/test/frontstage_layout_test.dart` 检查菜单在选择框上方，并验证地形选项回调。

## 2026-07-17 macOS 应用启动

### 旧进程退出期间 `open` 返回 -600

- 错误状态：Release 构建成功后立即执行 `pkill` 和 `open`，Launch Services 返回 `_LSOpenURLsWithCompletionHandler() failed with error -600`，新应用没有启动。
- 根因：旧 `mgcs` 进程尚处于退出和 Launch Services 状态清理阶段，紧接着复用同一应用标识启动产生时序冲突。
- 修复：确认终止旧进程后等待 2 秒，再使用 `open -n <完整 app 路径>` 启动；随后用完整可执行路径检查进程。
- 防再犯：替换正在运行的 macOS Release 应用时，在终止与重新打开之间保留短暂等待；遇到 -600 必须检查进程并重试，不能假装应用已经打开。

## 2026-07-17 紧凑姿态面板

### 用估算行高计算面板高度不准确

- 错误状态：增大姿态球和数据卡时，按字体大小估算面板总高为 235px，与 Flutter 最终布局不一致。
- 根因：`Text` 的实际布局高度包含字体度量，三项姿态读数条经 Widget 测试测得为 30px，不能只按声明的字号相加。
- 修复：使用已渲染的 30px 读数条计算：184px 姿态球 + 4px 间距 + 30px 读数条 + 20px 面板上下内边距 = 238px；测试锁定最终尺寸。
- 防再犯：涉及文字的固定格式面板必须以 Widget 渲染尺寸为准，不能用字号手算后直接写规格。

### 数据卡居中导致上下大块留白

- 错误状态：六项数据卡虽然缩小并改为上下信息结构，但三行整体集中在数据区中间；空速/地速上方和飞行距离/飞行时间下方各留一大块空白，视觉反而显得拥挤。
- 根因：191px 高的数据区域仍使用 `Center` 包裹只有 146px 高的网格，额外空间全部分配到网格外部，而不是三行之间。
- 修复：移除 `Center`，让网格占满数据区并使用 `MainAxisAlignment.spaceBetween`；第一行贴顶、最后一行贴底，剩余空间平均进入两段行距。卡片尺寸和卡片内部间距不变。
- 防再犯：判断布局密度时必须区分“组件内部间距”“组件之间间距”和“容器外部留白”；不能靠整体缩小替代空间分配。
- 回归测试：`app/test/frontstage_attitude_panel_test.dart` 检查首行与数据区顶部重合、末行与底部重合、两段行距相等且大于 4px。

### 只缩放尺寸导致视觉上几乎没有变化

- 错误状态：选择“紧凑双列矩阵”后，代码只把面板从 520px 缩到 480px、姿态球从 210px 缩到 176px；实际窗口中的内部排版和旧版相同，用户看不出明显变化。
- 根因：实现只关注整体尺寸与间距，没有把预览图中“标签在上、数值在下”的数据层级和细读数条纳入验收条件；原测试也只检查宽高，无法发现结构未变化。
- 修复：面板进一步重排为 450px 宽、160px 姿态球、46px 纵向数据卡和不超过 30px 的姿态读数条；测试增加数据卡内部标签与数值的垂直位置断言。
- 防再犯：视觉方案不能只翻译成外框尺寸；规格和测试必须同时覆盖组件层级、信息方向和关键对齐关系。
- 回归测试：`app/test/frontstage_attitude_panel_test.dart` 检查空速卡高度、标签位于数值上方以及姿态读数条高度。

### 读数卡内边距导致高度超出 4px

- 错误状态：面板宽度和姿态球尺寸已按方案 A 缩小，但 Widget 测试测得整体高度为 249px，超过不大于 245px 的目标。
- 根因：姿态球下方三项读数卡仍保留 6px 上下内边距，左右区域收紧后它成为整体高度的决定项。
- 修复：读数卡上下内边距从 6px 调整为 4px，最终面板高度降为 245px，不缩小字体或姿态球。
- 防再犯：紧凑面板不能只计算显式 `SizedBox`；必须用 Widget 测试读取最终渲染尺寸，包含文字行高、内边距和间距。
- 回归测试：`app/test/frontstage_layout_test.dart` 锁定面板宽 480px、整体高度不超过 245px；`app/test/frontstage_attitude_panel_test.dart` 锁定姿态球 176px。

## 2026-07-17 前台导航切换

### 任务入口错误替换仪表盘

- 错误状态：连接已改为独立浮层，但点击任务仍会隐藏仪表盘，入口行为不一致。
- 根因：任务和仪表盘仍共用互斥的 `FrontstageNavigationSection`，修复连接时只拆出了连接状态，没有清除剩余的互斥建模。
- 修复：将导航枚举改为只表达点击目标的 `FrontstageNavigationItem`，模型分别持有 `connectionDrawerVisible`、`missionPanelVisible` 和 `dashboardVisible`；每次点击只切换对应布尔值。
- 防再犯：工具栏上的独立面板入口不能共用“当前页面”枚举；除非业务明确要求互斥，否则每个入口必须有独立显隐状态和独立回归测试。
- 回归测试：`app/test/widget_test.dart` 覆盖任务与仪表盘同时高亮、任务开关不改变姿态面板、仪表盘可单独关闭。

### 连接浮层错误替换仪表盘

- 错误状态：点击左侧连接入口后，连接抽屉出现，但左下角仪表盘消失；只让两个组件同时渲染后，它们仍占据相同坐标并发生完全遮挡。
- 根因：同一个互斥导航状态同时控制主页面和连接抽屉，连接被错误建模成替代页面；连接抽屉与仪表盘的初始 `left` 也都为 106px。
- 修复：使用独立的 `connectionDrawerVisible` 浮层状态，连接点击不再修改仪表盘状态；连接打开时仪表盘定位改为 512px，位于 390px 宽连接抽屉右侧并保留 16px 间距。
- 防再犯：主内容切换和临时浮层必须使用独立状态；“两个 Widget 都存在”的测试不足以证明可见，还要用布局矩形断言两者不重叠。
- 回归测试：`app/test/widget_test.dart` 覆盖连接与仪表盘同时显示、同时高亮、关闭连接后仪表盘保留，以及两者矩形不重叠。

### 相似测试代码块导致补丁误命中

- 错误状态：修改多个 Widget 测试中的连接入口点击步骤时，补丁匹配到另一个结构相似的测试块，造成一个测试少点一次、另一个测试多点一次；新导航的二次点击关闭规则因此让断言失败。
- 根因：多个测试都包含相同的 `pumpWidget`、固定时长 `pump` 和连接入口点击片段，使用过短上下文的机械补丁无法区分测试用例。
- 修复：先用 `nl -ba` 按行核对完整测试方法，再使用包含测试名称和相邻断言的精确上下文修改。
- 防再犯：批量修改结构相似的 Widget 测试时，每次补丁必须包含测试名称或唯一断言；补丁后立即读取受影响的完整方法再运行测试。

## 2026-07-17 姿态仪预览动画

### Computer Use 按应用名读取到旧界面

- 错误状态：Release 应用已重新构建并启动，但按展示名 `mgcs` 读取窗口时仍返回旧版界面和旧可访问性树。
- 根因：同名应用或历史窗口状态会让按展示名定位产生歧义，不能据此判断当前 Release 包内容。
- 修复：使用 `build/macos/Build/Products/Release/mgcs.app` 完整路径重新读取，确认五项新状态和输出消息面板均来自当前构建。
- 防再犯：自动目视验收本地构建时，优先使用应用包完整路径，不只使用展示名。

### Flutter SDK 临时路径失效

- 错误状态：Flutter 测试报 `/private/tmp/flutter-mgcs-sdk/bin/flutter: no such file or directory`，随后依赖仍尝试从该路径加载。
- 根因：`/private/tmp` 会被系统清理，同时项目 `.dart_tool/package_config.json` 保留了旧 SDK 路径。
- 修复：统一使用持久路径 `/Users/fansicheng/development/flutter/bin/flutter`，并重新执行 `flutter pub get` 生成包配置。
- 防再犯：开发文档和脚本不得引用 `/private/tmp` 下的 Flutter SDK。

### 持续动画导致 `pumpAndSettle` 超时

- 错误状态：加入循环姿态动画后，页面组件测试报 `pumpAndSettle timed out`。
- 根因：`pumpAndSettle()` 要等待所有动画停止，而姿态仪的预览动画按设计不会停止。
- 修复：需要等待连接抽屉 180ms 过渡的测试改为固定推进 250ms。
- 防再犯：包含无限循环动画的页面测试使用明确时长的 `pump(Duration)`，不要使用 `pumpAndSettle()`。

## 2026-07-16 前台地图和连接抽屉

### 姿态仪组件库外观不贴近参考图

- 错误状态：用通用 Flutter 仪表组件拼出姿态仪后，虽然结构可用，但外观不接近用户给出的浅色罗盘参考图。
- 根因：通用组件的人工地平仪内部视觉较固定，外层只能调色和裁剪，无法准确复刻白色罗盘外环、柔和姿态球、固定黑色指针和简洁刻度比例。
- 修复：从旧版 `Downloads/M-GCS-main/app/lib/features/dashboard/attitude_compass.dart` 迁移项目自有三层 painter：旋转罗盘缓存层、独立姿态球层、固定航向指针层；外层仍由 `MacGlassSkin` wrapper 控制玻璃面板和读数布局。
- 防再犯：收到明确视觉参考图时，先查项目历史实现和可用自有资产；不要先用通用组件硬拼最终外观。商业安全仍然要求不复制 QGroundControl、Mission Planner 或来源链不清晰的 SVG/JS 仪表资源。
- 回归测试：`app/test/frontstage_attitude_panel_test.dart` 检查姿态面板实际渲染 `mg-compass-dial-layer`、`mg-attitude-ball-layer` 和 `mg-heading-pointer-layer`。

### 连接抽屉无法关闭

- 错误状态：点击连接抽屉右上角关闭按钮后，抽屉仍然显示。
- 根因：`FrontstagePage` 把 `drawerVisible` 计算为 `_connectionDrawerOpen || !connected`，未连接时永远强制显示。
- 修复：`drawerVisible` 改为只由 `_connectionDrawerOpen` 控制；左侧连接入口负责重新打开抽屉。
- 防再犯：UI 显隐状态不要和连接状态硬绑定。连接状态可以影响内容和按钮可用性，但不能覆盖用户关闭面板的意图。
- 回归测试：`app/test/widget_test.dart` 覆盖“未连接时关闭连接抽屉后不再显示”。

### macOS Release 地图瓦片无法加载

- 错误状态：地图区域没有真实瓦片，用户看到背景不像地图。
- 根因：macOS app sandbox 开启后，`Release.entitlements` 缺少 `com.apple.security.network.client`，Release app 没有出站网络权限。
- 修复：`DebugProfile.entitlements` 和 `Release.entitlements` 都加入 `com.apple.security.network.client`。
- 防再犯：任何需要 HTTPS API、地图瓦片、在线资源的 macOS 桌面功能，都必须同时检查 Debug/Profile/Release entitlements。
- 回归测试：`app/test/macos_entitlements_test.dart` 检查两个 entitlements 文件都声明出站网络权限。

### 公共瓦片服务或本机网络不可达

- 错误状态：即使 app 有出站权限，当前机器访问 `https://tile.openstreetmap.org/...` 也可能超时，地图瓦片仍然加载慢或加载失败。
- 根因：这是运行环境、网络链路或公共瓦片服务可达性问题，不一定是 Flutter 代码失败。
- 修复：地图组件底层增加项目自绘的轻量地图兜底背景；真实瓦片可达时会覆盖在兜底背景上。默认标准图改为真实 PNG GET 成功且较快的 `b.tile.openstreetmap.fr/hot`，fallback 到 `a.tile.openstreetmap.fr/hot`。地形图保留 `tile.openmaps.fr/openhikingmap`，fallback 到标准图。
- 防再犯：不要把公共 OSM / OpenTopoMap 瓦片当作生产依赖。商业发布前必须接入授权清晰、配额可控、可缓存或可离线的地图服务。
- 回归测试：`app/test/frontstage_layout_test.dart` 检查 `mg-map-fallback-background` 存在。

### 默认瓦片源必须做可达性验证

- 错误状态：代码里的默认瓦片 URL 合法，但用户机器上仍然加载不出来。
- 根因：`tile.openstreetmap.org`、`tile.opentopomap.org` 在当前网络环境超时；`tile.openstreetmap.de` 曾出现 HEAD 成功但真实 PNG GET 的 SSL 连接超时；`a.tile.openstreetmap.fr/osmfr` 也曾在后续验证中超时。合法 URL 或 HEAD 成功不等于 Flutter 能加载图片。
- 修复：用 `curl -L --connect-timeout 3 --max-time 12 <tile-url> -o /tmp/tile.png` 验证候选瓦片源，并用 `file /tmp/tile.png` 确认下载结果是真 PNG，再写入 `MgMapCatalog`。
- 防再犯：改默认地图源前必须同时验证标准图、fallback 标准图和地形图各一个实际 PNG URL，并把不可达域名记录在本文件。不要只做 HEAD 请求。
- 回归测试：`app/test/mg_map_catalog_test.dart` 检查默认源不是旧的不可达域名，而是当前验证过的开发源。

### zsh 与 bash 数组语法不兼容

- 错误状态：在 zsh 下运行 bash 风格 `${!urls[@]}`，脚本报 `bad substitution`。
- 根因：当前 shell 是 zsh，不支持该 bash 数组下标展开语法。
- 修复：使用 zsh 兼容的 `for url in $urls` 加手动计数，或显式用 `bash -lc` 运行 bash 脚本。
- 防再犯：当前项目终端默认按 zsh 处理；需要 bash 特性时，命令开头显式写 `bash -lc '...'`。

### `dart format` 误扫非 Dart 文件

- 错误状态：执行 `dart format lib test macos/Runner/*.entitlements` 后，entitlements XML 被 Dart parser 报大量语法错误。
- 根因：`.entitlements` 是 plist XML，不是 Dart 源码。
- 修复：格式化命令限定为 Dart 目录和 Dart 文件，例如 `dart format lib test`。
- 防再犯：不要把 plist、xcconfig、yaml、cmake、json 等非 Dart 文件传给 `dart format`。

### `Path` 类型名冲突

- 错误状态：自绘地图背景里 `Path().moveTo()`、`Path().cubicTo()` 被分析器报不存在，并出现 `Path<LatLng>` 类型不匹配。
- 根因：`flutter_map` 暴露了泛型 `Path<LatLng>`，和 Flutter canvas 使用的 `dart:ui Path` 同名。
- 修复：在自绘代码中使用 `import 'dart:ui' as ui;`，路径类型写成 `ui.Path()`。
- 防再犯：同时导入地图/几何库和 Flutter 绘制 API 时，所有可能冲突的绘制类型都使用命名空间前缀。

### 渐变 API 误用

- 错误状态：把 `LinearGradient` 改成 `ui.Gradient.linear(...).createShader(...)` 后，分析器报 `createShader` 不存在。
- 根因：Flutter widget 层的 `LinearGradient` 和 `dart:ui` 底层渐变 API 不是同一套调用方式。
- 修复：自绘背景的 shader 保持使用 Flutter `LinearGradient(...).createShader(rect)`。
- 防再犯：只有 canvas path 明确使用 `ui.Path`；不要为了“统一命名空间”把正常的 Flutter painting API 也替换掉。

### 本机截图验证失败

- 错误状态：`screencapture -x /tmp/mgcs-current.png` 返回 `could not create image from display`。
- 根因：当前 Codex 进程可能没有 macOS Screen Recording 权限，或当前会话没有可截图显示器上下文。
- 修复：不把截图失败当作 app 渲染失败；仍需通过构建、测试、进程启动和用户目视反馈验证。
- 防再犯：若需要自动视觉验收，优先使用 Flutter golden/testable widget 截图或给 Codex/终端授予屏幕录制权限。
