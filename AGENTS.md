# M-GCS 协作规则

当前版本：`0.0.79`

本文件是仓库级工作规则，Codex 等 agent 进入本仓库时会自动读取并遵守。
**本文件是唯一权威源**，`docs/guides/DEVELOPMENT.md` 是简化参考，
废弃的约束文档已清理，约束统一维护在这里。

## 项目简述

无人机地面站，对标 Mission Planner / QGroundControl。依赖方向单向，不可逆向：

```text
flutter_shell/  →  bridge/ffi/  →  core/  →  third_party/
```

- `core/`：C++17 后端核心（transport、mavlink、vehicle、mission、calibration、telemetry、log…）
- `bridge/ffi/`：唯一对外稳定边界，入口 `bridge/ffi/ffi_api.h`，契约清单 `bridge/ffi/API_CATALOG.md`
- `flutter_shell/`：Flutter 外壳（地图、连接、遥测、波形、校准）

## 铁律一：默认纯本地工作，不碰远端

**除非用户明确要求推送，否则不要访问 GitHub 或任何远端。**

- 不要执行 `git fetch`、`git pull`、`git ls-remote`、`git remote show`、`git remote update`
- 不要在未获明确指令时执行 `git push`、推送标签、删除或创建远端分支
- 判断仓库状态只用本地信息：`git status`、`git log`、`git diff`、`git diff --stat`
- 用户要求推送时，一次做完（提交 + 标签 + 推送），不要在中途插入探测性网络调用
- 门禁脚本、git hook、发布脚本内不得包含任何网络调用

原因：本机到 GitHub 的连接不稳定（HTTP2 framing error、空响应、75 秒超时）。
网络重试会浪费大量时间，而且与"验证代码是否正确"无关。

推送前如网络不通，先确认代理再重试；不要盲目循环重试。

## 铁律二：提交前必须跑本地门禁

提交前必须执行，并在失败时修复后重跑：

```bash
# 后端门禁（约 35 秒，34 个 offline/simulation 测试）
ctest --test-dir build_check --output-on-failure --no-tests=error -L "offline|simulation" -LE "hardware|manual"

# 前端门禁
cd flutter_shell && flutter analyze lib test && flutter test
```

`build_check` 不存在时先初始化（首次需要生成 MAVLink 头）：

```bash
./tools/generate-mavlink.sh
cmake -S . -B build_check -DMGCORE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_check --config Release
```

改动涉及界面时追加 `cd flutter_shell && flutter build macos --debug`。

改动涉及 `core/`、`bridge/ffi/` 时，门禁不能用"看起来没问题"代替。

## 铁律三：FFI 契约变更必须成套

修改 `bridge/ffi/ffi_api*.h` 时，同一次提交必须同时包含：

1. 头文件声明
2. 实现（`ffi_*.cc`）
3. 测试（`core/tests/`）
4. `bridge/ffi/API_CATALOG.md`
5. 受影响的文档

契约是前后端并行的前提，缺一项就会让后续工作建立在错误假设上。

## 铁律四：界面字体、间距与对齐统一

前端 `flutter_shell/` 必须遵守，禁止各自发明数值：

- 字号只能取自 `lib/theme/mg_typography.dart` 的 `MgTypography`，
  不允许在页面里写裸 `TextStyle(fontSize: ...)`
- 尺寸只能取自 `MgMetrics`（行高、控件高度、圆角、导航宽、工具栏高）
- 颜色只能取自 `lib/theme/mg_colors.dart` 的 `MgColors`，
  不允许写裸色值，也不允许直接用 `MacosColors.*`（未解析时只取浅色分支）
- 文本与控件默认垂直居中：固定行高 + `CrossAxisAlignment.center`，
  不允许出现顶对齐／底对齐造成的错位
