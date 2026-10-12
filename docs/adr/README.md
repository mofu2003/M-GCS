# 架构决策记录（ADR）

本目录包含 M-GCS 项目的架构决策记录（Architecture Decision Records）。

每个 ADR 记录一个重要的架构决策，包括背景、决定、后果和相关信息。

## ADR 列表

| 编号 | 标题 | 状态 | 日期 |
|------|------|------|------|
| [ADR-0001](0001-dependency-direction-and-ffi-boundary.md) | 依赖方向与 FFI 边界 | 已接受 | - |
| [ADR-0002](0002-upstream-reference-and-license.md) | 上游参考与许可证 | 已接受 | - |
| [ADR-0003](0003-mission-file-format-strategy.md) | 任务文件格式策略 | 已接受 | - |
| [ADR-0004](0004-ui-design-system-and-font.md) | UI 设计系统与字体 | 已接受 | - |
| [ADR-0005](0005-deferred-sends-outside-adapter-lock.md) | 延迟发送在适配器锁外 | 已接受 | - |
| [ADR-0006](0006-tlog-recording-sink.md) | TLog 记录接收器 | 已接受 | - |
| [ADR-0007](0007-send-mutex-and-ownership-model.md) | **发送锁与所有权模型** | 已接受 | 2026-10-12 |
| [ADR-0008](0008-video-decode-backend.md) | 视频解码后端 | 已接受 | - |

## 最近变更

### 2026-10-12

- **ADR-0007**：定义 `send_mtx` 边界与锁序，修复 KD-49（死锁）和 KD-52（use-after-free）

## ADR 编写指南

新的 ADR 应包含以下部分：

1. **背景（Context）**：说明需要做出决策的原因和背景
2. **决定（Decision）**：明确说明做出的决策
3. **后果（Consequences）**：说明这个决策的正面和负面影响
4. **状态（Status）**：已提议 / 已接受 / 已废弃 / 已替代
5. **相关（Related）**：相关的 ADR、文档、代码

ADR 一旦被接受就不应修改，只能通过新的 ADR 来替代或废弃。

## 相关文档

- [架构约束](../constraints/architecture.md)：必须遵守的架构规则
- [FFI 契约](../constraints/ffi-contract.md)：FFI 边界的契约
- [已知偏差](../constraints/known-deviations.md)：已知的约束偏差台账
