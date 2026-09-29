# M-GCS 协作规则

本文件是仓库级工作规则，Codex 等 agent 进入本仓库时会自动读取并遵守。
改动本文件前先确认它与 `docs/DEVELOPMENT.md`、`docs/CONSTRAINTS.md` 一致。

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
# 后端门禁（约 35 秒，31 个 offline/simulation 测试）
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
  `docs/adr/0007-unified-font-stack.md`）：
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

## 项目记忆

`.workbuddy/memory/MEMORY.md` 是项目的长期记忆，记录架构、关键约定、构建命令与文档索引。
开始一个不熟悉的子任务前，先读它；获得新的稳定结论后，更新它。
