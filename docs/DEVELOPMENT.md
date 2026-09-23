# 开发指南

## 日常流程

1. 先确认改动属于 `core`、`bridge/ffi`、`tests`、`tools`、`docker` 或 `docs`。
2. 后端行为变化补 C++ 测试；FFI 契约变化同时更新头文件、实现、测试、`API_CATALOG.md` 和文档。
3. 连接、任务、参数、校准或命令路径变化必须运行聚焦测试和默认门禁。
4. 提交前使用 `git status` 和 `git diff --check` 检查范围与格式。

参数文件导入与批量写入的变更必须覆盖后端参数变更集测试：去重、未知参数、未变化参数、`_ENABLE` 最后写入和逐项失败结果均属于 FFI 契约。

## Mission CAD 开发

Mission CAD 的文档模型、坐标换算、几何、约束、编译、任务估算和诊断属于 `core/mission/cad`。任何前端只能通过版本化 FFI 契约提交语义化编辑请求和消费结果，不得在调用方内计算权威航线或 MAVLink 任务项。

修改 Mission CAD 时至少应补齐以下离线测试：

1. `.mgcad.json` 的解析和序列化往返测试。
2. 固定输入文档与 profile 的确定性编译 fixture 测试。
3. 约束或几何错误产生稳定诊断、实体引用和严重级别的测试。
4. CAD 源实体到生成 `MissionPlan` 项的映射测试。

CAD 的创建、编辑、校验和编译不得接触真实飞控。只有把已编译的 `MissionPlan` 交给既有任务传输后，才进入硬件操作安全边界。完整设计见 [Mission CAD 初步设计](MISSION_CAD.md)。

## 构建

### Docker 开发环境

Docker 环境统一 Linux 下的 C++ 后端构建、MAVLink 头生成和默认门禁。源码通过卷挂载；Python/MAVLink 虚拟环境使用 Docker named volume，避免在宿主机安装这些依赖。`build_docker/` 是被忽略的本地生成物。

```bash
./tools/docker-dev.sh check
```

常用命令：

```bash
./tools/docker-dev.sh shell        # 进入开发容器
./tools/docker-dev.sh configure    # 生成 MAVLink 头并配置 CMake
./tools/docker-dev.sh build        # 编译 MgCore
./tools/docker-dev.sh test         # 后端 offline/simulation 门禁
```

Windows 安装 Docker Desktop 后，在 PowerShell 中执行等价命令：

```powershell
.\tools\docker-dev.ps1 check
```

容器不能替代 Windows 或 macOS 原生构建；后端在两个平台都使用同一套 CMake 配置。

### 本地构建

仓库包含 MAVLink XML 定义，但没有提交生成后的 C 头。首次构建前创建本地 Python 环境，安装已验证的 `pymavlink 2.4.49`，并生成 `common` dialect：

```bash
./tools/generate-mavlink.sh
```

```powershell
.\tools\generate-mavlink.ps1
```

脚本固定使用 `pymavlink 2.4.49` 和 `--no-validate`；当前 XML 使用了新版枚举标记，旧版严格校验会拒绝它。生成目录和 `.venv/` 均为本地生成物，不提交。Docker 使用独立 named volume 保存生成头，不会污染宿主源码树。

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

## 测试标签

- `offline`：不依赖真实链路。
- `simulation`：使用内存链路或模拟飞控。
- `hardware`：依赖真实串口、飞控或外部设备。
- `manual`：需要人工确认或存在更高操作风险。

`hardware` 和 `manual` 默认不会注册到 CTest。需要由操作者显式允许其进入 CTest 时，在配置命令中追加 `-DMGCORE_REGISTER_HARDWARE_TESTS=ON`；日常硬件操作仍优先使用专用脚本或直接运行对应可执行文件。

## 共享库产物

外部前端通过 `mgcore` 共享库调用 FFI：

- macOS：`build/mgcore.dylib`
- Windows：`build/mgcore.dll`
- Linux：`build/libmgcore.so`

构建共享库：

```bash
cmake -S . -B build -DMGCORE_BUILD_SHARED=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target mgcore
```

### macOS 工具链注意

macOS 完整构建要求系统选择完整 Xcode，而不是 Command Line Tools：

```bash
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
sudo xcodebuild -runFirstLaunch
```

如果完整 Xcode 已安装但暂时不能切换全局开发目录，可只为本次构建指定它：

```bash
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer \
  cmake -S . -B build -DMGCORE_BUILD_SHARED=ON -DCMAKE_BUILD_TYPE=Release
```

## ArduPilot 参数兼容

- ArduPilot Copter 和 Plane `4.6.0`（2025-05-21）到 `4.6.3` 使用 `ARMING_CHECK`：值 `1` 表示全部解锁检查，其他位表示单项检查。
- 后续 ArduPilot 开发线将其迁移为 `ARMING_SKIPCHK`：值 `0` 表示不跳过任何检查，位值表示跳过对应检查。
- 安全设置、枚举和位掩码语义必须以飞控实际下发的参数名和描述元数据决定读写语义；缺少参数或元数据时仅返回不可用状态，禁止猜测参数名或写入替代值。

## 真实硬件

只读检查示例：

```bash
build_check/test_scan_connect /dev/cu.usbmodem1101
build_check/test_real_fc_dump /dev/cu.usbmodem1101 --quick
```

Windows 入口：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\run-hardware-test.ps1 `
  -Test dump -Port COM16
```

执行前必须确认飞控未解锁并说明测试发送的命令。未经明确确认，不得执行解锁、起飞、切模式、写参数、任务启动、电机测试或危险校准。

## 第三方依赖

`third_party/mavlink` 当前 vendored 了 XML 定义和部分上游源码，但没有 `pymavlink` 工具或 C 生成头；`third_party/cjson` 直接以源码编译。更新时必须记录上游版本、许可证和生成物变化，并运行完整后端门禁。

## 生成物

以下目录和文件不提交：

- `build/`、`build_check/`、`build_docker/` 及同类构建目录。
- IDE 状态、系统缓存、测试 dump、硬件日志和临时报告。
- 一次性计划、agent 过程记录和已经完成的迁移清单。
