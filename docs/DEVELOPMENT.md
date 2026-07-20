# 开发指南

本指南只描述当前源码包可执行的开发流程。项目长期目标平台按优先级为 Windows、Android、macOS、iOS；当前源码包已恢复 C++ 后端、C FFI bridge 和 Flutter macOS/Windows 桌面骨架。

## 日常规则

1. 改动前先确认层级：`core`、`bridge/ffi`、`app`、`tests`、`tools` 或 `docs`。
2. 后端行为变化补 C++ 测试；FFI 契约变化补契约测试和文档。
3. 前端状态和 UI 变化补 Flutter test，至少跑 `flutter analyze`。
4. 安全敏感路径改动后跑默认门禁，必要时跑聚焦测试。
5. 不把构建目录、dump、临时日志、`.DS_Store`、一次性计划放进源码树。
6. 当前目录不是 git 仓库；正式版本管理前应恢复 `.git` 或重新 clone。

## 工具链

macOS 当前可用工具：

```bash
/Users/fansicheng/Library/Python/3.9/bin/cmake --version
/Users/fansicheng/Library/Python/3.9/bin/ninja --version
flutter --version
pod --version
```

如果 CMake/Ninja 已进入系统 PATH，可直接使用 `cmake` 和 `ninja`。

## 后端构建

macOS 已验证命令：

```bash
/Users/fansicheng/Library/Python/3.9/bin/cmake -S . -B build_check -G Ninja \
  -DCMAKE_MAKE_PROGRAM=/Users/fansicheng/Library/Python/3.9/bin/ninja \
  -DMGCORE_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release

/Users/fansicheng/Library/Python/3.9/bin/cmake --build build_check
```

通用命令：

```bash
cmake -S . -B build -DMGCORE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

构建产物：

```text
build/mgcore.dylib
build_check/test_*
```

`build/` 和 `build_check/` 是生成物，已在 `.gitignore` 中。

## 后端测试

默认门禁：

```bash
ctest --test-dir build_check --output-on-failure \
  -L "offline|simulation" \
  -LE "hardware|manual"
```

当前默认门禁基线：

```text
29/29 tests passed
offline: 18
simulation: 11
```

常用聚焦测试：

```bash
ctest --test-dir build_check --output-on-failure \
  -R "test_multi_vehicle|test_mavlink_protocol|test_v2_contract|test_transport_links|test_sim_full_flow"
```

测试标签：

- `offline`：不依赖真实链路。
- `simulation`：内存链路或模拟飞控。
- `hardware`：依赖真实飞控、串口或外部设备。
- `manual`：需要人工确认。

`hardware` 和 `manual` 不进入默认自动化门禁。

## Flutter 前端

当前可执行验证集中在 macOS/Windows desktop；长期验收必须覆盖 Windows、Android、macOS、iOS，其中 Windows 和 Android 是主力平台。

macOS 已验证：

```bash
cd app
flutter pub get
flutter analyze
flutter test
flutter build macos
```

Windows 待验证：

```powershell
cd app
flutter build windows
```

Android/iOS 待恢复：

```bash
cd app
flutter create --platforms=android,ios .
flutter build apk
flutter build ios --no-codesign
```

重新启用移动端后，需要补齐 native MgCore library 打包、权限、文件系统和 USB/串口能力适配。

前端架构必须遵守 ADR 0004：UI 通过 `ConnectionSessionBackend` 和 `ConnectionController` 接入，不能直接调用 MgCore FFI 或复制 MAVLink/参数/任务状态机。

前端 UI 必须遵守 `docs/CONSTRAINTS.md` 和 `docs/FRONTEND_COMPONENTS.md`：一套业务 UI 跨平台复用，Windows/macOS 尽可能同屏一致，最大程度复刻 macOS/iOS 通用风格气质；第三方视觉组件先封装为项目 wrapper，不直接散落进 feature 页面；所有依赖和资产必须确认商业授权，且不得复制 Apple 官方资产或平台专有界面。

## Native library

Flutter 前端当前通过 Dart FFI 尝试加载 MgCore：

1. `MGCORE_LIBRARY_PATH` 环境变量。
2. 当前工作目录附近的 `mgcore.dylib` / `mgcore.dll`。
3. 仓库后端输出附近的 `build/mgcore.dylib` / `build/mgcore.dll`。

找不到 native library 时，前端会退回离线演示后端，便于继续开发 UI。

## 真实硬件只读检查

macOS 可直接运行测试二进制：

```bash
build_check/test_real_fc_dump /dev/cu.usbmodem1101 --quick
build_check/test_scan_connect /dev/cu.usbmodem1101
```

Windows 统一入口：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\run-hardware-test.ps1 -Test dump -Port COM16
```

规则：

- 执行前确认飞控未解锁。
- 明确本次测试会触发哪些命令。
- 只读 dump 可以读取心跳、遥测、参数、任务/围栏状态。
- 不经确认不得执行解锁、起飞、切模式、写参数、任务启动、电机测试或危险校准。
- 默认不保留日志；只有显式要求时才归档。

## 文档和编码

- 面向维护者和操作员的说明默认中文。
- 自有文本目标状态是 UTF-8 + LF。
- 当前仍有 CRLF/BOM 历史遗留；大规模归一化必须单独提交。
- 终端中文显示异常时，先按 `docs/ENCODING.md` 判断，不要把乱码复制回仓库。

## 清理规则

可随时删除并重新生成：

- `build/`
- `build_check/`
- `app/build/`
- `app/.dart_tool/`
- `app/.idea/`
- `.DS_Store`

历史计划归档在 `docs/archive/`，不作为当前实施步骤。
