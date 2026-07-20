# 前端组件库

当前 Flutter 版本：

```text
Flutter 3.44.6
Dart 3.12.2
```

本项目目标平台按优先级为 Windows、Android、macOS、iOS。组件库选择以商业授权清晰、跨平台一致、可封装、性能可控为主；macOS / iOS 风格是正式视觉方向，但必须通过项目自有跨平台设计系统实现，不作为平台专用实现目标。

## UI 策略

1. 正式业务 UI 使用项目自有跨平台设计系统，目标是在 Windows、macOS、Android 和 iOS 上保持一致的页面结构和视觉层级。
2. Windows 和 macOS 必须按同一套页面、同一套 token、同一套组件 wrapper 验收；不能为了贴近平台原生效果而分裂成两套界面。
3. 视觉风格必须最大程度复刻 macOS / iOS 的通用风格气质：圆角卡片、磨砂玻璃背景、半透明层级、柔和阴影、清晰工具栏、克制色彩和高质量排版。
4. 商业安全是硬约束：不直接复制 Apple 官方界面、SF Symbols、图标、截图、文案、App Store 资源或任何受限制资源。
5. `macos_ui`、`adaptive_platform_ui`、`liquid_glass_widgets` 等库可以作为参考、原型或封装后的实现细节，不得直接散落到 feature 页面。
6. 正式页面应优先依赖项目 wrapper，例如 `MgCard`、`MgGlassPanel`、`MgButton`、`MgSegmentedControl`、`MgStatusTile`、`MgDataTable`。
7. 参数表、日志、告警、任务列表等高密度页面默认优先可读性和性能，磨砂、透明和动画只能局部使用。

## 皮肤系统

第一版采用编译时固定皮肤，不在设置中提供运行时切换。

- 默认皮肤：`MacGlassSkin`。
- 皮肤可以替换前台/后台布局骨架和组件表现。
- 皮肤不能改变业务语义、安全确认或 MgCore 契约。
- 业务页面必须依赖 `app/lib/ui/skin/`、`app/lib/ui/layouts/` 和项目稳定 wrapper，不直接依赖 `app/lib/ui/skins/mac_glass/` 的具体 widget。
- `MacGlassSkin` 第一版是浅色玻璃风格：真实地图在底层，顶部状态栏、左侧导航、连接抽屉、左下姿态仪、右侧地图工具栏和右下地图切换均悬浮在地图上。
- 顶部状态栏左侧当前留空，右侧固定显示 GPS、电池、飞行模式、解锁状态和飞控信息五项；飞控信息可展开 macOS 风格 popover。真实消息数据尚未接入时显示 `0条消息`，popover 保持空列表，不伪造演示输出。
- 电池状态项第二行固定为“电压 + 电池图标 + 剩余百分比”；缺少遥测时显示 `--` 和 `--%`，不把默认数值伪装成真实电量。
- 连接抽屉宽 330px，位于仪表盘正上方并与仪表盘左边对齐；抽屉 `top: 102px`，顶部与左侧导航 dock 对齐，底部距仪表盘顶部 12px。打开连接不能覆盖或移动仪表盘。顶部标题已删除，四种连接方式选择与关闭按钮同排；Serial 页不显示重复的“传输方式”和未接入本轮布局的“飞控识别”。
- 后续商业客户皮肤可以新增独立目录，例如 `app/lib/ui/skins/customer_x/`，但不能复制业务状态机或直接调用 MgCore FFI。

## 地图能力边界

当前前台地图使用 `flutter_map` 作为渲染层，地图源通过 `features/map/MgMapCatalog` 统一登记，业务页面只接触 `MgMapStyle` 和项目封装的 `MgMapSurface`。

- 已接入开发验证地图源：`OpenStreetMap HOT 标准地图`（`b.tile.openstreetmap.fr/hot`）和 `OpenHikingMap 地形地图`（`tile.openmaps.fr/openhikingmap`）。地图选择器和菜单必须显示这些完整名称，后续新增来源继续通过 `MgMapCatalog` 登记。
- 地图源支持 fallback URL：标准图 fallback 到 `a.tile.openstreetmap.fr/hot`；地形图 fallback 到标准图。
- `tile.openstreetmap.org`、`tile.opentopomap.org` 和 `tile.openstreetmap.de` 在当前网络环境下曾出现真实 PNG 下载超时，不作为默认开发源。
- 地图 attribution 必须常驻显示在最右下角；地图选择框位于其上方，显示当前地图完整名称并向上展开来源菜单。两者右边缘对齐，不能被玻璃面板或工具栏遮挡。
- 地图在每次应用启动后的首次挂载中调用一次系统定位，成功后保持当前缩放级别并移动地图中心。定位服务关闭、权限拒绝、平台异常或 5 秒超时均静默跳过，不提示、不重试、不打开系统设置。macOS 通过 `NSLocationUsageDescription` 和 location entitlement 声明权限。
- 高德、商业离线地图、私有瓦片服务等只预留 provider 接口；未确认商业授权、Key 管理、配额、缓存和分发条款前，不接入正式页面。
- 不匿名调用未授权瓦片地址，不复制 Mission Planner 或 QGroundControl 的地图实现代码和素材。
- 公共瓦片仅用于开发期验证；商业发布前需要切换到授权清晰、可控配额和缓存策略明确的地图服务。

