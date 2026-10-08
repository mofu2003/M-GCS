# M-GCS 开发指南

当前版本：`0.0.78`

> **完整规则请参阅** [`AGENTS.md`](../../AGENTS.md)（仓库根目录）
> 
> 本文件是人类友好的快速参考，完整的架构约束、工作流程和详细说明都在 `AGENTS.md` 中。

## 快速开始

### 初次构建

```bash
# 1. 生成 MAVLink 头文件
./tools/generate-mavlink.sh

# 2. 初始化构建目录
cmake -S . -B build_check -DMGCORE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_check --config Release

# 3. 运行前端
cd flutter_shell && flutter run -d macos
```

### 提交前门禁（必须）

```bash
# 后端测试（35 个 offline/simulation 测试，约 35 秒）
ctest --test-dir build_check --output-on-failure --no-tests=error -L "offline|simulation" -LE "hardware|manual"

# 前端测试
cd flutter_shell && flutter analyze lib test && flutter test

# 界面改动时追加
cd flutter_shell && flutter build macos --debug
```

**门禁失败时必须修复后重跑，不能跳过。**

## 五大铁律

### 1. 默认纯本地工作
- 不执行 `git fetch`、`git pull`、`git push`（除非用户明确要求）
- 不在门禁脚本中包含网络调用
- 原因：GitHub 连接不稳定（HTTP2 framing error、75 秒超时）

### 2. 提交前必须跑本地门禁
- 改动 `core/`、`bridge/ffi/` 时不能用"看起来没问题"代替测试

### 3. FFI 契约变更必须成套
修改 `bridge/ffi/ffi_api*.h` 时，同一次提交必须包含：
1. 头文件声明
2. 实现（`ffi_*.cc`）
3. 测试（`core/tests/`）
4. `bridge/ffi/API_CATALOG.md`
5. 受影响的文档

### 4. 界面设计系统统一
- 字号：只用 `MgTypography`
- 颜色：只用 `MgColors`
- 间距：只用 `MgMetrics`
- 字体：MiSans 全平台统一，以 macOS 为基准
- **只有标题加粗**，正文一律 `FontWeight.w400`

### 5. 硬件是排他资源
- 飞控串口同一时刻只能被一个进程占用
- 硬件测试串行执行，不并行

## 架构

### 依赖方向（单向，不可逆）

```text
flutter_shell/  →  bridge/ffi/  →  core/  →  third_party/
```

- `core/` 不得依赖 `bridge/` 或 `flutter_shell/`
- `bridge/ffi/` 是唯一稳定边界
- 循环依赖会被编译器拒绝

### 上游实现策略

**混合策略，不站队**：
- ArduPilot 语义 → 参考 Mission Planner
- 通用框架 → 参考 QGroundControl
- 协议定义 → 使用官方 MAVLink

详细比较见 [`docs/../specs/upstream/SELECTION_STRATEGY.md`](../specs/upstream/SELECTION_STRATEGY.md)

## 常用命令

### 构建

```bash
# 后端共享库
cmake -S . -B build -DMGCORE_BUILD_SHARED=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target mgcore

# Docker 全量检查
./tools/docker-dev.sh check
```

### 发布

```bash
# 使用发布脚本（自动更新版本号、CHANGELOG）
./tools/release.sh 0.0.78

# 发布流程：改版本号 → 更新 CHANGELOG → 提交 → 打标签 →（用户要求时）推送
```

## 提交规范

```bash
# 格式
<type>: <summary>

# type 可选值
feat     # 新功能
fix      # Bug 修复
docs     # 文档更新
chore    # 杂项（构建、工具配置）
release  # 版本发布
```

提交前运行 `git diff --check` 检查空白字符问题。

## 禁区

- ❌ 不提交 `build*/`、`.venv/`、生成的 MAVLink 头
- ❌ 不修改 `third_party/` 中的第三方源码
- ❌ 不使用 `git reset --hard`、`git checkout --` 丢弃他人改动
- ❌ 不 force push
- ❌ 不在 `core/` 持锁路径中做阻塞 I/O

## 多 Agent 协作

- 每个 agent 使用独立 worktree 与分支
- 任务卡写清：涉及文件、依赖契约、验收命令、完成标准
- 独占资源：飞控串口、Flutter 热重载窗口、git 索引

详细流程见 [`docs/COLLABORATION.md`](COLLABORATION.md)

## 文档导航

### 指南
- [开发指南](DEVELOPMENT.md)（本文件）
- [协作流程](COLLABORATION.md)
- [故障排除](TROUBLESHOOTING.md)

### 架构
- [架构总览](../architecture/ARCHITECTURE.md)
- [目录结构](../architecture/PROJECT_LAYOUT.md)
- [ADR（架构决策记录）](../architecture/adr/)

### 规划
- [路线图](../planning/DEVELOPMENT_ROADMAP.md)
- [完成定义](../planning/DEFINITION_OF_DONE.md)
- [当前审计](../planning/STATUS_AUDIT.md)

### 技术规范
- [任务规划契约](../specs/MISSION_PLANNING.md)
- [上游选择策略](../specs/upstream/SELECTION_STRATEGY.md)

## 参考资料

- **完整规则**：[`AGENTS.md`](../../AGENTS.md)（仓库根目录）
- **项目记忆**：`.workbuddy/memory/MEMORY.md`
- **API 清单**：[`bridge/ffi/API_CATALOG.md`](../../bridge/ffi/API_CATALOG.md)
