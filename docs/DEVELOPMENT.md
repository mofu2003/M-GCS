# 开发指南

## 日常流程

1. 先确认改动属于 `core`、`bridge/ffi`、`app`、`tests`、`tools` 或 `docs`。
2. 后端行为变化补 C++ 测试；FFI 契约变化同时更新头文件、绑定、测试和文档。
3. 前端状态或 UI 变化至少运行 `flutter analyze` 和相关 Flutter 测试。
4. 连接、任务、参数、校准或命令路径变化必须运行聚焦测试和默认门禁。
5. 提交前使用 `git status` 和 `git diff --check` 检查范围与格式。

参数文件导入与批量写入的变更必须覆盖后端参数变更集测试：去重、未知参数、未变化参数、`_ENABLE` 最后写入和逐项失败结果均属于 FFI 契约，不得重新放进某个前端 adapter。

## Mission CAD 开发

Mission CAD 的文档模型、坐标换算、几何、约束、编译、任务估算和诊断属于 `core/mission/cad`。任何新前端只能通过版本化 FFI 契约提交语义化编辑请求和消费结果，不得在 UI 内计算权威航线或 MAVLink 任务项。

修改 Mission CAD 时至少应补齐以下离线测试：

1. `.mgcad.json` 的解析和序列化往返测试。
2. 固定输入文档与 profile 的确定性编译 fixture 测试。
3. 约束或几何错误产生稳定诊断、实体引用和严重级别的测试。
4. CAD 源实体到生成 `MissionPlan` 项的映射测试。

CAD 的创建、编辑、校验和编译不得接触真实飞控。只有把已编译的 `MissionPlan` 交给既有任务传输后，才进入硬件操作安全边界。完整设计见 [Mission CAD 初步设计](MISSION_CAD.md)。

## 后端构建

仓库包含 MAVLink XML 定义，但没有提交生成后的 C 头。首次构建前创建本地 Python 环境，安装已验证的 `pymavlink 2.4.49`，并生成 `common` dialect：

```bash
python3 -m venv .venv
.venv/bin/python -m pip install "pymavlink==2.4.49"
.venv/bin/mavgen.py \
  --no-validate \
  --lang=C \
  --wire-protocol=2.0 \
  --output=third_party/mavlink/include/mavlink/v2.0 \
  third_party/mavlink/message_definitions/v1.0/common.xml
```

`--no-validate` 是必需参数：当前 XML 使用了新版枚举标记，`pymavlink 2.4.49` 的严格校验会拒绝它。生成目录和 `.venv/` 均为本地生成物，不提交。

生成完成后执行：

```bash
cmake -S . -B build_check \
  -DMGCORE_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build_check --config Release
```

需要指定 Ninja 时：

```bash
cmake -S . -B build_check -G Ninja \
  -DMGCORE_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release
```

默认门禁：

```bash
ctest --test-dir build_check --output-on-failure \
  -L "offline|simulation" \
  -LE "hardware|manual"
```

测试标签：

- `offline`：不依赖真实链路。
- `simulation`：使用内存链路或模拟飞控。
- `hardware`：依赖真实串口、飞控或外部设备。
- `manual`：需要人工确认或存在更高操作风险。

`hardware` 和 `manual` 默认不会注册到 CTest。需要由操作者显式允许其
进入 CTest 时，在配置命令中追加
`-DMGCORE_REGISTER_HARDWARE_TESTS=ON`；日常硬件操作仍优先使用专用脚本或直接运行对应可执行文件。

## Flutter

macOS 已验证 Flutter `3.44.7`（Dart `3.12.2`）、Ninja `1.13.2` 和 CocoaPods `1.17.0`。Flutter SDK 位于 `~/development/flutter`，其 `bin` 已加入 `~/.zshrc`。

当前网络使用以下镜像，已写入 `~/.zprofile`。打开新终端后自动生效；当前终端可先执行 `source ~/.zprofile`。为避免 Flutter 访问 GitHub 版本检查超时，以下命令使用 `--no-version-check`。

```bash
source ~/.zprofile
cd app
flutter --no-version-check pub get
flutter --no-version-check analyze
flutter --no-version-check test
flutter --no-version-check build macos
```

如果出现 `Invalid request method, uri=//pub.dev:443`，说明终端继承了格式错误的代理变量。清除该次命令的代理后重试：

```bash
env -u HTTP_PROXY -u HTTPS_PROXY -u ALL_PROXY \
  -u http_proxy -u https_proxy -u all_proxy \
  flutter --no-version-check pub get
```

完整 macOS 构建要求系统选择完整 Xcode，而不是 Command Line Tools。首次配置执行：

```bash
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
sudo xcodebuild -runFirstLaunch
flutter doctor -v
```

如果完整 Xcode 已安装但暂时不能切换全局开发目录，可只为本次构建指定它：

```bash
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer \
  flutter --no-version-check build macos
```

Windows 构建在 Windows 环境执行：

```powershell
cd app
flutter build windows
```

一键构建会先编译 MgCore，再生成桌面应用并嵌入 native library：

```bash
./tools/build-desktop-macos.sh
```

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-desktop-windows.ps1
```

脚本从 `PATH` 查找 Flutter，也可以通过 `FLUTTER_BIN` 指向 Flutter 可执行文件；
通过 `MGCORE_BUILD_DIR` 覆盖后端 CMake 构建目录。

Android/iOS 尚未启用。恢复平台工程时，平台目录只承载权限、打包、签名和 native adapter，不复制业务页面。

## Native library

Dart FFI 依次检查：

1. `MGCORE_LIBRARY_PATH`。
2. 应用包或可执行文件附近的库。
3. 当前目录附近的 `build/mgcore.dylib` 或 `build/mgcore.dll`。

找不到库时应用进入不可用状态，不会自动使用演示数据。`DemoConnectionSessionBackend` 只用于测试。

## ArduPilot 参数兼容

- ArduPilot Copter 和 Plane `4.6.0`（2025-05-21）到 `4.6.3` 使用 `ARMING_CHECK`：值 `1` 表示全部解锁检查，其他位表示单项检查。
- 后续 ArduPilot 开发线将其迁移为 `ARMING_SKIPCHK`：值 `0` 表示不跳过任何检查，位值表示跳过对应检查。
- 安全设置、枚举和位掩码控件必须以飞控实际下发的参数名和描述元数据决定读写语义；缺少参数或元数据时仅显示不可用状态，禁止猜测参数名或写入替代值。

## 真实硬件

只读检查示例：

```bash
build_check/test_scan_connect /dev/cu.usbmodem1101
build_check/test_real_fc_dump /dev/cu.usbmodem1101 --quick
```

Windows 入口：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\run-hardware-test.ps1 \
  -Test dump -Port COM16
```

执行前必须确认飞控未解锁并说明测试发送的命令。未经明确确认，不得执行解锁、起飞、切模式、写参数、任务启动、电机测试或危险校准。

## 第三方依赖

`third_party/mavlink` 当前 vendored 了 XML 定义和部分上游源码，但没有 `pymavlink` 工具或 C 生成头；`third_party/cjson` 直接以源码编译。更新时必须记录上游版本、许可证和生成物变化，并运行完整后端门禁。

## 生成物

以下目录和文件不提交：

- `build/`、`build_check/`、`app/build/`、`app/.dart_tool/`。
- IDE 状态、系统缓存、测试 dump、硬件日志和临时报告。
- 一次性计划、agent 过程记录和已经完成的迁移清单。