- 数字读数使用 `MgAnimatedNumber`，并保留等宽数字（`tabularFigures`）
- **字体跨平台必须统一，并以 macOS 端的设计为基准**（依据见
  `docs/architecture/adr/0007-unified-font-stack.md`）：
  - 同一段文字在 macOS / Windows / Linux 上必须由同一款字体渲染，
    不允许按平台各挑一套系统字体
  - 字体族、字重、字号、行高的取值一律以 mac 端效果为准，其他平台向它对齐，
    而不是反过来让 mac 端迁就别的平台
  - 字体族只在 `MgFonts` 单点定义；业务页面里不得出现字体族名或平台判断
  - 禁止平台专属字体名（例如 `SF Pro Text`、`Menlo`）。这类名字在未安装的平台上
    会静默回退成完全不相关的字体，且不会报错
  - 字体族总数控制在 1～2 套（西文一套、中文回退一套）。数字读数不单独引入
    等宽族，对齐由 `tabularFigures` 保证
  - 打包字体必须允许商用与随软件分发。优先 OFL、Apache-2.0；商业友好的自定义
    许可也可用，但署名、修改、再分发、可撤销性四类义务与风险必须记录在 ADR 0007
  - **只有标题可以加粗**。页面标题、区块标题、卡片标题、表头之外的一切文字
    （正文、数值、状态、标签、徽标、传感器型号、固件名、机型名）都用
    `FontWeight.w400`；层级由字号与颜色承担，不靠字重堆叠

新增控件必须先考虑复用已有组件，而不是再写一个近似版本。

## 铁律五：硬件是排他资源

飞控串口同一时刻只能被一个进程或 agent 占用。

- 运行硬件相关操作前，确认没有其他会话正连着 `/dev/cu.usbmodem*`
- 硬件测试串行执行，不要并行
- 校准、解锁、电机测试前确认机体安全（拆桨、未解锁）

## 上游实现择优准则：先查 QGC / MP / 成熟依赖

M-GCS 首先服务 ArduPilot，同时保持 PX4 兼容。实现上游已有能力时，不按项目整体
站队，而要按模块比较 MP、QGC、官方协议和成熟第三方库的质量，再选择引用、参考或
模仿对象。

**实现任何连接、MAVLink、任务、参数、校准、日志、地图或平台功能前，必须先查
QGroundControl 和 Mission Planner 的本地源码，以及项目已经采用的第三方库。**

- 先确定该能力的质量归属：ArduPilot 专用命令、参数、校准、固件和飞行行为通常优先
  参考 MP；任务模型、链路生命周期、跨平台结构、状态机完整性和测试治理通常优先
  参考 QGC。具体模块的默认归属见 [`docs/specs/upstream/SELECTION_STRATEGY.md`](docs/specs/upstream/SELECTION_STRATEGY.md)。
- 能直接引用或复用官方 MAVLink、许可证兼容的成熟上游库或 QGC/MP 实现时，优先复用，
  不要自己另写一套。MAVLink 消息、协议流程和生成代码一律复用官方定义。
- 不能直接复用时，也必须以质量更高的上游实现的字段、状态机、错误语义、能力门控
  和交互顺序为参考，再按 M-GCS 的 `flutter_shell -> bridge/ffi -> core -> third_party`
  边界实现。
- QGC 和 MP 冲突时不能凭偏好选择。要记录比较依据；如果两者各有所长，可以明确采用
  “QGC 负责结构，MP 负责 ArduPilot 语义”的组合，但必须说明边界。
- 不允许在没有查过 QGC / MP 对应模块、也没有复用成熟依赖的情况下，凭个人习惯从零设计。
- UI 只借工作流、状态和信息层级，不照搬 QGC/MP 的 Qt/QML/WinForms 布局或控件。
- 当前 QGC checkout 采用 Apache-2.0 / GPLv3 双许可，MP 为 GPLv3，M-GCS 当前是 MIT。
  从 QGC/MP 直接移植大段源码前必须核对所选许可证、Qt 商业授权和再分发义务；优先复用的是
  协议事实、算法、字段、流程和允许再分发的第三方库。
- 设计说明、提交说明或代码注释中要能指出所参考的 QGC / MP 模块、选择理由或引用的
  上游库。

## 任务文件格式策略

- `MissionDocument` JSON 是唯一内部主状态；`.waypoints` 和 `.plan` 都是交换格式。
- ArduPilot / Mission Planner 工作流默认推荐 `.waypoints`。
- PX4 / QGroundControl 工作流默认推荐 `.plan`。
- `.waypoints` 不能无损表达的 Survey、Corridor、GeoFence、Rally 等复杂任务，必须推荐 `.plan` 或明确警告。
- `.plan` 不删除；它承担 PX4/QGC 兼容和复杂任务保留。
- 不得根据格式或飞控静默覆盖用户文件；上传飞控始终从 `MissionDocument` 编译 `MissionPlan`。

## 构建与运行

