# 快速参考 — 架构速查表

**用途**：遇到问题时快速查阅。不替代详细文档，但能在 30 秒内给出答案。

---

## 我要增添一个新功能，应该放在哪里？

```
是 FFI 后端（与 C++ 交互）?
├─ 是 → 需要 ConnectionController 状态吗?
│  ├─ 否 → ✅ VehicleSessionAssembler
│  └─ 是 → ✅ ConnectionController (直接)
└─ 否 → 是业务逻辑工作流吗?
   ├─ 是 → ✅ 新建 Controller (参考 LogController)
   └─ 否 → 看场景，通常是 UI 或工具类
```

---

## 我的代码违反了约束吗？

### VehicleSessionAssembler 中不允许的

- ❌ 有状态回调（beforeCommand、onStateChange 等）
- ❌ 访问 ConnectionController 的私有方法
- ❌ UI 操作（addListener、notifyListeners）
- ❌ 业务逻辑判断（if sessionId == X）
- ❌ 有循环依赖（A 依赖 B，B 也依赖 A）

### ConnectionController 中应该避免的

- ⚠️ 超过 5000 行（现 3767，警告线 4500）
- ⚠️ 新增 > 200 行的工作流（应该拆分出 Controller）
- ⚠️ 后端之间的直接依赖（应该通过 CC 调度）

---

## 常见问题

### Q: 新后端需要 ConnectionController 的 fleet，应该怎么办？
**A**: 两个方案：
- 方案 1：后端构造时接收 fleet，但这样是有状态 → 放在 ConnectionController
- 方案 2：后端无状态，需要时通过回调访问 → 装配器中添加 callback 参数

当不确定时，**留在 ConnectionController** 更安全。

### Q: 我发现两个模块代码重复，应该合并吗？
**A**: 取决于它们的生命周期：
- 同生命周期 → 合并
- 不同生命周期 → 保持独立（不要强行合并造成耦合）

例：Telemetry 和 Parameter 都是无状态后端 ✅ 可以共享工具函数
但 LogController 和 MissionDocumentController 生命周期不同 ❌ 不要合并

### Q: 我想用工厂模式支持多固件，现在就要做吗？
**A**: **不要**。1.0 保持简洁。
- 现在：用 if (firmware == .px4) + 记录在 known-deviations.md
- 2.0：集中重构为工厂模式

提前设计 = 假设的复杂性，实际需求会不同。

### Q: 如何做好单元测试？
**A**: 三个要点：
1. 构造参数接受 override（用于 mock）
2. 不依赖全局状态
3. 公开方法最少化

```dart
// ✅ 好的设计
class MyController {
  MyController(
    this._backend,
    { this._backendOverride }  // 测试用
  );
  final MyBackend _backend;
  final MyBackend? _backendOverride;
}

// ❌ 不好的设计
class MyController {
  late MyBackend _backend;  // 无法注入
  // ...
}
```

---

## 文件导航

| 我想... | 看这份文档 |
|--------|----------|
| 理解为什么这样设计 | `docs/adr/0009-vehicle-session-assembler.md` |
| 检查我的代码是否违反约束 | `docs/constraints/vehicle-session-assembler.md` |
| 学习如何添加新功能 | `docs/development/adding-features.md` |
| 了解长期维护计划 | `docs/development/maintenance.md` |
| 查看已知的技术债 | `docs/constraints/known-deviations.md` |
| 看完整的重构报告 | `docs/reports/2026-10-13-assembler-refactoring-report.md` |

---

## 一句话总结

**无状态用装配器，有状态用控制器，无法决定时问：这个东西需要跨车(多会话)共享吗？**

- 需要 → 放在装配器或 ConnectionController
- 不需要 → 放在单车的 Controller

---

## 检查清单（写代码前）

```
□ 读了 adding-features.md 的对应场景？
□ 这个东西是无状态还是有状态？
□ 有测试注入吗？
□ 没有循环依赖？
□ flutter analyze 无警告？
□ 单元测试通过？

全是 ✓ → 可以提交
有 ✗ → 回到开发
```

---

## 快速命令

```bash
# 检查代码质量
flutter analyze              # 需要 0 issues
flutter test                 # 需要全通过

# 找代码
grep -r "if.*firmware"      # 检查固件判断硬编码
grep -r "TODO"              # 检查待办项

# 查看行数
wc -l lib/connection/*.dart | sort -n

# 检查测试覆盖
flutter test --coverage
```

---

## 红线（不能越过）

🔴 **Hard Rules**：

- 装配器中不能有业务逻辑判断
- FFI 后端之间不能有依赖
- ConnectionController 不能直接依赖 UI 层的 widgets
- 不能在 core/ 的持锁路径中做阻塞 I/O

❌ 如果代码违反红线 → PR 无法合并

🟡 **Soft Rules**：

- 优先用装配器，有状态才留在 CC
- 超过 4500 行要考虑拆分
- 新的 if (条件) 要记录原因

⚠️ 如果代码违反软线 → PR 需要讨论

---

**最后提示**：不确定时问维护者，不要猜。猜错的代码成本很高。
