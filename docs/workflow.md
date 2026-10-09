# 多 agent 协作流程

本文件规定四个 agent 与维护者如何分工、交接和验收。除标注 [SHOULD] 的条目外，本文件的规则都是约束。

## 开场

维护者对 agent 说："你是 <角色>，请阅读项目文档，认领你的任务。" agent 按以下顺序执行：

1. 阅读 `AGENTS.md` 和本文件。
2. 阅读 `/Users/fansicheng/Projects/M-GCS/tasks/BOARD.md`。
3. 按"认领规则"找一个轮到自己的任务。没有可认领的任务时，报告"无可认领任务"并停止。

## 角色

| 角色 | 工作目录 | 负责的状态 |
|---|---|---|
| Codex | `~/Projects/M-GCS-wt/codex` | 待调研、待验收 |
| Claude | `~/Projects/M-GCS`（只写 `tasks/`） | 待写卡、待仲裁、待终审 |
| DeepSeek | `~/Projects/M-GCS-wt/deepseek` | 实现者为 DeepSeek 的待实现、退回 |
| WorkBuddy | `~/Projects/M-GCS-wt/workbuddy` | 实现者为 WorkBuddy 的待实现、退回 |
| 维护者 | `~/Projects/M-GCS` | 新建任务、待合并、待硬件验证 |

- Claude 不写实现代码，也不做大范围代码探索。它只读调研结论、相关 KD 条目、审查结论和必要的 diff。
- WorkBuddy 只接卡片明确派给它的低风险任务，不碰 `core/` 和 `bridge/ffi/`。
- 维护者负责在各个应用之间传话、合并到 main、操作硬件。

## 共享状态

- `tasks/` 以主检出中的 `/Users/fansicheng/Projects/M-GCS/tasks/` 为准。所有 agent 都用这个绝对路径读写任务文件。worktree 里的 `tasks/` 副本一律不读不改。
- agent 不提交 `tasks/`。维护者在交接或任务结束时提交，以保留历史。
- 代码只在各自 worktree 的任务分支上修改。任何 agent 都不得在主检出里改代码。

## 状态流转

| 当前状态 | 负责 | 产出 | 下一状态 |
|---|---|---|---|
| 待调研 | Codex | `research.md` | 待写卡 |
| 待写卡 | Claude | `card.md`、`accept.sh` | 待实现 |
| 待实现 / 退回 | 卡片指定的实现者 | 任务分支提交、`claim.md` | 待验收 |
| 待验收 | Codex | `review.md` | 通过：高风险转待终审，其余转待合并；不通过：退回，轮次加一 |
| 待仲裁 | Claude | 在 `card.md` 末尾写仲裁结论 | 待实现（改卡后）、拆分或关闭 |
| 待终审 | Claude | 在 `review.md` 末尾写终审结论 | 待合并或退回 |
| 待合并 | 维护者 | 合并任务分支 | 需要硬件时转待硬件验证，否则转已完成 |
| 待硬件验证 | 维护者 | 把实机结果记入 `review.md` | 已完成或退回 |

- 轮次达到 2 之后，如果再次不通过，转为待仲裁，不再退回。
- 低风险任务（只改文档或表格）可以由维护者直接建为待写卡，跳过调研。
- 状态只能按本表转换，不得跳过待验收。

## 认领规则

1. 只认领状态由自己负责、且"认领"列为空的任务。实现类任务还要求卡片中的实现者是自己。
2. 一次只认领一个任务，按 ID 从小到大选择。
3. 开工前，在 `BOARD.md` 该行的"认领"列写入自己的角色和时间。编辑前重新读取文件，只改自己负责的那一行。
4. 完成后，更新状态和更新时间，清空"认领"列，然后停止，并向维护者报告。
5. 发现卡片有错、范围不清或无法完成时，不要自行扩大范围。在 `card.md` 末尾写明问题，把状态改为待仲裁。

## 风险分级

风险等级由 Claude 写卡时判定，实现者不得下调。

- **高**：涉及 `safety.md` 的 S1 至 S8、`architecture.md` 的 A1 至 A4、`ffi-contract.md` 的 F1 至 F4，或者需要硬件。必须经过待终审。
- **中**：其他代码改动。
- **低**：只改文档或表格。

## 各阶段要求

### 调研（Codex）

- 按 `upstream.md` 的 P1，阅读 QGC 与 MP 中对应的模块。本地路径与版本见 `parity.md`。
- `research.md` 不超过一页，写明参考路径、两者的做法、建议采用的做法及理由、相关 KD 编号。只记录行为和位置，不复制源码。

### 写卡（Claude）

- 输入只用 `research.md`、相关 KD 条目和约束文件。
- `card.md` 不超过一页。必须写明目标、实现者、风险等级、允许修改的文件、不在范围内的内容、完成标准（包括要删除的 KD 编号和要更新的 `parity.md` 条目），以及基线提交。
- `accept.sh` 每个验收项一条命令。写完后运行 `shasum -a 256 accept.sh`，把结果填进 `card.md`。之后任何人不得修改 `accept.sh`。确需修改时，先转为待仲裁，由 Claude 修改并更新哈希。

### 实现（DeepSeek / WorkBuddy）

- 在自己的 worktree 中，从 main 新建分支：`git switch -c task/<ID> main`。
- 只改"允许修改的文件"。需要改其他文件时，转为待仲裁。
- 在 worktree 根目录运行 `bash /Users/fansicheng/Projects/M-GCS/tasks/<ID>/accept.sh`，把完整输出贴进 `claim.md`。
- `claim.md` 必须如实写明未完成的部分和已知问题。提交后，把提交哈希写进 `claim.md` 和 `BOARD.md` 的"实现提交"列。

### 验收（Codex）

- 写完 `review.md` 的逐项结论之前，不得打开 `claim.md`。实现提交的哈希从 `BOARD.md` 获取。
- 在自己的 worktree 检出该提交：`git switch --detach <提交>`。
- 核对 `accept.sh` 的哈希与 `card.md` 中记录的是否一致。
- 重跑 `accept.sh`，逐项记录命令、退出码和结论。笼统的"通过"无效。
- 检查卡外事项：用 `git diff --stat <基线>..<提交>` 确认只改了允许的文件，确认 KD 条目已删除、`parity.md` 已更新。
- 默认任务未完成，直到命令证明它已完成。
- 写完结论后，再对照 `claim.md`，记录声明与实际是否有出入。

## 防止虚报

- 只有 Codex 能把任务转为通过。实现者只能把任务转到待验收。
- 验收标准在实现之前冻结，并用哈希核对。
- 先审查，再看实现者的声明。
- `review.md` 的"声明与实际不符"一栏填"有"时，维护者记一次虚报。
- [SHOULD] 每周抽查 2 到 3 个已完成的任务，重跑 `accept.sh`。

## 一次性准备（维护者）

先提交 `AGENTS.md`、`CLAUDE.md` 和 `docs/`，否则 worktree 里读不到这些文档。然后创建三个 worktree：

```bash
mkdir -p ~/Projects/M-GCS-wt
git worktree add --detach ~/Projects/M-GCS-wt/deepseek main
git worktree add --detach ~/Projects/M-GCS-wt/codex main
git worktree add --detach ~/Projects/M-GCS-wt/workbuddy main
```

每个 worktree 首次构建前，都要单独生成 MAVLink 头文件并初始化 `build_check`，步骤见 `docs/development.md`。
