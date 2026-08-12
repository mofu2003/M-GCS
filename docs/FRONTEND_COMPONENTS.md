# 前端组件与依赖

Flutter 前端保持一套跨平台业务 UI。平台差异只进入 native library、权限、文件系统、USB/串口、窗口和安全区适配层。

## 当前依赖

| 依赖 | 用途 | 使用边界 |
| --- | --- | --- |
| `ffi` | 调用 MgCore C ABI | 仅限 `application/mgcore/` |
| `window_manager` | 桌面窗口尺寸和生命周期 | 仅限应用入口或平台适配 |
| `flutter_map` | 地图渲染 | 通过 `features/map/` 封装 |
| `latlong2` | 地图坐标类型 | 仅限地图模块 |
| `geolocator` | 系统定位 | 通过 `MgDeviceLocator` 封装 |

未被运行时代码使用的视觉探索依赖不保留在 `pubspec.yaml`。新增依赖前必须确认用途、维护状态、许可证和跨平台行为。

## 设计系统

- 业务页面依赖 `ui/skin/` 和 `ui/layouts/` 的稳定入口。
- 当前默认皮肤是 `MacGlassSkin`，皮肤选择在编译时固定。
- 皮肤可以改变布局和表现，不能改变连接、遥测、参数、任务或安全确认语义。
- 高密度参数、日志和告警页面优先可读性与性能，透明、模糊和动画只局部使用。
- Windows 和 macOS 使用同一套页面结构和 token，避免长期维护平台分叉。

## 地图

地图源由 `features/map/MgMapCatalog` 统一登记，业务 UI 只接触项目定义的地图样式和 `MgMapSurface`。

- 公共瓦片仅用于开发验证，不是生产 SLA。
- attribution 必须持续可见。
- 定位权限失败或超时时保留默认位置，不阻塞应用启动。
- 商业发布前必须确定地图供应商、授权、Key 管理、配额、缓存和离线策略。

## Mission CAD 前端规则

Mission CAD 的地图或画布只是可替换的编辑器表面。Flutter 版本可以提供点选、框选、拖拽、吸附、图层显示、属性编辑和快捷操作；Swift、React、Web 或 CLI 也可以实现相同工作流。

- 前端通过应用层契约发送 `move_point`、`set_constraint`、`add_survey_area` 等语义编辑请求。
- 前端展示后端返回的路径、转弯预览、估算、诊断和源到任务项映射。
- 前端不得自行分配 MAVLink 任务序号、计算权威 ENU/WGS84 几何、判断机型约束或上传任务。
- 吸附和手势只用于辅助输入，必须转换为持久化的领域实体或约束。

详见 [Mission CAD 初步设计](MISSION_CAD.md)。

## 资产与授权

- 依赖、字体、图标、图片、地图、音效和示例数据都必须记录来源与许可证。
- 优先选择 MIT、BSD、Apache-2.0 等商业友好许可证。
- 不使用 Apple 官方 logo、SF Symbols、系统截图、App Store 资源或来源不明素材。
- 可以借鉴通用 Cupertino 视觉语言，但不能逐屏复制平台或第三方商业应用。
