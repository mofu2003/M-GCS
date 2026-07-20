# 项目状态

更新日期：2026-07-16。

## 当前结论

当前源码包已经恢复为可继续开发的后端 + Flutter 桌面前端工程：

- C++ core 和 C FFI bridge 可在 macOS 上配置、完整编译并通过默认 `offline` / `simulation` 测试。
- Flutter macOS/Windows 桌面前端骨架已恢复，macOS `analyze`、`test`、`build macos` 已通过。
- Flutter 前台已切换为 `MacGlassSkin`：真实地图背景 + 顶部状态栏 + 左侧导航 + 连接抽屉 + 左下姿态仪 + 右侧地图工具栏 + 右下地图切换。
- 当前前端仍是 UI 优先阶段，连接面板只做 Serial / UDP / TCP / 蓝牙布局预览，暂不调用真实连接接口。
- 长期产品目标平台已明确为 Windows、Android、macOS、iOS，其中 Windows 和 Android 是主力验收平台。
- 真实 ArduPilot Copter 飞控只读连接已验证，能读取心跳、姿态、健康、固件、参数和部分实时 servo output。
- 源码目录仍不是 git 仓库，无法使用 `git status` / `git diff` / 子模块状态校验。

## 已验证内容

后端 macOS：

```text
CMake: 4.3.4
Ninja: 1.13.0
Compiler: AppleClang
Build: passed
Default tests: 29/29 passed
Output: build/mgcore.dylib
```

前端 macOS：

```text
flutter analyze: passed
flutter test: 18 tests passed
flutter build macos: passed
Output: app/build/macos/Build/Products/Release/mgcs.app
```

真实飞控只读检查：

```text
Port: /dev/cu.usbmodem1101
Firmware: ArduCopter 4.7.0-dev
System ID: 1
Mode: Stabilize
Armed: false
Parameter count: 1027
Servo output after stream request: [0,0,0,0,1000,1000,0,0,0,0,0,0,0,0,0,0]
```

本次真实飞控检查只执行只读 dump 和参数读取，没有解锁、起飞、切模式、写参数或校准。

## 最近修复

- macOS 后端构建链路已补齐：串口、TCP/UDP、POSIX filesystem、CMake MinGW 条件等。
- UDP 远端地址访问已加锁，避免数据竞争。
- FFI 事件回调 JSON 所有权已明确：回调方使用后调用 `mg_free_string()`。
- Flutter 前端已按 ADR 0004 建立 `ConnectionSessionBackend` 接入边界。
- Flutter 前台已新增 build-time fixed 皮肤契约，默认皮肤为 `MacGlassSkin`，业务 UI 不直接调用 MgCore FFI。
- 前台无任务弹窗已隐藏，当前布局优先展示地图、状态栏、连接入口和遥测仪表区域。
- 前台地图已从占位图切换为 `flutter_map` 真实瓦片地图，当前显示 `OpenStreetMap HOT 标准地图` 和 `OpenHikingMap 地形地图` 完整名称，并配置 fallback 瓦片 URL。
- 地图已接入 `geolocator 14.0.3` 系统定位：每次启动只尝试一次，5 秒超时或权限失败时静默保留默认位置；macOS 已补用途说明与定位 entitlement。
- 连接抽屉已改为 Serial / UDP / TCP / 蓝牙分段页；连接按钮仍不调用未确认的后端接口，界面不再显示开发预览提示。
- 连接抽屉关闭逻辑已修复，未连接状态下也尊重用户关闭面板的操作。
- macOS Debug/Profile/Release entitlements 已补出站网络权限，允许地图瓦片 HTTPS 请求。
- 地图组件已增加自绘兜底背景，公共瓦片服务不可达时不会退成纯空白背景。
- 后端连接识别后会主动请求 `SERVO_OUTPUT_RAW` 和 `RC_CHANNELS`，避免 servo/RC 实时值长期停留默认缓存。
- `mg_vehicle_get_servo_output()` 未收到真实输出时不再误报 `valid=true`。
- 历史 agent 计划已从 `docs/superpowers/` 移到 `docs/archive/agent-plans/`。
- `.DS_Store`、Flutter build 缓存、旧本地代理配置等明确噪音已清理。

## 后端完成度

已形成维护骨架：

