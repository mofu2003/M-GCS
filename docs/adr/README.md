# 架构决策记录（ADR）

> 历史架构记录：本文档记录特定时间点的长期决策。普通改动不需要逐项阅读或同步更新；当前协作规则以根目录 `AGENTS.md` 为准。

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
| [ADR-0007](0007-send-mutex-and-ownership-model.md) | **发送锁与所有权模型** | 草案 | 2026-10-12 |
| [ADR-0008](0008-video-decode-backend.md) | 视频解码后端 | 已接受 | - |
| [ADR-0009](0009-vehicle-session-assembler.md) | 车辆会话装配器 | 已接受 | 2026-10-13 |
| [ADR-0010](0010-vehicle-identity-vs-capability.md) | 机型身份与能力分层 | 已接受 | 2026-10-13 |
| [ADR-0011](0011-firmware-parameter-ownership.md) | 固件参数名持有机制 | 已接受 | 2026-10-13 |

## 最近变更

### 2026-10-13

- **ADR-0011**：明确固件参数名的两条正式持有渠道（角色表 + 插件方法），
  解决「决定 profile 本身的参数」因循环依赖无法走角色表的问题（S6）
- **ADR-0010**：机型身份与能力分层，能力按运行时求值，消除前后端重复判定
- **ADR-0009**：引入车辆会话装配器管理无状态 FFI 后端

### 2026-10-12

- **ADR-0007**：定义 `send_mtx` 边界与锁序，修复 KD-49（死锁）和 KD-52（use-after-free）

## 什么时候新增 ADR

只有真正改变长期架构、线程模型、公开 ABI、许可边界或飞行安全语义时，才新增 ADR。普通 bug 修复、局部重构和一般功能改动不需要新增 ADR。

## 历史编写格式

新的 ADR 应包含以下部分：

1. **背景（Context）**：说明需要做出决策的原因和背景
2. **决定（Decision）**：明确说明做出的决策
3. **后果（Consequences）**：说明这个决策的正面和负面影响
4. **状态（Status）**：已提议 / 已接受 / 已废弃 / 已替代
5. **相关（Related）**：相关的 ADR、文档、代码

ADR 一旦被接受就不应修改，只能通过新的 ADR 来替代或废弃。

## 相关文档

- [历史约束资料](../constraints/README.md)：过去版本的约束和审计材料
- [当前协作入口](../../AGENTS.md)：现在仍然有效的硬约束
