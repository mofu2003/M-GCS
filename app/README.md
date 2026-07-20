# M-GCS Flutter 前端

`app/` 是 M-GCS 的 Flutter 前端工程。长期目标平台按优先级为 Windows、Android、macOS、iOS；当前源码包已恢复 macOS/Windows 桌面骨架，后续需要重新打开并验证 Android/iOS 平台工程。

## 当前状态

- 已生成 Flutter 工程骨架。
- 已启用 macOS 和 Windows desktop。
- Android 和 iOS 是长期目标平台，当前需要重新启用平台工程、补齐权限和 native library 打包。
- Web、Linux 暂不作为当前目标平台。
- 已建立 `ConnectionSessionBackend` 应用契约。
- UI 通过 `ConnectionController` 接入应用契约，不直接调用 MgCore FFI。
- MgCore FFI 适配层已预留，默认尝试加载 native library；加载失败时退回离线演示后端。
- 已安装 macOS / iOS 26 / Liquid Glass 风格探索组件库，但正式业务 UI 必须通过项目自有 wrapper 封装后使用，详见 `docs/FRONTEND_COMPONENTS.md`。
- 前台默认使用 `MacGlassSkin`：真实地图作为背景，顶部状态栏、左侧导航、连接抽屉、左下姿态仪、右侧地图工具栏和右下地图切换均为悬浮 UI。
- 当前连接抽屉是前端布局预览，Serial / UDP / TCP / 蓝牙分段页暂不调用真实后端连接接口。
- 皮肤选择为编译时固定，服务商业客户定制；当前不提供运行时皮肤切换器。

## UI 和授权边界

- Windows 和 Android 是主力验收平台；macOS 和 iOS 是次级目标平台。
- Windows 和 macOS 上看到的页面布局、视觉层级、组件样式和交互动效必须尽可能一致。
- 前端视觉必须最大程度复刻 macOS / iOS 的通用风格气质，包括圆角卡片、磨砂玻璃背景、半透明层级、柔和阴影、清晰工具栏和高质量排版。
- 复刻必须商业安全：不复制 Apple 官方图标、SF Symbols、系统截图、App Store 资源、专有文案、平台专有界面或任何可能暗示官方关联的素材。
- 新增依赖、字体、图标、图片、地图素材和示例数据必须确认商业授权。
- 当前开发验证地图源为 `b.tile.openstreetmap.fr/hot` 标准图和 `tile.openmaps.fr/openhikingmap` 地形图，并配置 fallback 瓦片 URL；必须保留 attribution，商业发布前需要替换或签约授权清晰的地图服务。

## 主要目录

```text
lib/
|- main.dart
|- application/
|  |- connection_models.dart
|  |- connection_session_backend.dart
|  |- connection_controller.dart
|  |- demo_connection_session_backend.dart
|  \- mgcore/
|- features/
|  |- frontstage/
|  |- map/
|  \- shell/
|- ui/
|  |- layouts/
|  |- skin/
|  \- skins/
|     \- mac_glass/
\- platform/
```

## 验证命令

```bash
flutter pub get
flutter analyze
flutter test
flutter build macos
```

macOS 当前已验证：

```text
flutter analyze: passed
flutter test: 18 tests passed
flutter build macos: passed
```

Windows 目录已生成，需要在 Windows 机器上执行 `flutter build windows` 验证。

Android/iOS 需要重新启用平台工程后补齐：

- native MgCore library 打包。
- Android USB/串口/文件系统权限和 JNI/AssetManager 上下文。
- iOS 权限、签名和 App Store 合规边界。

## Native library

MgCore FFI 默认按以下方式寻找 native library：

1. `MGCORE_LIBRARY_PATH` 环境变量。
2. 当前工作目录附近的 `mgcore.dylib` / `mgcore.dll`。
3. 仓库后端构建输出附近的 `build/mgcore.dylib` / `build/mgcore.dll`。

如果找不到 native library，应用会进入离线演示后端，便于前端界面和状态管理继续开发。
