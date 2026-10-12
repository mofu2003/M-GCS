# M-GCS 仓库协作入口

M-GCS 是无人机地面站，对标 Mission Planner 与 QGroundControl。

**当前版本（0.2.2）聚焦 ArduPilot 固件支持**。PX4 支持计划在 0.3.0+ 版本提供。

开始任何改动前，先按任务类型阅读对应细则：

| 任务 | 必读 |
|---|---|
| 修改 `core/` 或 `bridge/ffi/` | `docs/constraints/architecture.md`、`ffi-contract.md`、`safety.md` |
| 修改飞行相关行为 | `docs/constraints/safety.md`、`upstream.md`、`parity.md` |
| 修改 `flutter_shell/` 界面 | `docs/constraints/ui.md`、`ui-pages.md` |
| 新增或修改功能 | `docs/constraints/upstream.md`（P1 开发前阅读参考源码）、`parity.md` |
| 编写或修改测试 | `docs/constraints/testing.md` |
| 发布或修改版本号 | `docs/constraints/release-and-hardware.md` |
| 多 agent 协作（Codex、Claude、DeepSeek、WorkBuddy） | `docs/workflow/COLLABORATION.md` |

## 规则分级

- **约束（MUST）**：违反即不得合并。每条写明违反后果和验证方式。
- **建议（SHOULD）**：默认采用。偏离时在设计说明或提交说明中写明理由。
- **参考（INFO）**：对照事实，不规定行为。`parity.md` 属于这一级。

## 依赖方向

`flutter_shell → bridge/ffi → core → third_party`，单向，不可逆。

## 已知偏差

当前代码与约束不符的条目见 `docs/constraints/known-deviations.md`。新增偏差必须登记，不得悄悄绕过。

## 说明文档

- `README.md`：项目概览
- `docs/development.md`：构建、测试、门禁、发布
- `docs/adr/`：架构决策记录
- `docs/capabilities.md`：FFI 模块交付状态与 UI 覆盖矩阵（阶段 0 基线）
- `docs/plans/`：分阶段开发计划
- `docs/comparisons/`：与 QGroundControl、Mission Planner 的对比报告

## 禁区

- 不修改 `third_party/` 中的第三方源码
- 不提交 `build*/`、`.venv/`、`build_docker/` 等本地生成物，也不提交生成后的 MAVLink 头文件
- 不使用 `git reset --hard`、`git checkout --` 等会丢弃他人改动的命令
- 不强制推送
- 不在 `core/` 的持锁路径中做阻塞 I/O 或调用外部回调
