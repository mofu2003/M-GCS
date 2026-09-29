# 协作流程

本文面向在 M-GCS 中并行工作的人和 agent。`AGENTS.md` 是不可违反的硬规则；
本文负责把规则落成可执行的日常流程。构建和测试命令以
[开发指南](DEVELOPMENT.md) 为准。

## 基本原则

1. 默认只使用本地信息和本地工具；除用户明确要求外，不访问远端。
2. 每个并行任务使用独立 worktree 和分支，避免共享 git 索引与工作目录。
3. 同一文件同时只由一个工作流负责修改；跨目录任务先明确所有权再开工。
4. 任务开始前写清范围、契约、独占资源和验收命令；完成后由另一个执行者复核。
5. FFI、UI token 和硬件操作分别遵守 `AGENTS.md` 的铁律三、四、五。

## 任务卡

每项任务在聊天、任务线程或提交说明中至少记录以下内容：

```text
目标：
涉及文件 / 目录：
依赖契约：
不修改范围：
独占资源：
验收命令：
完成标准：
```

`.workbuddy/` 是本地记忆，不纳入版本控制，不能作为多人之间唯一的任务登记处。
跨执行者交接时，任务卡内容必须随消息或提交记录一起传递。

## 分支与工作区

- 分支默认使用 `codex/<short-task-name>`，名称描述工作内容，不使用个人姓名。
- 一个 worktree 只服务一个进行中的任务。
- 开始前检查 `git status`，确认没有把他人未提交改动带入任务。
- 不删除、覆盖或重置其他 worktree 的改动；需要清理时先确认没有流程仍在使用。
- 完成后保留分支和提交，等负责人确认合并或归档后再处理 worktree。

## 标准流程

1. 阅读 `AGENTS.md`、`docs/DEVELOPMENT.md` 和相关领域文档。
2. 确认任务依赖的契约版本，尤其是 `bridge/ffi/ffi_api*.h` 和 UI token。
3. 建立任务卡，声明文件所有权、独占资源和验收命令。
4. 小步实现并运行聚焦测试；不要等全部改完才第一次编译。
5. 运行 `tools/preflight.sh`，修复所有失败后再请求复核。
6. 由另一个 agent 或人在同一 revision 运行任务卡中的验收命令。
7. 提交本地 commit；只有用户明确要求时才推送远端或创建远端资源。

## 独占资源

以下资源同一时刻只能由一个流程持有：

| 资源 | 约束 |
| --- | --- |
| 飞控串口 `/dev/cu.usbmodem*` | 先确认无其他连接；硬件测试串行执行 |
| Flutter 热重载窗口 | 一个工作副本只保留一个 `flutter run` |
| git 索引 | 同一 worktree 不并行执行写索引操作 |
| `build_check/` | 同一 checkout 不并行 configure/build/test |
| 发布版本文件 | 版本改动由单一发布流程完成 |

硬件测试前必须确认飞控未解锁。未经明确确认，不执行解锁、电机测试、危险校准、
任务启动或参数写入。

## 本地预检

```bash
tools/preflight.sh
```

默认执行补丁格式、UI token、Flutter 分析/测试、后端 offline/simulation 门禁，
并在 macOS 上追加 debug 构建。只做快速迭代时可以使用：

```bash
tools/preflight.sh --frontend-only
tools/preflight.sh --backend-only
tools/preflight.sh --no-macos-build
```

这些简化选项不能替代提交前的完整预检。仓库 hook 可通过以下命令启用：

```bash
tools/install-hooks.sh
```

hook 会要求工作区与暂存区一致；存在未暂存或相关未跟踪文件时会拒绝提交，
避免门禁验证的版本与最终提交内容不同。

## 变更边界

### FFI

修改公开契约时，同一次提交必须包含：

1. 头文件声明；
2. `ffi_*.cc` 实现；
3. C++ 测试；
4. `bridge/ffi/API_CATALOG.md`；
5. 受影响的文档和前端调用点。

### UI

- 字号、颜色、关键尺寸分别取 `MgTypography`、`MgColors`、`MgMetrics`。
- 不在业务页面直接使用 `MacosColors`、裸色值或裸 `TextStyle(fontSize:)`。
- 数字读数使用 `MgAnimatedNumber`，并保留等宽数字。
- `component_gallery_demo.dart` 只作为组件画廊，允许展示 `macos_ui` 原始色板。
- `dart tools/check_ui_tokens.dart` 会扫描业务代码中的禁用写法。

### 文档

- 可执行命令写入 `docs/DEVELOPMENT.md`。
- 长期架构决策写入 `docs/ARCHITECTURE.md` 或 ADR。
- 协作流程写入本文；硬约束写入 `AGENTS.md`。
- 同一事实只保留一个权威出处，其他位置使用链接。

## 发布

版本号通过脚本一次更新：

```bash
tools/release.sh 0.1.0
```

脚本更新 `CMakeLists.txt`、`flutter_shell/pubspec.yaml`、`README.md` 和
`CHANGELOG.md`，不会提交、打标签或访问远端。执行后仍需：

1. 补全 CHANGELOG 的行为变化和验证结果；
2. 运行 `tools/preflight.sh`；
3. 本地提交；
4. 本地创建标签；
5. 只有用户明确要求时才推送提交和标签。

## 交接

交接消息至少包含：

```text
当前 branch / commit：
完成的改动：
验证命令与结果：
未验证或失败的项：
未提交改动：
下一步建议：
```

交接时不要只给聊天结论，必须给出可重复运行的命令和明确的工作目录。