```bash
# 后端共享库
cmake -S . -B build -DMGCORE_BUILD_SHARED=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target mgcore

# 前端运行（本地热重载）
cd flutter_shell && flutter run -d macos

# Docker 全量检查（可选）
./tools/docker-dev.sh check
```

产物位置：macOS `build/mgcore.dylib`、Windows `build/mgcore.dll`、Linux `build/libmgcore.so`。

## 禁区

- 不提交 `build*/`、`.venv/`、`build_docker/` 等本地生成物
- 不提交生成后的 MAVLink C 头（用 `tools/generate-mavlink.sh` 本地生成）
- 不修改 `third_party/` 中的第三方源码
- 不使用 `git reset --hard`、`git checkout --` 丢弃他人改动
- 不 force push
- 不在 `core/` 的持锁路径中做阻塞 I/O 或调用外部回调

## 多 agent 协作

- 每个 agent 使用独立 worktree 与分支，避免同一工作区并发编辑同一文件
- 任务卡写清四件事：涉及文件、依赖契约、验收命令、完成标准
- 独占资源清单：飞控串口、Flutter 热重载窗口、git 索引
- 完成后由另一个 agent 或人跑同一验收命令复核，不要由实现者自证
- 日常操作流程、任务卡模板和本地脚本见 `docs/COLLABORATION.md`

## 提交与发布

- 提交信息格式 `<type>: <summary>`，type 取 `feat` / `fix` / `docs` / `chore` / `release`
- 提交前跑 `git diff --check`，确认没有空白字符问题
- 发布需要同步四处版本号：
  `CMakeLists.txt`、`flutter_shell/pubspec.yaml`、`README.md`、`CHANGELOG.md`
- 发布流程：改版本号 → 更新 CHANGELOG → 本地提交 → 本地打标签 →（用户要求时）推送
- 本地标签不推送不影响开发，推送是独立步骤

## 架构约束

### 依赖方向不可逆

```text
flutter_shell/  →  bridge/ffi/  →  core/  →  third_party/
```

- `core/` 不得 `#include` bridge 或 flutter_shell 的头
- `bridge/ffi/` 只能调用 `core/` 的公开接口
- 循环依赖在编译时会被拒绝

### 安全设计原则

- **超时必须报错**：网络调用、串口读写、任务上传设置合理超时，超时后返回错误，不能无限挂起
- **拒绝必须传播**：飞控拒绝命令时，错误必须返回给调用方，不能被吞掉或转换成"成功"
- **断链必须通知**：连接断开时，所有依赖该连接的操作必须收到通知，不能继续假装连通
- **用户确认门控**：解锁、起飞、降落、紧急停止、任务启动必须用户确认，不得静默执行
- **飞行模式切换确认机制**：切换飞行模式需要按住按钮 1 秒确认，避免误触

### FFI 契约稳定性

- `bridge/ffi/ffi_api.h` 是唯一稳定边界，改动需要同步：头文件、实现、测试、API_CATALOG.md、文档
- 所有 `char*` 返回值由调用方通过 `mg_free_string()` 释放
- 不删除已导出的函数；弃用需要文档化的 ABI-major 发布和迁移说明
- 306 个导出函数中，89 个前端使用，220 个测试覆盖，83 个尚未被前端或测试消费（logging/geofence/config 等领域）

### UI 设计约束

- **设计系统单点定义**：所有字号、颜色、间距、圆角必须取自 `MgTypography`、`MgColors`、`MgMetrics`
- **字体跨平台统一**：MiSans 全平台一致，以 macOS 为基准，禁止平台专属字体
- **只有标题加粗**：正文、数值、状态标签等一律使用 `FontWeight.w400`，层级由字号和颜色区分
- **控件复用优先**：新增控件前先检查是否可以复用已有组件

### 许可证约束

- M-GCS 使用 MIT 许可证
- 当前 QGC checkout 为 Apache-2.0 / GPLv3 双许可，MP 为 GPLv3；大段源码不能直接复制
- 采用 QGC Apache-2.0 分支时仍需核对 Qt 商业授权、署名和再分发义务
- 可复用：协议事实、算法、字段、流程、允许再分发的第三方库
- 打包字体必须允许商用与随软件分发（OFL / Apache-2.0 优先）

## 项目记忆

`.workbuddy/memory/MEMORY.md` 是项目的长期记忆，记录架构、关键约定、构建命令与文档索引。
开始一个不熟悉的子任务前，先读它；获得新的稳定结论后，更新它。
