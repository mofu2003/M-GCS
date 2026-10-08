# Flutter Shell Changelog

This file tracks changes specific to the Flutter presentation layer (`flutter_shell/`).
For full project history, see the root `CHANGELOG.md`.

## 0.0.79 - 2026-10-08

### Changed

- 将 4,099 行的配置工作区拆成 `lib/configuration/` 下 35 个独立 Dart 文件；
  `lib/configuration_workspace.dart` 保留为兼容导出。
- 九个配置页面按领域分层，校准、PID、参数控件、提示和数据卡片拆为可复用组件。
- 配置间距统一使用 `MgMetrics.config*`，Flutter 结构整理为 `components/`、
  `configuration/` 和 `demo/`。
- 删除已完成的一次性配置工作区拆分指南和过期文档重定向。

### Testing

- `flutter analyze`: 无问题
- `flutter test`: 79/79 通过
- `flutter build macos --debug`: 通过

## 0.0.78 - 2026-10-08

### Added

- 顶部状态栏统一的悬浮长按确认条：0.5 秒白底到蓝色进度填充，填满后出现对勾并
  下发命令；"移动到此处"、返航、开始任务、解锁/上锁共用该流程
- 右下角 MacGlass 复合姿态/指南针仪表：固定在圆形玻璃盘内，中央为姿态球，外圈
  为航向刻度，顶部固定航向箭头，底部胶囊显示中文方向和三位航向
- 飞控消息中文翻译：覆盖常见 ArduPilot 预解锁、EKF/GPS、电池、模式切换等状态
  文本；未知消息保留原文

### Changed

- 飞行模式切换和顶部危险命令统一为 0.5 秒长按确认，移除重复的确认对话框
- 所有弹窗改为纯淡入，移除 macos_ui 默认的缩放回弹跳动
- 位置、姿态、VFR HUD 遥测流和前端轮询提高到 20 Hz，地图和仪表刷新更平滑
- 飞控消息时间戳固定 72 px 宽度并右对齐，正文独立列对齐
- 右侧连接、飞控消息和任务规划面板共享右上槽位，同一时间只显示一个

### Fixed

- QGC/ArduPilot 任务坐标帧兼容：`MAV_FRAME_MISSION` 的 `x/y` 按 `param5/param6`
  语义处理，不再把 DO_/CONDITION_ 等非定位命令回读成 `(0,0)` 假航点
- 修复任务文档在 Home 缺失时从飞控 HOME_POSITION 读取后仍重复要求手工设置 Home
  的前端体验

### Testing

- `flutter analyze`: 无问题
- `flutter test`: 69/69 通过
- `flutter build macos --debug`: 通过

## 0.0.73 - Previous Release

Initial tracked version. Earlier changes were part of the unified project changelog.
