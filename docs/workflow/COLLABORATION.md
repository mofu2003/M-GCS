# M-GCS 多 Agent 协作指南

本文件记录 DeepSeek、Claude Code、Codex、WorkBuddy 四个 agent 的分工与协作方式。

## 设计原则

1. **按需协作，不强制流程**：大部分工作由 DeepSeek 直接完成，不需要多 agent 串行
2. **主力干活，辅助复核**：DeepSeek 是实现主力，其他 agent 按需出场
3. **Claude 只做关键决策**：架构设计、FFI 契约、复杂功能分解时才叫 Claude
4. **独立 worktree 避免冲突**：多人并行工作时各用一个 worktree，完成后合并

## 四个 Agent 的定位

### DeepSeek - 实现主力

**职责**：
- 修复 `known-deviations.md` 里的已知问题
- 实现小功能和改进
- UI 开发和调整
- 单元测试补充
- 文档更新

**工作方式**：
- 用户直接下命令："修复 KD-01"、"把顶栏高度改成 48"
- 在主检出或 `~/Projects/M-GCS-wt/deepseek` worktree 工作
- 改完必须跑门禁：`ctest --test-dir build_check -L "offline|simulation"` + `cd flutter_shell && flutter test`
- 通过后提交，写清楚的 commit message

**自律规则**：
- 涉及 FFI 契约变更时，停下来问 Claude
- 不确定架构方向时，停下来问 Claude
- 改动超过 5 个文件或跨 3 层时，停下来问用户或 Claude

**worktree 位置**：`~/Projects/M-GCS-wt/deepseek`

---

### Claude Code - 架构与设计

**职责**：
- 架构决策（"Survey 应该怎么建模？"）
- FFI 契约设计（新增 API 的接口定义）
- 复杂功能的设计文档（固件刷写、多机任务分配）
- 争议仲裁（DeepSeek 和 Codex 有分歧时）
- 季度架构文档梳理

**工作方式**：
- 用户明确请求时才出场："Claude，Survey 的架构应该怎么设计？"
- 产出设计文档到 `docs/specs/`
- 如果需要多人协作，写任务分解和验收标准
- 写完文档后退出，由 DeepSeek/Codex 实现

**不做的事**：
- 不写实现代码（除非是演示性的原型）
- 不做大范围代码探索（交给 DeepSeek 或 Explore agent）
- 不参与日常 bug 修复和 UI 调整

---

### Codex - 复核与并行实现

**职责**（取决于能力，二选一）：

**选项 A - 复核者**：
- 按用户要求复核 DeepSeek 的提交
- 在独立 worktree 检出提交，跑门禁，审 diff
- 报告问题或通过

**选项 B - 并行实现者**：
- 和 DeepSeek 并行工作，各自在独立 worktree
- 实现不同的功能模块
- 完成后由用户或第三方合并

**工作方式**：
- 用户明确指派时才工作："Codex，复核提交 abc123"
- 使用 `~/Projects/M-GCS-wt/codex` worktree
- 发现问题时报告，不自己改（避免冲突）

**worktree 位置**：`~/Projects/M-GCS-wt/codex`

---

### WorkBuddy - 杂活

**职责**：
- 四处版本号同步（`CMakeLists.txt`、`pubspec.yaml`、`README.md`）
- CHANGELOG 更新
- `bridge/ffi/API_CATALOG.md` 补全
- Memory 文件维护
- 文档格式整理
- 样板代码生成

**工作方式**：
- 用户直接命令："把 CHANGELOG 补全到 v0.1.1"
- 使用 `~/Projects/M-GCS-wt/workbuddy` worktree 或主检出
- 完成后提交

**不做的事**：
- 不碰 `core/` 和 `bridge/ffi/` 的业务代码
- 不设计 FFI 契约
- 不做需要理解业务逻辑的改动

**worktree 位置**：`~/Projects/M-GCS-wt/workbuddy`

---

## 典型工作场景

### 场景 1：修复已知问题

```
用户：DeepSeek，修复 KD-01，ACK 判定问题
DeepSeek：
  1. 读 known-deviations.md，找到 KD-01 的位置和描述
  2. 读相关代码和测试
  3. 修复实现
  4. 跑 ctest
  5. 提交
```

### 场景 2：UI 调整（连续多次）

```
用户：DeepSeek，把顶栏高度改成 48
DeepSeek：改 → flutter analyze → 提交

用户：标题字号改成 18
DeepSeek：改 → flutter analyze → 提交

用户：左边距加 4
DeepSeek：改 → flutter analyze → 提交
```

不需要其他 agent 参与，DeepSeek 连续完成。

### 场景 3：新增复杂功能

```
用户：Claude，Survey 测绘功能应该怎么设计？
Claude：
  1. 读 QGC SurveyComplexItem 和 MP GridUI
  2. 写设计文档到 docs/specs/survey-design.md
  3. 列出需要的 FFI 接口和验收标准

用户：DeepSeek，按 docs/specs/survey-design.md 实现
DeepSeek：
  1. 读设计文档
  2. 实现 core 层算法
  3. 实现 FFI 接口
  4. 实现 Flutter UI
  5. 写测试
  6. 跑门禁
  7. 提交

（如果改动太大，用户可以让 DeepSeek 和 Codex 各做一半）
```

### 场景 4：并行开发

