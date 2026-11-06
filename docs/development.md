# 开发指南

## 开始开发

先阅读与当前改动直接相关的代码和测试。协作规则见根目录 `AGENTS.md`。

---

## 环境

- CMake 3.20 及以上，C++17 编译器
- Flutter SDK；Dart 版本必须满足 `flutter_shell/pubspec.yaml` 的约束，依赖版本由 `pubspec.lock` 固定
- Python 3，用于生成 MAVLink 头文件（脚本会在 `.venv` 中安装 pymavlink）
- 构建 macOS 应用需要 Xcode

## 生成 MAVLink 头文件

首次构建前执行：

```bash
./tools/generate-mavlink.sh
```

Windows 使用 `tools/generate-mavlink.ps1`。输出位于 `third_party/mavlink/include/`，已被 git 忽略。

## 构建共享库

旧产物可以删除 `build/`、`build_check/` 与 `flutter_shell/build/` 后重建。Flutter 缓存由 `flutter clean` 清理；保留 `.venv/` 和已生成的 MAVLink 头文件可避免重复获取依赖。

```bash
cmake -S . -B build -DMGCORE_BUILD_SHARED=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target mgcore --parallel 8
```

产物位置：macOS 为 `build/libmgcore.dylib`，Windows 为 `build/mgcore.dll`，Linux 为 `build/libmgcore.so`。

`CMakeLists.txt` 把 `mgcore` 共享目标的输出目录固定为仓库根 `build/`，所有构建树共用同一个文件。
因此**不要**在 `build_asan`/`build_tsan` 里开启 `MGCORE_BUILD_SHARED`：那会用带 sanitizer 运行时的库覆盖
Release 产物，随后 Flutter 内嵌或 preflight 的 FFI 符号核对会加载到错误的库。`tools/sanitize.sh` 已按此约定
固定为 `MGCORE_BUILD_SHARED=OFF`；preflight 在符号核对前也会检测并重新链接被污染或过期的产物。

## 运行前端

```bash
cd flutter_shell && flutter run -d macos
```

### 非交互式热重载（可选）

后台/无控制终端运行时，`flutter run` 不安装 raw-mode 按键处理，`r`（热重载）与 `R`（热重启）
不可达。`tools/flutter-pty-run.py` 用 pty 让 flutter 独占控制终端，并把按键通过 UNIX 套接字送入：

```bash
python3 tools/flutter-pty-run.py --workdir flutter_shell -- flutter run -d macos --debug
python3 tools/flutter-pty-run.py --send r    # 热重载
python3 tools/flutter-pty-run.py --send R    # 热重启
python3 tools/flutter-pty-run.py --send q    # 退出
```

输出同时镜像到日志文件（默认 `/tmp/mgcs-flutter-run.log`），控制套接字默认
`/tmp/mgcs-flutter-ctl.sock`。该脚本只用于本机开发，不参与构建产物，也不属于门禁。

### 启用本地视频解码（可选）

默认构建是 `MGCORE_HAS_FFMPEG=OFF`，相机与云台页会诚实显示「本平台未启用本地视频解码」，也不会画录制控件。
要在本机开发构建里启用本地视频预览、录制和截图，把 macOS 应用的 mgcore 构建缓存指向自建的 LGPL FFmpeg
（`--disable-gpl`；GPL 构建不得链接）：

```bash
cmake -S . -B flutter_shell/build/mgcore -DCMAKE_BUILD_TYPE=Release \
  -DMGCORE_BUILD_SHARED=ON -DMGCORE_BUILD_TESTS=OFF \
  -DMGCORE_HAS_FFMPEG=ON -DFFMPEG_ROOT=/Users/fansicheng/.local/ffmpeg-lgpl
cd flutter_shell && flutter build macos --debug
```

Xcode 内嵌阶段会复用这个缓存，产出的 `libmgcore.dylib` 仍然动态链接上述前缀下的 FFmpeg（本机绝对路径，
仅限开发构建）。要回到默认构建，删除 `flutter_shell/build/mgcore` 后重新构建。发布打包需要单独处理 FFmpeg 的分发。

## 后端测试

测试源码位于 `core/tests/`，通过 CTest 标签分组：

- `offline`：不依赖外部资源
- `simulation`：使用回环和模拟飞控
- `hardware`、`manual`：需要真实飞控，默认不运行

```bash
cmake -S . -B build_check -DMGCORE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_check --parallel 8
ctest --test-dir build_check -L "offline|simulation" -LE "hardware|manual"
```

### 编译并发

显式设置 `--parallel 8` 或适合主机内存的更小值，不使用无限并发。`preflight.sh` 自动选择上限，可用 `MGCORE_BUILD_JOBS` 覆盖。MAVLink 头文件较大，过量并发会引发内存压力；历史耗时不能作为其他主机的保证。

