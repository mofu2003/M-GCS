# M-GCS Flutter 前端

`app/` 是 M-GCS 的 Flutter 前端。当前启用 macOS 和 Windows 桌面平台，Android 和 iOS 尚未恢复。

当前版本：`0.0.3`

## 接入状态

- `ConnectionSessionBackend` 是 UI 与后端之间的应用契约。
- `ConnectionController` 管理连接、参数启动同步、遥测轮询和断连状态。
- 正式入口会加载 MgCore native library；加载失败时显示明确的不可用状态，不会伪装成演示飞控。
- 串口、TCP、UDP 连接已经接入真实 FFI；蓝牙仍只保留 UI 入口。
- 参数后台支持读取描述、查看飞控参数和通过 MgCore 写入参数。
- 机架类型后台支持 Copter 的 `FRAME_CLASS` / `FRAME_TYPE` 和 Plane/QuadPlane 的 `Q_FRAME_CLASS` / `Q_FRAME_TYPE`。
- 串口、TCP、UDP 和蓝牙入口共用固定规格的连接控件，避免不同传输方式之间的布局漂移。
- 测试使用 `DemoConnectionSessionBackend` 注入确定性数据，它不是正式运行时回退路径。

## 目录

```text
lib/
|- main.dart                 应用入口与 native backend 创建
|- application/             应用契约、控制器、模型和 MgCore FFI 适配
|- features/backstage/      参数等后台页面
|- features/frontstage/     地图工作台和前台交互
|- features/map/            地图源、定位和地图 surface
|- features/shell/          应用根 shell
|- ui/layouts/              稳定布局入口
|- ui/skin/                 皮肤契约和 token
\- ui/skins/mac_glass/      当前默认皮肤
```

## Native library

MgCore 按以下顺序查找：

1. `MGCORE_LIBRARY_PATH` 环境变量。
2. 应用包或可执行文件附近的 `mgcore.dylib` / `mgcore.dll` / `mgcore.so`。
3. 当前目录附近的后端 `build/` 输出。

找不到库或缺少所需符号时，应用进入 `MgCore 不可用` 状态。macOS Xcode phase 与 Windows CMake install 会在桌面构建时嵌入 native library。GitHub Actions 会在 Windows runner 上构建 Release 桌面包并检查 `mgcs.exe` 与 `mgcore.dll`；真实串口和飞控行为仍需在 Windows 机器上单独验收。

## 验证

```bash
source ~/.zprofile
flutter --no-version-check pub get
flutter --no-version-check analyze
flutter --no-version-check test
flutter --no-version-check build macos
```

Windows Release 包由 CI 在 Windows runner 上持续验证。重新启用 Android/iOS 后，还需要补齐 native library 打包、权限、文件系统、USB/串口和签名适配。

地图、皮肤和商业授权约束见 [前端组件说明](../docs/FRONTEND_COMPONENTS.md) 与 [开发约束](../docs/CONSTRAINTS.md)。
