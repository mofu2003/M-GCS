# 归档文档说明

本目录存放已归档的文档，包括：

## 1. PX4 相关文档（`px4-parity-deferred.md`）

**归档日期**：2026-10-12

**归档原因**：根据 ADR-0008（ArduPilot 优先，PX4 支持延后），0.2.x 版本聚焦 ArduPilot 固件支持，PX4 相关功能延后至 0.3.0+ 开发。

**当前状态**：
- PX4 相关代码（插件、参数元数据、模式映射）保留在代码库中，保持架构扩展性
- PX4 功能已实现但未经完整验证，存在已知缺陷（如 KD-53 陀螺校准参数硬编码）
- 文档内容仍可作为将来恢复 PX4 支持的参考

**相关 ADR**：[ADR-0008](../adr/0008-ardupilot-first-px4-deferred.md)

## 2. 过渡性工作报告（`reports/`）

**归档日期**：2026-10-12

**归档原因**：阶段性工作完成后的过渡文档，内容已整合到正式文档（CHANGELOG、known-deviations、ADR 等）。

**归档内容**：
- `ENGINEERING_QUALITY_AUDIT.md`：工程质量审计报告
- `FINAL_VERIFICATION_REPORT.md`：最终验证报告
- `HANDOFF-2026-10-12-completion.md`：交接文档
- `MERGE_COMPLETE.md`：合并完成报告
- `QUALITY_IMPROVEMENT_PLAN.md`：质量改进计划
- `RELEASE_0.2.2.md`：0.2.2 版本发布说明
- `T14_QUALITY_REPORT.md`：T14 质量报告
- `T14_VERIFICATION.md`：T14 验证报告
- `T16_COMPLETION.md`：T16 完成报告

**信息去向**：
- 架构风险修复（KD-49、KD-52）→ `CHANGELOG.md`、`docs/adr/0009-send-mutex-and-ownership-model.md`
- 已知偏差更新 → `docs/constraints/known-deviations.md`
- 测试覆盖与质量指标 → 代码库本身（测试文件、注释、ADR）

## 使用建议

1. **归档文档仅供参考**，不作为当前开发依据
2. **查找信息时**，优先查阅活跃文档（`docs/constraints/`、`docs/adr/`、`CHANGELOG.md`）
3. **恢复已归档功能时**（如 0.3.0 恢复 PX4 支持），参考归档文档了解历史决策和实现细节
4. **不要修改归档文档**，除非添加归档说明或修正事实性错误

## 相关文档

- [架构决策记录（ADR）](../adr/README.md)
- [已知偏差台账](../constraints/known-deviations.md)
- [变更日志](../../CHANGELOG.md)
- [开发计划](../plans/)
