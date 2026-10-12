# 文档更新总结 - 2026-10-12

## 更新原因

根据项目发展需要，明确 **0.2.x 版本聚焦 ArduPilot 固件支持**，PX4 相关功能延后至 0.3.0+ 版本开发。本次更新将此策略写入文档，并清理过时文档。

## 主要变更

### 1. 新增架构决策记录

**ADR-0008：ArduPilot 优先，PX4 支持延后**
- 文件：`docs/adr/0008-ardupilot-first-px4-deferred.md`
- 内容：记录 ArduPilot 优先策略的背景、决策与后果
- 状态：已接受（2026-10-12）

**ADR 重新编号**
- 原 ADR-0007（send-mutex）→ ADR-0009
- 原 ADR-0008（video）→ ADR-0010
- 新增 ADR-0008（ArduPilot 优先策略）

### 2. 核心文档更新

#### README.md
- 更新项目描述：优先服务 ArduPilot，兼容 PX4 改为"延后支持 PX4"
- 添加开发策略说明，指向 ADR-0008

#### AGENTS.md
- 更新协作入口文档，明确 ArduPilot 优先策略

#### CHANGELOG.md
- 新增 Unreleased 段落，记录文档更新与策略变更
- 记录 Phase 1 质量加固成果（KD-49、KD-52 修复）

### 3. 约束文档更新

#### docs/constraints/known-deviations.md
- KD-53（PX4 陀螺校准参数硬编码）：状态改为"延后修复"，处置方式改为"0.3.0 恢复 PX4 支持时修复"

#### docs/constraints/upstream.md
- 更新"飞控优先级"建议，引用 ADR-0008

#### docs/constraints/parity.md
- 添加开发策略说明，指出当前重点为 ArduPilot
- 新增状态值："延后验证"（用于 PX4 专属功能）

### 4. 计划文档更新

#### docs/plans/mgcs-development-plan-2026-10-10.md
- 添加开发策略更新说明
- 更新产品目标，明确 PX4 支持延后至 0.3.0+

### 5. 文档归档

#### PX4 相关文档
- `docs/features/px4-parity.md` → `docs/archive/px4-parity-deferred.md`
- 添加归档说明，记录归档原因与当前状态

#### 过渡性工作报告
移动至 `docs/archive/reports/`：
- ENGINEERING_QUALITY_AUDIT.md
- FINAL_VERIFICATION_REPORT.md
- HANDOFF-2026-10-12-completion.md
- MERGE_COMPLETE.md
- QUALITY_IMPROVEMENT_PLAN.md
- RELEASE_0.2.2.md
- T14_QUALITY_REPORT.md
- T14_VERIFICATION.md
- T16_COMPLETION.md

### 6. 新增索引文档

#### docs/adr/README.md
- ADR 列表与最近变更
- ADR 编写指南
- 相关文档链接

#### docs/archive/README.md
- 归档文档说明
- 归档原因与日期
- 使用建议

## 代码库状态

### PX4 相关代码保留

以下代码保留在代码库中，保持架构扩展性：
- `core/src/config/firmware_plugin_px4.*`：PX4 固件插件
- `core/src/config/px4_modes.*`：PX4 飞行模式定义
- `core/src/config/PX4ParameterMetaData.json`：PX4 参数元数据
- 相关测试代码

### 验收标准变更

- **0.2.x 版本**：PX4 功能不作为验收依据
- **测试覆盖**：PX4 相关测试保留但不强制通过
- **功能对齐**：`parity.md` 中 PX4 专属条目标注"延后验证"

## 后续工作

### 0.2.x 版本（当前）
- 聚焦 ArduPilot 固件支持
- 完成质量加固（Phase 1）
- 补全 ArduPilot 主流程测试

### 0.3.0+ 版本（计划）
- 恢复 PX4 支持开发
- 修复 KD-53（陀螺校准参数）等 PX4 相关缺陷
- 完整验证 PX4 工作流

## 验证清单

- [x] ADR-0008 已创建
- [x] ADR 重新编号完成（0007 → 0009，0008 → 0010）
- [x] README.md 已更新
- [x] AGENTS.md 已更新
- [x] CHANGELOG.md 已更新
- [x] known-deviations.md 已更新
- [x] upstream.md 已更新
- [x] parity.md 已更新
- [x] 开发计划已更新
- [x] PX4 文档已归档
- [x] 过渡性报告已归档
- [x] ADR 索引已创建
- [x] 归档说明已创建

## 相关文档

- [ADR-0008: ArduPilot 优先，PX4 支持延后](docs/adr/0008-ardupilot-first-px4-deferred.md)
- [ADR 索引](docs/adr/README.md)
- [归档文档说明](docs/archive/README.md)
- [变更日志](CHANGELOG.md)
- [已知偏差台账](docs/constraints/known-deviations.md)

---

**更新完成日期**：2026-10-12  
**执行人**：Claude Code  
**版本**：0.2.2+