```
用户：DeepSeek，你实现 Survey 的核心算法
用户：Codex，你实现 Survey 的 Flutter UI

（两人各自在 worktree 工作，不冲突）

DeepSeek：提交到 feature/survey-core 分支
Codex：提交到 feature/survey-ui 分支

用户：合并两个分支到 main
```

### 场景 5：复核

```
用户：DeepSeek 刚提交了 abc123，Codex 你复核一下
Codex：
  1. 检出提交 abc123
  2. 跑门禁
  3. 审 diff
  4. 报告：通过 / 有问题（列出问题）
```

### 场景 6：杂活

```
用户：WorkBuddy，把版本号改成 0.1.2，更新 CHANGELOG
WorkBuddy：
  1. 改 CMakeLists.txt
  2. 改 pubspec.yaml
  3. 改 README.md
  4. 更新 CHANGELOG.md
  5. 提交
```

---

## Worktree 管理

### 初始化

三个 worktree 已建好：
- `~/Projects/M-GCS-wt/deepseek`
- `~/Projects/M-GCS-wt/codex`
- `~/Projects/M-GCS-wt/workbuddy`

每个 worktree 需要独立的构建环境：

```bash
cd ~/Projects/M-GCS-wt/deepseek
./tools/generate-mavlink.sh
cmake -S . -B build_check -DMGCORE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_check --config Release
```

### 同步到最新

```bash
# 在主检出
git pull

# 更新三个 worktree
cd ~/Projects/M-GCS-wt/deepseek && git reset --hard main
cd ~/Projects/M-GCS-wt/codex && git reset --hard main
cd ~/Projects/M-GCS-wt/workbuddy && git reset --hard main
```

### 合并提交

DeepSeek 在 worktree 提交后，用户在主检出合并：

```bash
cd ~/Projects/M-GCS
git cherry-pick <commit-hash>
# 或
git merge --ff-only feature/xxx
```

---

## 门禁要求

所有 agent 提交前必须跑门禁（AGENTS.md 铁律二）：

```bash
# 后端门禁（约 35 秒）
ctest --test-dir build_check --output-on-failure --no-tests=error -L "offline|simulation" -LE "hardware|manual"

# 前端门禁
cd flutter_shell && flutter analyze lib test && flutter test
```

改动涉及界面时追加：
```bash
cd flutter_shell && flutter build macos --debug
```

---

## FFI 契约变更流程

修改 `bridge/ffi/ffi_api*.h` 时，必须同一次提交包含（AGENTS.md 铁律三）：

1. 头文件声明
2. 实现（`ffi_*.cc`）
3. 测试（`core/tests/`）
4. `bridge/ffi/API_CATALOG.md` 更新
5. 受影响的文档

**流程**：
1. DeepSeek 发现需要改 FFI 契约时，停下来
2. 用户问 Claude："这个 API 应该怎么设计？"
3. Claude 给出接口定义
4. DeepSeek 按定义完成上述 5 项
5. 一次提交

---

## 争议仲裁

DeepSeek 和 Codex 对实现方案有分歧时：

1. 各自写出方案和理由
2. 用户问 Claude："方案 A 和方案 B，选哪个？"
3. Claude 给出选择和理由
4. 采用选中的方案

---

## 文档管理

### 设计文档

复杂功能的设计文档存放在 `docs/specs/`：
- `survey-design.md`：Survey 测绘功能设计
- `firmware-flash-ui.md`：固件刷写 UI 设计
- `multi-vehicle-task-allocation.md`：多机任务分配设计

由 Claude 编写，DeepSeek/Codex 实现时参考。

### 已知问题

`docs/constraints/known-deviations.md` 记录所有待修复的偏差。

DeepSeek 修复时：
1. 修复代码
2. 在 `known-deviations.md` 删除对应条目
3. 在 commit message 引用编号（例如 `fix: resolve KD-01 ACK matching`）

### Memory

项目的长期记忆在 `.workbuddy/memory/`，由 WorkBuddy 维护。

记录架构决策、约定、构建命令、文档索引等稳定结论。

---

## 成本控制

### Claude 的使用

只在这些时刻叫 Claude：
- 需要架构决策
- 需要设计 FFI 契约
- 需要复杂功能分解
- 需要仲裁争议
- 季度文档梳理

**不要**在这些时刻叫 Claude：
- 日常 bug 修复
- UI 调整
- 文档更新
- 已知问题修复
- 小功能添加

### 并行工作

DeepSeek 和 Codex 可以并行工作，各自在独立 worktree：
- 减少等待时间
- 避免互相阻塞
- 用户最后合并

但不要过度并行：
- 涉及同一模块时串行更好
- FFI 契约变更必须由一个人完成

---

## 推送策略

遵守 AGENTS.md 铁律一：**默认纯本地工作，不碰远端**。

- Agent 不主动执行 `git fetch`、`git pull`、`git push`
- 用户要求推送时，一次做完（提交 + 标签 + 推送）
- 推送前确认网络通畅，失败不盲目重试

---

## 总结

- **DeepSeek 是主力**，大部分工作它一个人搞定
- **Claude 按需出场**，只做关键决策和设计
- **Codex 按需复核或并行**，不强制每次都用
- **WorkBuddy 干杂活**，维护文档和版本号
- **不走看板流程**，直接命令式协作
- **保留 worktree**，但不强制使用
- **门禁必跑**，提交前必须通过测试