- 连接：串口、TCP、UDP、内存链路和连接生命周期管理。
- MAVLink：adapter、发送器、命令队列、参数缓存、任务传输。
- 飞控模型：`Vehicle`、`MultiVehicleManager`、活动飞控选择和多机管理。
- 参数：参数读写、参数文件导入导出、描述符和中文翻译。
- 任务：mission、geofence、rally、survey 相关能力。
- 校准：陀螺仪、加速度计、罗盘、气压计校准流程。
- 遥测：缓存、新鲜度、环形缓冲、ADSB、诊断聚合。
- FFI：领域 API 拆分、JSON envelope、错误码、事件回调。
- 测试：offline / simulation / hardware / manual 标签分层。

## 前端完成度

当前 `app/`：

- 已启用 macOS 和 Windows desktop。
- Android 和 iOS 已确认为长期目标平台，但当前仍需重新启用平台工程并补齐权限、native library 打包和移动端适配。
- `application/` 已有连接会话契约、控制器、模型、MgCore FFI 适配层和离线演示后端。
- `features/frontstage/` 已有前台任务工作台页面、前台 view model 和 controller adapter。
- `features/map/` 已有地图源目录和地图 surface 封装，业务 UI 通过项目接口选择地图样式。
- `ui/skin/` 已有皮肤协议、token、scope 和编译时默认皮肤工厂。
- `ui/skins/mac_glass/` 已有第一版浅色玻璃皮肤，实现真实地图背景和悬浮 UI 骨架。
- 旧连接面板、仪表盘面板和旧 demo 主题文件已移除，避免正式前台与历史 demo 混在一起。
- `test/` 已覆盖控制器、MgCore JSON 解码、皮肤契约、前台布局、连接抽屉、姿态仪和 app smoke test。
- UI 方向改为项目自有跨平台设计系统，硬约束是在 Windows、macOS、Android、iOS 上保持一致页面结构，Windows/macOS 尽可能同屏一致，并最大程度复刻 macOS/iOS 通用风格气质。
- 商业安全是前端同级硬约束：不得复制 Apple 官方资产、SF Symbols、系统截图、专有图标、官方文案、平台专有界面或来源/授权不清晰的素材。

仍需补齐：

- 将 `mgcore.dylib` / `mgcore.dll` 正式打包进桌面应用。
- 在 Windows 机器上执行 `flutter build windows`。
- 在 Android 上重新启用平台工程、验证 USB/串口/文件系统权限和 native library 打包。
- 在 iOS 上重新启用平台工程、验证签名、权限和 native library 打包。
- 将真实连接、参数、遥测、告警、任务等页面逐步接到 UI。
- 让前端显示 servo/RC 实时值、参数总数和飞控健康信息。
- 建立商业授权检查清单，覆盖依赖、字体、图标、图片、地图素材和示例数据。
- 商业发布前替换或签约可商用地图服务，补齐 Key 管理、配额、缓存、attribution 和离线地图策略。

## 仍不是生产完成

- 固件刷写：`FirmwareFlasher` 仍是占位或未验收能力。
- 视频接收/解码：已有元数据骨架，完整链路未生产验收。
- Android filesystem：当前为占位，需要 JNI/AssetManager 上下文。
- Windows 前端构建：目录已生成，但未在 Windows 环境验证。
- Android/iOS 前端构建和 native packaging：尚未恢复验收。
- CI：当前未完整覆盖 macOS + Flutter 桌面构建。
- 仓库：当前目录不是 git 仓库，子模块状态无法确认。
- 编码：仍有 CRLF/BOM 历史遗留，归一化应单独处理。

## 下一阶段建议

1. 恢复完整 git 仓库或重新 clone 后迁移当前改动。
2. 将 macOS 后端和 Flutter macOS 构建纳入 CI。
3. 在 Windows 上验证 `flutter build windows` 和后端硬件脚本。
4. 重新启用 Android 平台工程并验证 Android native library、权限和文件系统适配。
5. 重新启用 iOS 平台工程并验证签名、权限和 native library 打包。
6. 固化 Flutter native library 打包流程。
7. 建立跨平台 UI 设计系统 wrapper、macOS/iOS 风格复刻 token 和商业授权检查清单。
8. 单独做编码/换行归一化。
9. 基于真实飞控数据完善前端连接、参数、遥测和 servo/RC 页面。