## 已安装依赖

| 包 | 当前版本 | 用途 |
|---|---:|---|
| `macos_ui` | `^2.2.2` | macOS 风格窗口、侧边栏、toolbar、列表、按钮等桌面控件。 |
| `adaptive_platform_ui` | `^0.1.107` | iOS 26+ / 平台自适应控件、卡片和系统模糊风格探索。 |
| `liquid_glass_widgets` | `^0.21.3` | Liquid Glass 风格按钮、菜单、搜索、sheet、toast、chip、surface bar 等组件探索。 |
| `window_manager` | `^0.5.2` | macOS/Windows 桌面窗口尺寸、位置、关闭行为和窗口事件管理。 |
| `flutter_map` | `^8.3.1` | 跨平台地图渲染层，用于前台全屏真实地图背景。 |
| `latlong2` | `^0.10.1` | 地理坐标类型和地图坐标计算辅助。 |
| `geolocator` | `^14.0.3` | MIT；调用 macOS/Windows 系统定位，并为后续 Android/iOS 共用实现保留路径。 |
| `cupertino_icons` | `^1.0.9` | Cupertino 图标。 |
| `ffi` | `^2.1.4` | Dart FFI 调用 native MgCore。 |

## 仪表组件边界

前台姿态仪采用项目自有 Flutter painter，视觉来源为旧版 `Downloads/M-GCS-main/app/lib/features/dashboard/attitude_compass.dart` 的自有实现。当前只迁移罗盘外环、姿态球和固定航向指针三层绘制结构，外层仍通过 `MacGlassSkin` wrapper 控制玻璃面板、读数、布局和跨平台一致性。

- 选择理由：旧实现已经接近目标参考图，且不依赖第三方视觉资源；迁移为当前项目 wrapper 后便于继续微调比例、颜色和动效。
- 使用边界：业务页面不能直接散落 painter；必须通过项目自己的姿态仪 wrapper 引用，后续换皮肤或更换仪表库时只改 wrapper。
- 预览状态：当前打开地面站后，姿态仪在显示层自动循环航向、横滚和俯仰，仅用于检查视觉效果；这些数值不是飞控遥测，也不会写入 `ConnectionController` 或后端。
- 商业安全：不采用 GPL/AGPL、无 license、来源链不清晰或疑似复制其他地面站/飞行仪表资源的 SVG、图片、JS 插件和示例资产。
- 视觉原则：罗盘外环、姿态球和固定指针由仪表 wrapper 负责，玻璃、阴影、面板尺寸、读数排布由项目 token 和皮肤层控制，确保 Windows/macOS 看到同一套界面。
- 读数原则：航向、俯仰、横滚只在右侧矩阵第一列各显示一次；姿态球下方不再设置重复读数条。
- 姿态面板左侧只保留 184px 姿态球，不再在球下方重复显示俯仰、横滚和航向读数。右侧使用固定 2 列 x 3 行矩阵：第一列为航向/俯仰/横滚，第二列为空速/地速/升降速度；高度、飞行距离、飞行时间不在本面板显示。
- 姿态面板为 450 x 216px，右侧数据区域高 196px；六项数据卡为 60px 高的“标签在上、数值在下”结构，两列间距 4px、三行间距 8px。地速复用现有数据，空速和升降速度在接口未确认前显示 `--`。姿态面板始终固定在 `left: 106px`、`bottom: 18px`，连接抽屉打开时也不移动。

## 授权要求

1. 新增依赖前必须确认 license，优先 MIT、BSD、Apache-2.0。
2. GPL、AGPL、SSPL、无 license、商业用途不清晰或来源不明的库不得进入正式产品。
3. LGPL、MPL、CC 素材、字体授权等需要单独记录分发义务和商用风险。
4. 图标、字体、图片、地图、音效和示例数据都必须有可追溯来源。
5. 若依赖只适合原型或参考，必须在文档中标记，不能进入业务页面。
6. 类 macOS / iOS 的外观只能使用项目自绘、Flutter 标准能力、商业授权清晰的字体/图标/素材和自有 token 实现。

## 暂不采用

- `flutter_platform_widgets`：pub 已标记 discontinued，不适合作为新项目基础依赖。

## 使用原则

1. 操作员工具优先清晰、稳定、可扫描，不为了效果牺牲可读性。
2. 主业务 UI 优先使用 Flutter 原生能力、自定义 Paint、`BackdropFilter` 和项目 wrapper 实现跨平台一致效果。
3. macOS / iOS 风格复刻必须先沉淀为 token 和 wrapper，再进入业务页面。
4. iOS 26 / Liquid Glass 风格先用于局部实验和低风险面板，不直接铺满参数表、日志、告警等高密度业务区。
5. 窗口行为统一通过 `window_manager` 封装，不在 feature 页面里散落平台窗口逻辑。
6. 引入新组件前先做小范围 demo 或 wrapper，再进入业务页面。
7. Windows 和 Android 是性能验收主平台；磨砂玻璃、阴影和动画必须在这两个平台优先验证。
