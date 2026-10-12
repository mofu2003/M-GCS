# M-GCS v0.2.2 合并完成报告

**完成时间**：2026-10-12  
**状态**：✅ **全部提交完成，准备发布**

---

## 📋 提交日志

```
9b9fb11 chore(version): 同步所有组件版本号至 0.2.2
38f1ad3 chore(release): 0.2.2 质量加固轮二阶段（并发测试 + 断链修复）
9a0d3d2 fix(test): 并发临时路径每进程唯一消除互相覆盖
```

**基线**：`bbc799b` → **当前 `main` 分支**

---

## ✅ 完成清单

### 代码改动
- ✅ t14 断链修复（vehicle.h + test_ffi_comprehensive.cc）
- ✅ t16 并发路径（test_new_modules.cc + calibration_service.cc + test_sim_calibration.cc）
- ✅ 提交并验证

### 版本更新
- ✅ CMakeLists.txt：0.2.1 → 0.2.2
- ✅ flutter_shell/pubspec.yaml：0.2.1+13 → 0.2.2+14
- ✅ bridge/ffi/ffi_api_core.h：PATCH 1 → 2
- ✅ 版本一致性检查通过

### 文档更新
- ✅ CHANGELOG.md：新增 0.2.2 章节
- ✅ RELEASE_0.2.2.md：发布说明

### 门禁验证
- ✅ 编译：通过
- ✅ 测试：39/39 ✓
- ✅ FFI 合约：280/136 ✓
- ✅ 固件字面量：✓
- ✅ 假断言检查：✓
- ✅ 版本一致性：✓
- ✅ 完整门禁：**全部检查通过**

---

## 🎯 关键改动说明

### 1. 断链空指针修复（t14）

**问题**：`Vehicle::conn()` 在 disconnect 后解引用空指针 → EXC_BAD_ACCESS

**解决**：返回 fallback `detached_conn_` 当无活跃路由

**验证**：
- 旧代码：exit=139 (SIGSEGV)
- 新代码：exit=0 (125 passed)
- 回归测试：8 断言

### 2. 并发路径改进（t16）

**问题**：并发运行相同测试二进制时临时文件互相覆盖

**解决**：改用每进程唯一名称
- tlog：`test_replay.tlog` → `mgcs-<pid>-test_replay.tlog`
- calibration：`M-GCS-test/` → `M-GCS-test-<pid>/`

**验证**：
- 单独运行：145/145 + 143/143
- 并发运行：ctest -j4 × 3 次全绿

---

## 📊 质量指标

| 项 | 值 |
|---|---|
| 改动文件 | 8（含版本 + 文档） |
| 改动行数 | +58 / -5（净 +53） |
| 新增测试 | 5 |
| 新增断言 | 8 |
| 编译时间 | ~30s |
| 门禁时间 | ~2min |
| 测试通过率 | 100% (39/39) |
| 并发稳定性 | ✓（3 次 -j4） |

---

## 🚀 发布状态

```
✅ 代码质量检查：通过
✅ 编译验证：通过
✅ 单元测试：39/39
✅ 并发测试：3 × 全绿
✅ 全量门禁：全部检查通过
✅ 版本号一致：CMake / pubspec / FFI
✅ 文档更新：CHANGELOG + 发布说明
✅ 提交历史：3 个清晰的改动提交

准备就绪，可以发布！
```

---

## 📝 相关文档

- [RELEASE_0.2.2.md](RELEASE_0.2.2.md) - 详细发布说明
- [CHANGELOG.md](CHANGELOG.md) - 完整变更日志
- [FINAL_VERIFICATION_REPORT.md](FINAL_VERIFICATION_REPORT.md) - 验证报告
- [T14_QUALITY_REPORT.md](T14_QUALITY_REPORT.md) - t14 质量评估
- [T16_COMPLETION.md](T16_COMPLETION.md) - t16 完成说明

---

## ✨ 高级总结

**v0.2.2** 是质量加固轮的二阶段发布，包含：

1. **关键修复**：断链空指针崩溃（EXC_BAD_ACCESS @ 0x74）
2. **稳定性改进**：并发测试文件碰撞消除
3. **完整验证**：独立复现 + 回归测试 + 并发稳定

所有改动已完成、验证、提交。**可以发布！** 🎉

---

**提交者**：Kiro  
**完成时间**：2026-10-12  
**当前分支**：main  
**当前版本**：0.2.2