### Sanitizer 复测（改到锁、回调、线程或传输层后）

```bash
tools/sanitize.sh              # 依次跑 ASan 与 TSan
tools/sanitize.sh --tsan-only  # 只跑其中一个
```

脚本在 `build_asan`/`build_tsan` 下以 Debug 重新编译全部测试，并运行
`ctest -L "offline|simulation" -LE "hardware|manual"`。只应在触及锁、回调、线程或传输层的改动后运行：
sanitizer 需要重编所有测试，比 Release 门禁慢得多。macOS 的 ASan 不支持 `detect_leaks`，脚本不设置
`ASAN_OPTIONS`。

### SITL 端到端任务场景

需要本地可运行的 ArduPilot SITL 构建：

```bash
tools/sitl-mission-scenario.sh                                   # arducopter --model quad
tools/sitl-mission-scenario.sh --vehicle arduplane --model plane \
    --port 14551 --evidence docs/sitl-scenario-evidence-plane.md
```

脚本拉起 SITL（`--serial0 udpclient:127.0.0.1:<port>`，不是 `sim_vehicle.py` 的
`--out`），运行 `build_check/test_sitl_mission_scenario`，并把断言与 `[EVIDENCE]`
行写成 `build_check/sitl-scenario-evidence.md`（默认写构建目录，证据是运行产物、
不落进仓库；要发布时用 `--evidence docs/...` 显式指定）。没设 `MG_SITL_ENDPOINT` 时驱动打印
`[SKIP]` 并以 0 退出，所以它不属于 preflight。用 `--ardupilot <dir>` 或
`ARDUPILOT_DIR` 指定 ArduPilot 源码/构建目录。

## 门禁

提交前执行：

```bash
tools/preflight.sh
```

它检查补丁格式、版本一致性、Flutter 分析与测试、后端构建与测试，以及可用时的 macOS debug 构建。它不再把历史文档、UI 风格、FFI 目录或静态审计台账当作普通改动的门禁。

可用选项：

- `--frontend-only`：只跑前端相关检查
- `--backend-only`：只跑后端相关检查
- `--no-macos-build`：跳过 macOS 构建
- `--changed`（或 `--changed=<ref>`）：按改动挑最小范围，默认对比 `HEAD`（含未提交与未跟踪文件）

### 开发时用 `--changed`，提交前跑全量

测试数量和耗时随代码与主机变化，以当次门禁输出为准。共享头文件修改可能触发多模块重编译，定向测试仅适用于工具已分类的局部改动。

```bash
tools/preflight.sh --changed              # 内环：只跑改动相关的部分
tools/preflight.sh --no-macos-build       # 提交前：全量
```

`--changed` 的取舍规则：

| 改动位置 | 跑什么 |
|---|---|
| `docs/**`、`*.md` | 只跑补丁格式与版本检查（约 0.1 s） |
| `flutter_shell/**` | analyze + 全部 Flutter 测试（约 11 s），跳过后端 |
| 单个 `core/tests/test_<名字>.cc` | 只编、只跑该测试 |
| `core/**/calibration/**` | `test_sim_calibration`、`test_state_machines` |
| `core/**/mission/**` | 任务相关几套 |
| **`core/**/mavlink/**`、`bridge/ffi/**`、共享测试夹具、CMake、未分类路径** | **升级为后端全量并打印原因** |

最后一行的存在理由：涉及 MAVLink adapter、FFI 上下文、共享夹具或 CMake 的改动可能跨多个测试目标，工具会明确打印升级原因。测试数量随代码变化，不在这里固定记录。

`--changed` 是内环工具。**提交前请仍跑一次不带 `--changed` 的全量**，它是唯一能覆盖全部组合的检查。

启用提交钩子：

```bash
tools/install-hooks.sh
```

钩子要求相关修改全部暂存且没有未跟踪文件，再运行 `preflight.sh`；已暂存的修改不要求先提交。

### 维护原则

保持实现简单，避免为假设中的未来增加层和接口。只有出现真实使用方、真实错误或
明确的架构变化时，才增加抽象、测试或文档。

## 前后端接口对齐

```bash
(cd flutter_shell && MGCS_FLEET_LIBRARY="$PWD/../build/libmgcore.dylib" flutter test --no-pub test/fleet_ffi_smoke_test.dart)
```

Linux 的库路径为 `build/libmgcore.so`，Windows 为 `build/mgcore.dll`。FFI smoke 使用本地
UDP 模拟链路覆盖生产 backend，不需要实体飞控；没有本地共享库时跳过。

## Docker（仅后端）

```bash
tools/docker-dev.sh check
```

## 发布

发布时只执行实际需要的版本和硬件检查：

1. `tools/release.sh <x.y.z>` 同步版本号。
2. 更新 `CHANGELOG.md`。
3. 本地提交并打标签。
4. 用户明确要求时才推送。
